/*!
 * \file lockin-api.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Public API of the lock-in measurement.
 */

#ifndef LOCKIN_API_H
#define LOCKIN_API_H

#include "lockin.h"

/*!
 * \brief Build the excitation tables and reset the window.
 *
 * \return The first DAC code, to write before starting the DAC.
 */
uint16_t lockin_api_init(void);

/*!
 * \brief Feed one ADC sample and advance the excitation. Sampling ISR only.
 *
 * \param sample_ok false if the ADC read failed: the window restarts
 * \param raw ADC code, ignored if \p sample_ok is false
 *
 * \return The next DAC code, to write right away.
 */
uint16_t lockin_api_on_sample(bool sample_ok, uint16_t raw);

/*!
 * \brief Send the last result, if any, as ACQUISINATOR_LOCKIN + ACQUISINATOR_LOCKIN_STATUS. Main loop only.
 */
enum LockinReturnCode lockin_api_send_result(void);

#endif // LOCKIN_API_H
