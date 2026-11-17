/*!
 * \file ltc1865l-api.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Public API of the LTC1865L ADC module.
 *
 * \details Lifecycle:
 *       1. ltc1865l_api_init(config) once, after the SPI peripheral is up.
 *       2. ltc1865l_api_read(&raw) once per sample, typically from the
 *          sampling-timer ISR.
 *
 *     All calls must come from the same context: the module state is not
 *     protected against concurrent access.
 */

#ifndef LTC1865L_API_H
#define LTC1865L_API_H

#include "ltc1865l.h"

/*!
 * \brief Initialize the module and prime the ADC pipeline.
 *
 * \details Stores the configuration and performs one dummy conversion so
 *     that the configured channel is loaded for the first real read.
 *
 * \param[in] config Callbacks and channel to read.
 *
 * \retval LTC1865L_RC_OK on success.
 * \retval LTC1865L_RC_NULL_POINTER if any callback is NULL.
 * \retval LTC1865L_RC_INVALID_CHANNEL if the channel is out of range.
 * \retval LTC1865L_RC_SPI_ERROR if the priming conversion failed.
 */
enum Ltc1865lReturnCode ltc1865l_api_init(const struct Ltc1865lConfig config);

/*!
 * \brief Run one conversion and read the raw code of the configured channel.
 *
 * \param[out] raw Raw 16-bit ADC code (0..65535).
 *
 * \retval LTC1865L_RC_OK on success.
 * \retval LTC1865L_RC_NULL_POINTER if \p raw is NULL.
 * \retval LTC1865L_RC_NOT_INITIALIZED if init has not succeeded.
 * \retval LTC1865L_RC_SPI_ERROR if the SPI transfer failed (\p raw untouched).
 */
enum Ltc1865lReturnCode ltc1865l_api_read(uint16_t *raw);

/*!
 * \brief Number of SPI failures since init.
 *
 * \return The error counter.
 */
uint32_t ltc1865l_api_get_error_count(void);

#endif // LTC1865L_API_H
