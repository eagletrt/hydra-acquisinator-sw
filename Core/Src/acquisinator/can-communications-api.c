/*!
 * \file can-communications-api.c
 * \date 2026-06-12
 * \authors Alessandro Bridi [ale.bridi15@gmail.com]
 * \ingroup CM4_Core
 *
 * \brief Implementation of the CAN-bus communication module.
 *
 * \details One file-static handler owns a PAL handler per network plus a
 *     shared arena. Frames travel through PAL as raw bytes of the
 *     CanCommunicationFrame struct itself, and PAL's built-in
 *     memcpy-deserialiser (selected by passing NULL for the deserialise
 *     callback at init) takes care of the copy in both directions..
 */

#include "can-communications-api.h"
#include "arena-allocator-api.h"
#include "pal-api.h"
#include "eagletrt.h"

#include <string.h>

EAGLETRT_STATIC struct CanCommunicationsHandler handler;

/*!
 * \brief Shared body of the per-network PAL send shims.
 *
 * \details PAL's pal_send_callback type has no user-data slot, so the
 *     module installs a one-per-network static shim that thunks into this
 *     helper with the right network ID.
 *
 * \param[in] network Network ID of the slot whose user send to dispatch to.
 * \param[in] message PAL message whose payload is a raw CanCommunicationFrame.
 *
 * \retval PAL_RC_OK on success.
 * \retval PAL_RC_NULL_POINTER if \p message or the user send callback is NULL.
 * \retval PAL_RC_INVALID_ARGUMENT if the message size does not match a frame.
 * \retval PAL_RC_IO_ERROR if the user send callback reported failure.
 */
EAGLETRT_STATIC enum PalReturnCode prv_pal_send_dispatch(const struct PalMessage *message) {
    if (message == NULL) {
        return PAL_RC_NULL_POINTER;
    }
    if (message->size != sizeof(struct CanCommunicationFrame)) {
        return PAL_RC_INVALID_ARGUMENT;
    }

    struct CanCommunicationFrame frame;
    (void)memcpy(&frame, message->payload, sizeof(frame));

    const can_communications_send_callback user_send = handler.network.send;
    if (user_send == NULL) {
        return PAL_RC_NULL_POINTER;
    }
    if (user_send(&frame) != CAN_COMMUNICATION_RC_OK) {
        return PAL_RC_IO_ERROR;
    }
    return PAL_RC_OK;
}

/*!
 * \brief Initialise one network slot in the module's file-static handler.
 *
 * \param[in] network  Network ID to initialise.
 * \param[in] config   User-supplied wiring for this network.
 * \param[in] pal_send PAL send shim hard-wired to this network.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_NULL_POINTER if any required callback is NULL.
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_ERROR if PAL fails to initialise.
 */
EAGLETRT_STATIC enum CanCommunicationReturnCode prv_can_communications_api_init_network(const struct CanCommunicationsNetworkConfig config, pal_send_callback pal_send) {
    if (config.send == NULL || config.on_receive == NULL) {
        return CAN_COMMUNICATION_RC_NULL_POINTER;
    }
    memset(&handler.network, 0, sizeof(handler.network));

    handler.network.send = config.send;
    handler.network.on_receive = config.on_receive;

    if (pal_api_init(
            &handler.network.pal,
            CAN_COMMUNICATIONS_RX_QUEUE_CAPACITY,
            CAN_COMMUNICATIONS_TX_QUEUE_CAPACITY,
            (uint32_t)sizeof(struct CanCommunicationFrame),
            NULL,
            pal_send,
            config.cs_enter,
            config.cs_exit,
            &handler.arena) != PAL_RC_OK) {
        return CAN_COMMUNICATION_RC_ERROR;
    }

    return CAN_COMMUNICATION_RC_OK;
}

enum CanCommunicationReturnCode can_communications_api_init(const struct CanCommunicationsNetworkConfig config) {
    memset(&handler, 0, sizeof(handler));
    arena_allocator_api_init(&handler.arena);

    const enum CanCommunicationReturnCode return_code = prv_can_communications_api_init_network(config, prv_pal_send_dispatch);
    if (return_code != CAN_COMMUNICATION_RC_OK) {
        return return_code;
    }

    return CAN_COMMUNICATION_RC_OK;
}

