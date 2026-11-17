/*!
 * \file raw-stream-api.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Public API of the raw ADC CAN stream.
 *
 * \details Lifecycle:
 *       1. raw_stream_api_init(config) once, after can_communications_api_init.
 *       2. raw_stream_api_on_sample(raw) from the sampling ISR, every sample.
 *       3. raw_stream_api_process() from the main loop, then
 *          can_communications_api_process_tx() to put frames on the bus.
 */

#ifndef RAW_STREAM_API_H
#define RAW_STREAM_API_H

#include "raw-stream.h"

/*!
 * \brief Initialize the module.
 *
 * \param[in] config Critical-section callbacks.
 *
 * \retval RAW_STREAM_RC_OK on success.
 * \retval RAW_STREAM_RC_NULL_POINTER if a callback is NULL.
 */
enum RawStreamReturnCode raw_stream_api_init(const struct RawStreamConfig config);

/*!
 * \brief Feed one ADC sample. ISR context only.
 *
 * \details Keeps one sample every RAW_STREAM_CAN_DECIMATION and publishes a
 *     batch every RAW_STREAM_SAMPLES_PER_FRAME kept samples. If the previous
 *     batch has not been sent yet it is overwritten and counted as dropped.
 *
 * \param[in] raw Raw ADC code.
 */
void raw_stream_api_on_sample(uint16_t raw);

/*!
 * \brief Queue the published batch, if any, as an ACQUISINATOR_RAW frame. Main loop only.
 *
 * \retval RAW_STREAM_RC_OK if a frame was queued or there was nothing to send.
 * \retval RAW_STREAM_RC_SERIALIZE if libcan failed to serialize.
 * \retval RAW_STREAM_RC_QUEUE if the TX queue is full (the sample is lost).
 */
enum RawStreamReturnCode raw_stream_api_process(void);

/*!
 * \brief Number of batches lost because the main loop was too slow.
 *
 * \return The dropped-batch counter.
 */
uint32_t raw_stream_api_get_dropped_count(void);

#endif // RAW_STREAM_API_H
