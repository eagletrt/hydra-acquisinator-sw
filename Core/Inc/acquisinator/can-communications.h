/*!
 * \file can-communications.h
 * \date 2026-06-12
 * \authors Alessandro Bridi [ale.bridi15@gmail.com]
 * \ingroup CM4_Core
 *
 * \brief Hardware-agnostic definitions for the steering-wheel CAN-bus
 *     communication.
 *
 * \details The CAN module wraps a PAL handler per network and presents a
 *     queue-based API. It deliberately stays network-agnostic: frames
 *     travel as raw {id, length, data} tuples in both directions and the
 *     application is responsible for moving them through libcan.
 *     The module only ever moves bytes; one CAN module covers every
 *     network because libcan keeps each one in its own translation unit.
 */

#ifndef CAN_COMMUNICATIONS_H
#define CAN_COMMUNICATIONS_H

#include "pal.h"
#include "arena-allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*!
 * \brief Maximum payload size of a classic-CAN frame, in bytes.
 *
 * \details FDCAN in classic mode is limited to 8 data bytes. The PAL queues
 *     are sized off the raw-frame buffer that wraps this payload, so any
 *     bump here automatically widens the internal staging buffers.
 */
#define CAN_COMMUNICATIONS_FRAME_DATA_SIZE (8U)

// TODO: adjust capactities to fit libcan later

/*!
 * \brief Depth of the per-network reception queue.
 */
#define CAN_COMMUNICATIONS_RX_QUEUE_CAPACITY (32U)

/*!
 * \brief Depth of the per-network transmission queue.
 */
#define CAN_COMMUNICATIONS_TX_QUEUE_CAPACITY (32U)

/*!
 * \brief Return codes for CAN-communications operations.
 */
enum CanCommunicationReturnCode {
    CAN_COMMUNICATION_RC_OK,                    /*!< Operation completed successfully. */
    CAN_COMMUNICATION_RC_NULL_POINTER,          /*!< A required pointer argument was NULL. */
    CAN_COMMUNICATION_RC_INVALID_NETWORK,       /*!< Network ID is out of range or already initialized. */
    CAN_COMMUNICATION_RC_INVALID_LENGTH,        /*!< Frame length is larger than CAN_COMMUNICATIONS_FRAME_DATA_SIZE. */
    CAN_COMMUNICATION_RC_QUEUE_FULL,            /*!< Target queue has no room. */
    CAN_COMMUNICATION_RC_QUEUE_EMPTY,           /*!< Source queue is empty. */
    CAN_COMMUNICATION_RC_TRANSMISSION_ERROR,    /*!< The user-supplied send callback reported failure. */
    CAN_COMMUNICATION_RC_RECEIVE_HANDLER_ERROR, /*!< The user-supplied receive callback reported failure. */
    CAN_COMMUNICATION_RC_ERROR,                 /*!< Generic failure (PAL/arena/internal). */
};

/*!
 * \brief Available CAN networks on the steering wheel.
 *
 * \details The enum doubles as an index into the per-network handler
 *     arrays, so do not reorder existing entries — only append before
 *     CAN_COMMUNICATION_NETWORK_COUNT.
 */
enum CanCommunicationNetwork {
    CAN_COMMUNICATION_NETWORK_PRIMARY,   /*!< Primary CAN bus (FDCAN1) */
    CAN_COMMUNICATION_NETWORK_SECONDARY, /*!< Secondary CAN bus (FDCAN2) */
    CAN_COMMUNICATION_NETWORK_COUNT,
};

/*!
 * \brief A single CAN frame in or out of the module.
 *
 * \details The data array carries the raw bus payload; \c length tells the
 *     consumer how many bytes are valid. Bytes past \c length are left
 *     undefined and must not be read.
 */
struct [[gnu::packed]] CanCommunicationFrame {
    uint32_t id;                                      /*!< CAN identifier (standard or extended) */
    uint8_t length;                                   /*!< Valid bytes in \c data, 0..CAN_COMMUNICATIONS_FRAME_DATA_SIZE */
    uint8_t data[CAN_COMMUNICATIONS_FRAME_DATA_SIZE]; /*!< Raw payload bytes */
};

