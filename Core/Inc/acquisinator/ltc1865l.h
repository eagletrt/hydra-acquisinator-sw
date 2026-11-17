/*!
 * \file ltc1865l.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Hardware-agnostic definitions for the LTC1865L 16-bit SPI ADC.
 *
 * \details The module only knows the LTC1865L protocol: every hardware
 *     access (CONV pin, SPI transfer, microsecond delay) goes through the
 *     callbacks in Ltc1865lConfig, so no HAL header is ever included.
 *
 *     The ADC is pipelined: the channel-select word shifted in during a
 *     frame selects the channel of the *next* conversion. The module always
 *     reads the single channel chosen at init, so after the dummy conversion
 *     done by init every read returns a valid sample of that channel.
 */

#ifndef LTC1865L_H
#define LTC1865L_H

#include <stddef.h>
#include <stdint.h>

/*!
 * \brief Time the CONV pin is held high to complete a conversion, in µs.
 *
 * \details Datasheet tCONV max is 4.66 µs over temperature.
 */
static constexpr uint8_t ltc1865l_conversion_time_us = 5U;

/*!
 * \brief Size of one SPI frame (16-bit result / channel-select word), in bytes.
 */
static constexpr size_t ltc1865l_frame_size = 2U;

/*!
 * \brief Single-ended input channels of the ADC.
 */
enum Ltc1865lChannel : uint8_t {
    LTC1865L_CHANNEL_0,     /*!< CH0, single-ended */
    LTC1865L_CHANNEL_1,     /*!< CH1, single-ended */
    LTC1865L_CHANNEL_COUNT, /*!< Number of channels */
};

/*!
 * \brief Return codes for LTC1865L operations.
 */
enum Ltc1865lReturnCode : uint8_t {
    LTC1865L_RC_OK,              /*!< Operation completed successfully. */
    LTC1865L_RC_NULL_POINTER,    /*!< A required pointer argument or callback was NULL. */
    LTC1865L_RC_INVALID_CHANNEL, /*!< Channel is out of range. */
    LTC1865L_RC_NOT_INITIALIZED, /*!< ltc1865l_api_init has not succeeded yet. */
    LTC1865L_RC_SPI_ERROR,       /*!< The SPI callback reported a failure. */
};

/*!
 * \brief Drives the CONV pin of the ADC.
 *
 * \param[in] high true to set the pin high (start conversion), false to set it low (read out).
 */
typedef void (*ltc1865l_conv_callback)(bool high);

/*!
 * \brief Full-duplex SPI transfer, MSB first.
 *
 * \param[in]  tx     Bytes to shift out.
 * \param[out] rx     Buffer receiving the bytes shifted in, same length as \p tx.
 * \param[in]  length Number of bytes to exchange.
 *
 * \retval LTC1865L_RC_OK on success.
 * \retval LTC1865L_RC_SPI_ERROR on a hardware-side failure or timeout.
 */
typedef enum Ltc1865lReturnCode (*ltc1865l_spi_callback)(const uint8_t *tx, uint8_t *rx, size_t length);

/*!
 * \brief Busy-waits for the given number of microseconds.
 *
 * \details Must work inside an ISR (e.g. DWT cycle counter, not HAL_GetTick).
 *
 * \param[in] delay_us Delay in microseconds.
 */
typedef void (*ltc1865l_delay_callback)(uint32_t delay_us);

/*!
 * \brief Init-time configuration of the ADC.
 */
struct Ltc1865lConfig {
    ltc1865l_spi_callback spi_callback;     /*!< Required: SPI transfer */
    ltc1865l_delay_callback delay_callback; /*!< Required: microsecond delay */
    ltc1865l_conv_callback conv_callback;   /*!< Required: CONV pin control */
    enum Ltc1865lChannel channel;           /*!< Channel read by every ltc1865l_api_read */
};

/*!
 * \brief File-static state of the LTC1865L module.
 *
 * \details Should not be touched by code outside ltc1865l-api.c.
 */
struct Ltc1865lHandler {
    struct Ltc1865lConfig config; /*!< Wiring and channel given at init */
    uint32_t error_count;         /*!< SPI failures since init */
    bool initialized;             /*!< true once init has succeeded */
};

#endif // LTC1865L_H
