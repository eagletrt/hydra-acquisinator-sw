/*!
 * \file lockin.h
 * \date 2026-11-17
 * \authors Valerio Cancemi
 *
 * \brief Lock-in measurement of the strain gauge bridge.
 *
 * \details The DAC excites the bridge with a sine of LOCKIN_SAMPLES_PER_PERIOD
 *     points; the ADC samples the amplifier output once per point. Each sample
 *     is multiplied by the sine and cosine of the excitation point that
 *     produced it and summed over a window of LOCKIN_WINDOW_PERIODS whole
 *     periods. At the end of the window:
 *         i = A*cos(phi), q = A*sin(phi)   (peak, ADC counts)
 *     where A and phi are the amplitude and phase of the amplifier output at the
 *     excitation frequency. DC, 50 Hz and its harmonics cancel exactly.
 */

#ifndef LOCKIN_H
#define LOCKIN_H

#include <stdint.h>

/*!
 * \brief Excitation frequency [Hz]: not a multiple of 50 Hz.
 */
#define LOCKIN_EXC_FREQ_HZ (125U)

/*!
 * \brief DAC points / ADC samples per excitation period.
 */
#define LOCKIN_SAMPLES_PER_PERIOD (64U)

/*!
 * \brief Sampling rate [Hz], i.e. the TIM2 interrupt rate.
 */
#define LOCKIN_SAMPLE_RATE_HZ (LOCKIN_EXC_FREQ_HZ * LOCKIN_SAMPLES_PER_PERIOD)

/*!
 * \brief Window length in excitation periods: one result per window.
 *
 * \details Must last a multiple of 40 ms (two mains periods) to reject 50 Hz:
 *     5 periods = 40 ms (25 results/s), 25 periods = 200 ms (5 results/s, less noise).
 */
#define LOCKIN_WINDOW_PERIODS (5U)

/*!
 * \brief Window length in samples.
 */
#define LOCKIN_WINDOW_SAMPLES (LOCKIN_SAMPLES_PER_PERIOD * LOCKIN_WINDOW_PERIODS)

/*!
 * \brief Excitation: DC + AMP * sin [V], on a 12-bit DAC referred to VDDA.
 *
 * \details Board (acquisinator-hw dc9e10f, bridge top rewired from VDDA to the
 *     DAC buffer U2): the bridge mid nodes sit at half the excitation, so the
 *     AD8553 inputs swing 0.125..0.425 V. Its input range at 3.3 V supply is
 *     about 0..1.6 V: these levels leave only ~0.12 V of margin to GND.
 *     Output = Vref/2 (1.5 V) + 100 * (Vin+ - Vin-), ADC range 0..3.0 V (AD1583).
 */
#define LOCKIN_EXC_DC_V (0.55F)
#define LOCKIN_EXC_AMP_V (0.30F)
#define LOCKIN_DAC_VREF_V (3.3F)
#define LOCKIN_DAC_MAX_CODE (4095U)

static_assert(((LOCKIN_WINDOW_PERIODS * 1000U) % LOCKIN_EXC_FREQ_HZ == 0U) &&
                  (((LOCKIN_WINDOW_PERIODS * 1000U) / LOCKIN_EXC_FREQ_HZ) % 40U == 0U),
              "The window must last a multiple of 40 ms to reject 50 Hz");

enum LockinReturnCode {
    LOCKIN_RC_OK,
    LOCKIN_RC_ERROR,
};

/*!
 * \brief State of the lock-in, written by the sampling ISR.
 *
 * \details The result fields are written by the ISR only while \c ready is
 *     false and read by the main loop only while it is true, so no critical
 *     section is needed.
 */
struct LockinHandler {
    int16_t sin_q15[LOCKIN_SAMPLES_PER_PERIOD];    /*!< Reference sine, Q15 (cos = sin shifted by a quarter period) */
    uint16_t dac_table[LOCKIN_SAMPLES_PER_PERIOD]; /*!< DAC codes of one excitation period */
    uint32_t index;                                /*!< Excitation point currently on the DAC */
    uint32_t count;                                /*!< Samples in the current window */
    int64_t acc_i;                                 /*!< Sum of sample * sin */
    int64_t acc_q;                                 /*!< Sum of sample * cos */
    uint32_t acc_dc;                               /*!< Sum of samples */
    uint16_t min;                                  /*!< Smallest sample of the window */
    uint16_t max;                                  /*!< Largest sample of the window */
    bool spi_error;                                /*!< An ADC read failed since the last result */
    uint8_t sequence;                              /*!< Counter of the produced results */

    volatile bool ready;                           /*!< A result is waiting to be sent */
    volatile float i;                              /*!< In-phase amplitude [counts, peak] */
    volatile float q;                              /*!< Quadrature amplitude [counts, peak] */
    volatile uint16_t dc;                          /*!< Mean of the window [counts] */
    volatile uint16_t result_min;                  /*!< Smallest sample of the window [counts] */
    volatile uint16_t result_max;                  /*!< Largest sample of the window [counts] */
    volatile bool result_spi_error;                /*!< The window followed an ADC read failure */
    volatile uint8_t result_sequence;              /*!< Sequence number of the result */
};

#endif // LOCKIN_H