/*!
 * \brief Callback invoked when the module needs to put a frame on the raw_frame.
 *
 * \details Registered through can_communications_api_init. Wraps the
 *     vendor HAL call (typically HAL_FDCAN_AddMessageToTxFifoQ on the
 *     STM32H7). The module guarantees \c frame is non-NULL and that
 *     \c frame->length is within CAN_COMMUNICATIONS_FRAME_DATA_SIZE.
 *
 * \param[in] frame The frame to transmit.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_TRANSMISSION_ERROR on a hardware-side failure.
 */
typedef enum CanCommunicationReturnCode (*can_communications_send_callback)(const struct CanCommunicationFrame *frame);

/*!
 * \brief Callback invoked once per frame drained by can_communications_api_process_rx.
 *
 * \details The natural home for the libcan deserialise + switch-on-id:
 *
 *     \code
 *     static enum CanCommunicationReturnCode primary_on_receive(const struct CanCommunicationFrame *frame) {
 *         union CanPrimaryMessages decoded;
 *         if (can_primary_api_deserialize_from_id(frame->id, frame->data, &decoded) < 0) {
 *             return CAN_COMMUNICATION_RC_RECEIVE_HANDLER_ERROR;
 *         }
 *         switch (frame->id) {
 *             case CAN_PRIMARY_MESSAGE_FRAME_ID_ECU_STATUS: ...; break;
 *             ...
 *         }
 *         return CAN_COMMUNICATION_RC_OK;
 *     }
 *     \endcode
 *
 *     Returning anything other than CAN_COMMUNICATION_RC_OK does not stop
 *     the drain — the rest of the queue is still processed — but the
 *     final return code of process_rx will surface the error.
 *
 * \param[in] frame The frame just popped off the RX queue.
 *
 * \retval CAN_COMMUNICATION_RC_OK on success.
 * \retval CAN_COMMUNICATION_RC_RECEIVE_HANDLER_ERROR if dispatch fails.
 */
typedef enum CanCommunicationReturnCode (*can_communications_receive_callback)(const struct CanCommunicationFrame *frame);

/*!
 * \brief Critical-section enter/exit callback type.
 *
 * \details The module's RX queue is pushed from the FDCAN ISR and popped
 *     from the main loop, so the underlying ring buffer needs a critical
 *     section to stay coherent. The two callbacks are passed in at init
 *     time and forwarded straight to PAL; pass NULL on both to opt out
 *     (only safe in single-context tests).
 */
typedef void (*can_communications_critical_section_callback)(void);

/*!
 * \brief Init-time configuration for one CAN network.
 *
 * \details Bundled in a struct (rather than as four positional arguments
 *     on init) so adding a new wiring point later is a struct extension
 *     instead of an API break.
 */
struct CanCommunicationsNetworkConfig {
    can_communications_send_callback send;                 /*!< Required: wraps HAL_FDCAN_AddMessageToTxFifoQ */
    can_communications_receive_callback on_receive;        /*!< Required: dispatch + libcan deserialise */
    can_communications_critical_section_callback cs_enter; /*!< Optional, may be NULL */
    can_communications_critical_section_callback cs_exit;  /*!< Optional, may be NULL */
};

/*!
 * \brief Per-network slot in the module's file-static handler.
 *
 * \details Bundled here (instead of as parallel arrays in the impl) so the
 *     handler struct stays self-documenting. Should not be touched by code
 *     outside can-communications-api.c.
 */
struct CanCommunicationsNetworkState {
    struct PalHandler pal;                          /*!< PAL handler that owns the per-network queues */
    can_communications_send_callback send;          /*!< User-supplied "actually transmit" callback */
    can_communications_receive_callback on_receive; /*!< User-supplied per-frame dispatcher */
};

/*!
 * \brief File-static state of the can-communications module.
 *
 * \details One handler covers every network; the arena allocator is shared
 *     across all PAL instances since PAL only allocates at init time.
 */
struct CanCommunicationsHandler {
    struct ArenaAllocatorHandler arena;           /*!< Arena allocator for PAL staging buffers */
    struct CanCommunicationsNetworkState network; /*!< Per-network state */
};

#endif // CAN_COMMUNICATIONS_H
