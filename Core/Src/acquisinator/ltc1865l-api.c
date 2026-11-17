/*!
 * \file ltc1865l-api.c
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Implementation of the LTC1865L ADC module.
 */

#include "ltc1865l-api.h"
#include "eagletrt.h"

#include <string.h>

EAGLETRT_STATIC struct Ltc1865lHandler handler;

/*!
 * \brief Channel-select byte per channel (single-ended).
 *
 * \details Only the first two bits are used: SGL/DIFF = 1, ODD/SIGN = channel.
 *     The remaining bits are don't-care and repeat the pattern.
 */
EAGLETRT_STATIC const uint8_t channel_select[LTC1865L_CHANNEL_COUNT] = {
    [LTC1865L_CHANNEL_0] = 0xAAU,
    [LTC1865L_CHANNEL_1] = 0xFFU,
};

/*!
 * \brief One full conversion frame.
 *
 * \details CONV high starts the conversion, CONV low enables the output,
 *     then 16 clocks shift out the result of this conversion while shifting
 *     in the channel-select word for the next one.
 *
 * \param[out] raw Raw 16-bit result.
 *
 * \retval LTC1865L_RC_OK on success.
 * \retval LTC1865L_RC_SPI_ERROR if the SPI callback failed.
 */
EAGLETRT_STATIC enum Ltc1865lReturnCode prv_ltc1865l_convert(uint16_t *raw) {
    const uint8_t select = channel_select[handler.config.channel];
    const uint8_t tx[ltc1865l_frame_size] = { select, select };
    uint8_t rx[ltc1865l_frame_size] = { 0 };

    handler.config.conv_callback(true);
    handler.config.delay_callback(ltc1865l_conversion_time_us);
    handler.config.conv_callback(false);

    if (handler.config.spi_callback(tx, rx, ltc1865l_frame_size) != LTC1865L_RC_OK) {
        ++handler.error_count;
        return LTC1865L_RC_SPI_ERROR;
    }

    *raw = (uint16_t)(((uint16_t)rx[0] << 8U) | rx[1]);
    return LTC1865L_RC_OK;
}

enum Ltc1865lReturnCode ltc1865l_api_init(const struct Ltc1865lConfig config) {
    memset(&handler, 0, sizeof(handler));

    if (config.spi_callback == NULL || config.delay_callback == NULL || config.conv_callback == NULL) {
        return LTC1865L_RC_NULL_POINTER;
    }
    if (config.channel >= LTC1865L_CHANNEL_COUNT) {
        return LTC1865L_RC_INVALID_CHANNEL;
    }

    handler.config = config;

    // Dummy conversion: its result belongs to whatever channel the ADC had
    // selected at power-up, but it loads the configured channel for the next read.
    uint16_t discarded = 0U;
    const enum Ltc1865lReturnCode return_code = prv_ltc1865l_convert(&discarded);
    if (return_code != LTC1865L_RC_OK) {
        return return_code;
    }

    handler.initialized = true;
    return LTC1865L_RC_OK;
}

enum Ltc1865lReturnCode ltc1865l_api_read(uint16_t *raw) {
    if (raw == NULL) {
        return LTC1865L_RC_NULL_POINTER;
    }
    if (!handler.initialized) {
        return LTC1865L_RC_NOT_INITIALIZED;
    }
    return prv_ltc1865l_convert(raw);
}

uint32_t ltc1865l_api_get_error_count(void) {
    return handler.error_count;
}
