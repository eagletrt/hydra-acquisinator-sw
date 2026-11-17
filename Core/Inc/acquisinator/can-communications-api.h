/*!
 * \file can-communications-api.h
 * \date 2026-06-12
 * \authors Alessandro Bridi [ale.bridi15@gmail.com]
 * \ingroup CM4_Core
 *
 * \brief Public API for the steering-wheel CAN-bus communication module.
 *
 * \details The module owns one PAL handler per network. All wiring goes like this
 *     Lifecycle (per network):
 *       1. can_communications_api_init(network, &config);
 *       2. From the FDCAN RX ISR, build a CanCommunicationFrame from the
 *          HAL header + payload and push it with
 *          can_communications_api_add_to_rx_buffer.
 *       3. Queue outgoing frames with
 *          can_communications_api_add_to_tx_buffer (after running the
 *          libcan serialiser).
 *       4. From the main loop, call can_communications_api_process_tx and
 *          can_communications_api_process_rx for each active network.
 *          Both drain the entire queue in one call.
 */

#ifndef CAN_COMMUNICATIONS_API_H
#define CAN_COMMUNICATIONS_API_H

#include "can-communications.h"

/*!
 * \brief Initialize the CAN-communications module.
 *
 * \param[in] configs  Per-network wiring (send + on_receive required, cs
 *     callbacks optional). One for each network, in order.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_NULL_POINTER if \p config is NULL or any
 *     of its required callbacks (send, on_receive) is NULL.
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of
 *     range or already initialized.
 * \retval CAN_COMMUNICATION_RC_ERROR if PAL or the arena fail to set up.
 */
enum CanCommunicationReturnCode can_communications_api_init(const struct CanCommunicationsNetworkConfig config);

/*!
 * \brief Push a frame into the TX queue.
 *
 * \param[in] network The CAN bus the frame is destined for.
 * \param[in] frame   The frame to send.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_NULL_POINTER if \p frame is NULL.
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_INVALID_LENGTH if \p frame->length exceeds
 *     CAN_COMMUNICATIONS_FRAME_DATA_SIZE.
 * \retval CAN_COMMUNICATION_RC_NOT_INITIALIZED if \p network has not been
 *     initialized yet.
 * \retval CAN_COMMUNICATION_RC_QUEUE_FULL if the TX queue is saturated.
 * \retval CAN_COMMUNICATION_RC_ERROR on a PAL-internal failure.
 */
enum CanCommunicationReturnCode can_communications_api_add_to_tx_buffer( const struct CanCommunicationFrame *frame);

/*!
 * \brief Push a frame into the RX queue.
 *
 * \param[in] network The CAN bus the frame arrived on.
 * \param[in] frame   The freshly-received frame.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_NULL_POINTER if \p frame is NULL.
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_INVALID_LENGTH if \p frame->length exceeds
 *     CAN_COMMUNICATIONS_FRAME_DATA_SIZE.
 * \retval CAN_COMMUNICATION_RC_NOT_INITIALIZED if \p network has not been
 *     initialized yet.
 * \retval CAN_COMMUNICATION_RC_QUEUE_FULL if the RX queue is saturated.
 * \retval CAN_COMMUNICATION_RC_ERROR on a PAL-internal failure.
 */
enum CanCommunicationReturnCode can_communications_api_add_to_rx_buffer(const struct CanCommunicationFrame *frame);

/*!
 * \brief Drain the entire TX queue, sending each frame via the user callback.
 *
 * \param[in] network The CAN bus whose TX queue to drain.
 *
 * \retval CAN_COMMUNICATION_RC_OK if every frame was sent (or there were
 *     none to begin with).
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_NOT_INITIALIZED if \p network has not been
 *     initialized yet.
 * \retval CAN_COMMUNICATION_RC_TRANSMISSION_ERROR if at least one send
 *     callback reported failure.
 * \retval CAN_COMMUNICATION_RC_ERROR on a PAL-internal failure.
 */
enum CanCommunicationReturnCode can_communications_api_process_tx();

/*!
 * \brief Drain the entire RX queue, dispatching each frame via the user
 *     on_receive callback.
 *
 * \param[in] network The CAN bus whose RX queue to drain.
 *
 * \retval CAN_COMMUNICATION_RC_OK if every frame was dispatched (or there
 *     were none to begin with).
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_NOT_INITIALIZED if \p network has not been
 *     initialized yet.
 * \retval CAN_COMMUNICATION_RC_RECEIVE_HANDLER_ERROR if at least one
 *     on_receive callback reported failure.
 * \retval CAN_COMMUNICATION_RC_ERROR on a PAL-internal failure.
 */
enum CanCommunicationReturnCode can_communications_api_process_rx();

#endif // CAN_COMMUNICATIONS_API_H
