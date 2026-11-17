/*!
 * \file raw-stream-api.c
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Implementation of the raw ADC CAN stream.
 */

#include "raw-stream-api.h"
#include "can-acquisinator-api.h"
#include "can-communications-api.h"
#include "eagletrt.h"

#include <string.h>

EAGLETRT_STATIC struct RawStreamHandler handler;

enum RawStreamReturnCode raw_stream_api_init(const struct RawStreamConfig config) {
    memset(&handler, 0, sizeof(handler));
    if (config.cs_enter == NULL || config.cs_exit == NULL) {
        return RAW_STREAM_RC_NULL_POINTER;
    }
    handler.config = config;
    return RAW_STREAM_RC_OK;
}

void raw_stream_api_on_sample(uint16_t raw) {
    if (++handler.decimation_counter < RAW_STREAM_CAN_DECIMATION) {
        return;
    }
    handler.decimation_counter = 0U;

    handler.filling[handler.filling_count++] = raw;
    if (handler.filling_count < RAW_STREAM_SAMPLES_PER_FRAME) {
        return;
    }
    handler.filling_count = 0U;

    // The sequence advances for every batch, sent or not: a gap on the bus
    // shows both CAN losses and batches overwritten here.
    if (handler.pending_ready) {
        ++handler.dropped_count;
    }
    for (uint8_t i = 0U; i < RAW_STREAM_SAMPLES_PER_FRAME; ++i) {
        handler.pending[i] = handler.filling[i];
    }
    handler.pending_sequence = handler.next_sequence++;
    handler.pending_ready = true;
}

enum RawStreamReturnCode raw_stream_api_process(void) {
    uint16_t samples[RAW_STREAM_SAMPLES_PER_FRAME];
    uint8_t sequence = 0U;

    handler.config.cs_enter();
    const bool ready = handler.pending_ready;
    if (ready) {
        for (uint8_t i = 0U; i < RAW_STREAM_SAMPLES_PER_FRAME; ++i) {
            samples[i] = handler.pending[i];
        }
        sequence = handler.pending_sequence;
        handler.pending_ready = false;
    }
    handler.config.cs_exit();

    if (!ready) {
        return RAW_STREAM_RC_OK;
    }

    union CanAcquisinatorMessage message = { 0 };
    message.acquisinator_raw.debug_adc_raw0_u = samples[0];
    message.acquisinator_raw.debug_adc_raw1_u = samples[1];
    message.acquisinator_raw.debug_adc_raw2_u = samples[2];
    message.acquisinator_raw.debug_seq_u = sequence;

    struct CanCommunicationFrame frame = { 0 };
    if (can_acquisinator_api_serialize_from_id(CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW, &message, frame.data) < 0) {
        return RAW_STREAM_RC_SERIALIZE;
    }
    frame.id = (uint32_t)CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW;
    frame.length = can_acquisinator_byte_size_acquisinator_raw;

    if (can_communications_api_add_to_tx_buffer(&frame) != CAN_COMMUNICATION_RC_OK) {
        return RAW_STREAM_RC_QUEUE;
    }
    return RAW_STREAM_RC_OK;
}

uint32_t raw_stream_api_get_dropped_count(void) {
    return handler.dropped_count;
}
