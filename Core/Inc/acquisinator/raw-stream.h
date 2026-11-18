/*!
 * \file raw-stream.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Hardware-agnostic definitions for the raw ADC CAN stream.
 *
 * \details The sampling ISR hands every sample to the module. One sample
 *     every RAW_STREAM_CAN_DECIMATION is kept; every RAW_STREAM_SAMPLES_PER_FRAME
 *     kept samples form a batch that the main loop turns into one
 *     ACQUISINATOR_RAW frame (oldest sample first, plus a sequence counter).
 *
 *     The stream taps the lock-in samples, so its rate is LOCKIN_SAMPLE_RATE_HZ;
 *     RAW_STREAM_CAN_DECIMATION keeps 1 sample every N (1 = all).
 *     CAN frame rate = RAW_STREAM_SAMPLE_RATE_HZ / RAW_STREAM_CAN_DECIMATION / 3.
 */

#ifndef RAW_STREAM_H
#define RAW_STREAM_H

#include <stdint.h>

#include "lockin.h"

/*!
 * \brief ADC sampling rate, in Hz: the lock-in drives the sampling.
 */
#define RAW_STREAM_SAMPLE_RATE_HZ LOCKIN_SAMPLE_RATE_HZ

/*!
 * \brief Keep one sample every N for the CAN stream (1 = every sample).
 */
#define RAW_STREAM_CAN_DECIMATION (1U)

/*!
 * \brief Samples packed in one ACQUISINATOR_RAW frame (fixed by the CAN message layout).
 */
#define RAW_STREAM_SAMPLES_PER_FRAME (3U)

/*!
 * \brief Resulting CAN frame rate, in Hz.
 */
#define RAW_STREAM_CAN_FRAME_RATE_HZ (RAW_STREAM_SAMPLE_RATE_HZ / RAW_STREAM_CAN_DECIMATION / RAW_STREAM_SAMPLES_PER_FRAME)

static_assert(RAW_STREAM_SAMPLE_RATE_HZ > 0U && RAW_STREAM_SAMPLE_RATE_HZ <= 50000U,
              "Sampling rate out of range: one sample takes ~10 us inside the ISR");
static_assert(RAW_STREAM_CAN_DECIMATION > 0U, "Decimation must be at least 1");
static_assert(RAW_STREAM_CAN_FRAME_RATE_HZ <= 3000U,
              "CAN frame rate too high: above ~3000 frames/s the 1 Mbit/s bus is over 40% loaded");

/*!
 * \brief Return codes for raw-stream operations.
 */
enum RawStreamReturnCode : uint8_t {
    RAW_STREAM_RC_OK,           /*!< Operation completed successfully (or nothing to do). */
    RAW_STREAM_RC_NULL_POINTER, /*!< A required callback was NULL. */
    RAW_STREAM_RC_SERIALIZE,    /*!< libcan failed to serialize the frame. */
    RAW_STREAM_RC_QUEUE,        /*!< can-communications refused the frame (queue full). */
};

/*!
 * \brief Critical-section enter/exit callback type.
 *
 * \details The published batch is written by the sampling ISR and read by
 *     the main loop, so the main-loop side copies it inside a critical section.
 */
typedef void (*raw_stream_critical_section_callback)(void);

/*!
 * \brief Init-time configuration.
 */
struct RawStreamConfig {
    raw_stream_critical_section_callback cs_enter; /*!< Required: e.g. __disable_irq */
    raw_stream_critical_section_callback cs_exit;  /*!< Required: e.g. __enable_irq */
};

/*!
 * \brief File-static state of the raw-stream module.
 *
 * \details Should not be touched by code outside raw-stream-api.c.
 */
struct RawStreamHandler {
    struct RawStreamConfig config;                           /*!< Critical-section wiring */
    uint32_t decimation_counter;                             /*!< Samples seen since the last kept one (ISR only) */
    uint16_t filling[RAW_STREAM_SAMPLES_PER_FRAME];          /*!< Batch being filled (ISR only) */
    uint8_t filling_count;                                   /*!< Samples already in \c filling (ISR only) */
    uint8_t next_sequence;                                   /*!< Sequence number of the next completed batch (ISR only) */
    volatile uint16_t pending[RAW_STREAM_SAMPLES_PER_FRAME]; /*!< Complete batch waiting for the main loop */
    volatile uint8_t pending_sequence;                       /*!< Sequence number of \c pending */
    volatile bool pending_ready;                             /*!< true while \c pending has not been sent yet */
    volatile uint32_t dropped_count;                         /*!< Batches overwritten before being sent */
};

#endif // RAW_STREAM_H