/*!
 * \brief Shared body of can_communications_api_add_to_tx_buffer and
 *     can_communications_api_add_to_rx_buffer.
 *
 * \param[in] network Target network.
 * \param[in] frame   Frame to push.
 * \param[in] to_tx   true  → push to the TX queue (send path),
 *                    false → push to the RX queue (ISR path).
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_NULL_POINTER if \p frame is NULL.
 * \retval CAN_COMMUNICATION_RC_INVALID_NETWORK if \p network is out of range.
 * \retval CAN_COMMUNICATION_RC_INVALID_LENGTH if \p frame->length is too large.
 * \retval CAN_COMMUNICATION_RC_QUEUE_FULL if the target queue is saturated.
 * \retval CAN_COMMUNICATION_RC_ERROR on a PAL-internal failure.
 */
EAGLETRT_STATIC enum CanCommunicationReturnCode prv_enqueue(const struct CanCommunicationFrame *frame, bool to_tx) {
    if (frame == NULL) {
        return CAN_COMMUNICATION_RC_NULL_POINTER;
    }
    if (frame->length > CAN_COMMUNICATIONS_FRAME_DATA_SIZE) {
        return CAN_COMMUNICATION_RC_INVALID_LENGTH;
    }

    // PAL needs a non-const pointer
    struct CanCommunicationFrame buffer = *frame;
    const enum PalReturnCode return_code = to_tx
                                               ? pal_api_add_to_tx_queue(&handler.network.pal, &buffer, (uint32_t)sizeof(buffer))
                                               : pal_api_add_to_rx_queue(&handler.network.pal, (uint8_t *)&buffer, (uint32_t)sizeof(buffer));

    switch (return_code) {
        case PAL_RC_OK:
            return CAN_COMMUNICATION_RC_OK;
        case PAL_RC_QUEUE_FULL:
            return CAN_COMMUNICATION_RC_QUEUE_FULL;
        case PAL_RC_NULL_POINTER:
            return CAN_COMMUNICATION_RC_NULL_POINTER;
        default:
            return CAN_COMMUNICATION_RC_ERROR;
    }
}

enum CanCommunicationReturnCode can_communications_api_add_to_tx_buffer(const struct CanCommunicationFrame *frame) {
    return prv_enqueue(frame, true);
}

enum CanCommunicationReturnCode can_communications_api_add_to_rx_buffer(const struct CanCommunicationFrame *frame) {
    return prv_enqueue(frame, false);
}

enum CanCommunicationReturnCode can_communications_api_process_tx() {
    enum CanCommunicationReturnCode result = CAN_COMMUNICATION_RC_OK;
    for (uint8_t i = 0; i < CAN_COMMUNICATIONS_TX_QUEUE_CAPACITY; ++i) {
        const enum PalReturnCode return_code = pal_api_process_tx(&handler.network.pal);
        if (return_code == PAL_RC_QUEUE_EMPTY) {
            break;
        }
        if (return_code == PAL_RC_IO_ERROR) {
            /* User send callback reported failure on this frame; keep draining. */
            result = CAN_COMMUNICATION_RC_TRANSMISSION_ERROR;
        } else if (return_code != PAL_RC_OK) {
            return CAN_COMMUNICATION_RC_ERROR;
        }
    }
    return result;
}

enum CanCommunicationReturnCode can_communications_api_process_rx() {
    const can_communications_receive_callback dispatch = handler.network.on_receive;

    enum CanCommunicationReturnCode result = CAN_COMMUNICATION_RC_OK;

    for (uint8_t i = 0; i < CAN_COMMUNICATIONS_RX_QUEUE_CAPACITY; ++i) {
        struct CanCommunicationFrame frame;
        const enum PalReturnCode return_code = pal_api_process_rx(&handler.network.pal, &frame);
        if (return_code == PAL_RC_QUEUE_EMPTY) {
            break;
        }
        if (return_code != PAL_RC_OK) {
            return CAN_COMMUNICATION_RC_ERROR;
        }
        if (dispatch(&frame) != CAN_COMMUNICATION_RC_OK) {
            /* Surface the failure but keep draining the queue. */
            result = CAN_COMMUNICATION_RC_RECEIVE_HANDLER_ERROR;
        }
    }
    return result;
}
