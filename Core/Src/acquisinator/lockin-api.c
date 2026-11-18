/*!
 * \file lockin-api.c
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Implementation of the lock-in measurement.
 */

#include "lockin-api.h"
#include "can-acquisinator-api.h"
#include "can-communications-api.h"
#include "eagletrt.h"

#include <math.h>
#include <string.h>

#define Q15_ONE (32767)
#define QUARTER_PERIOD (LOCKIN_SAMPLES_PER_PERIOD / 4U)

EAGLETRT_STATIC struct LockinHandler lockin_handler;

static void reset_window(void) {
    lockin_handler.count = 0U;
    lockin_handler.acc_i = 0;
    lockin_handler.acc_q = 0;
    lockin_handler.acc_dc = 0U;
    lockin_handler.min = UINT16_MAX;
    lockin_handler.max = 0U;
}

uint16_t lockin_api_init(void) {
    memset(&lockin_handler, 0, sizeof(lockin_handler));
    for (uint32_t k = 0U; k < LOCKIN_SAMPLES_PER_PERIOD; ++k) {
        const float sine = sinf(2.0F * (float)M_PI * (float)k / (float)LOCKIN_SAMPLES_PER_PERIOD);
        const float volts = LOCKIN_EXC_DC_V + LOCKIN_EXC_AMP_V * sine;
        lockin_handler.sin_q15[k] = (int16_t)lroundf((float)Q15_ONE * sine);
        lockin_handler.dac_table[k] = (uint16_t)lroundf(volts / LOCKIN_DAC_VREF_V * (float)LOCKIN_DAC_MAX_CODE);
    }
    reset_window();
    return lockin_handler.dac_table[0];
}

static void publish_result(void) {
    // Counted even when dropped: the gap in the sequence shows the loss
    const uint8_t sequence = lockin_handler.sequence++;
    // The main loop has not sent the previous result yet: drop this one
    if (lockin_handler.ready) {
        return;
    }
    // sum(x*sin) over whole periods = A*cos(phi) * N/2 (times the Q15 scale)
    const float scale = 2.0F / ((float)LOCKIN_WINDOW_SAMPLES * (float)Q15_ONE);
    lockin_handler.i = (float)lockin_handler.acc_i * scale;
    lockin_handler.q = (float)lockin_handler.acc_q * scale;
    lockin_handler.dc = (uint16_t)(lockin_handler.acc_dc / LOCKIN_WINDOW_SAMPLES);
    lockin_handler.result_min = lockin_handler.min;
    lockin_handler.result_max = lockin_handler.max;
    lockin_handler.result_spi_error = lockin_handler.spi_error;
    lockin_handler.result_sequence = sequence;
    lockin_handler.spi_error = false;
    lockin_handler.ready = true;
}

uint16_t lockin_api_on_sample(bool sample_ok, uint16_t raw) {
    // The amplifier settled on the point written at the previous tick
    const uint32_t k = lockin_handler.index;
    lockin_handler.index = (k + 1U) % LOCKIN_SAMPLES_PER_PERIOD;

    if (!sample_ok) {
        // A missing sample breaks the whole periods: start the window again
        lockin_handler.spi_error = true;
        reset_window();
    } else {
        lockin_handler.acc_i += (int32_t)raw * lockin_handler.sin_q15[k];
        lockin_handler.acc_q += (int32_t)raw * lockin_handler.sin_q15[(k + QUARTER_PERIOD) % LOCKIN_SAMPLES_PER_PERIOD];
        lockin_handler.acc_dc += raw;
        lockin_handler.min = raw < lockin_handler.min ? raw : lockin_handler.min;
        lockin_handler.max = raw > lockin_handler.max ? raw : lockin_handler.max;
        if (++lockin_handler.count == LOCKIN_WINDOW_SAMPLES) {
            publish_result();
            reset_window();
        }
    }
    return lockin_handler.dac_table[lockin_handler.index];
}

static enum LockinReturnCode send_frame(enum CanAcquisinatorMessageFrameId id, uint8_t length, union CanAcquisinatorMessage *message) {
    struct CanCommunicationFrame frame = { 0 };
    if (can_acquisinator_api_serialize_from_id(id, message, frame.data) < 0) {
        return LOCKIN_RC_ERROR;
    }
    frame.id = (uint32_t)id;
    frame.length = length;
    if (can_communications_api_add_to_tx_buffer(&frame) != CAN_COMMUNICATION_RC_OK) {
        return LOCKIN_RC_ERROR;
    }
    return LOCKIN_RC_OK;
}

enum LockinReturnCode lockin_api_send_result(void) {
    if (!lockin_handler.ready) {
        return LOCKIN_RC_OK;
    }

    union CanAcquisinatorMessage lockin = { 0 };
    lockin.acquisinator_lockin.lockin_inphase = lockin_handler.i;
    lockin.acquisinator_lockin.lockin_quadrature = lockin_handler.q;

    union CanAcquisinatorMessage status = { 0 };
    status.acquisinator_lockin_status.lockin_dc_u = lockin_handler.dc;
    status.acquisinator_lockin_status.lockin_min_u = lockin_handler.result_min;
    status.acquisinator_lockin_status.lockin_max_u = lockin_handler.result_max;
    status.acquisinator_lockin_status.lockin_spi_error_b = lockin_handler.result_spi_error;
    status.acquisinator_lockin_status.lockin_seq_u = lockin_handler.result_sequence;

    // Everything copied: the ISR may publish the next result
    lockin_handler.ready = false;

    if (send_frame(CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN, can_acquisinator_byte_size_acquisinator_lockin, &lockin) != LOCKIN_RC_OK) {
        return LOCKIN_RC_ERROR;
    }
    return send_frame(CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS, can_acquisinator_byte_size_acquisinator_lockin_status, &status);
}
