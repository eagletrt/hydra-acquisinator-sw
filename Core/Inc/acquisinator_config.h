#ifndef ACQUISINATOR_CONFIG_H
#define ACQUISINATOR_CONFIG_H

#include "acquisinator_id.h"

/***
 * STRAIN GAUGES -> ACQUISINATOR MAPPING
 */

#define REAR_RIGHT_WHEEL_F27   ACQUISINATOR_ID_15
#define REAR_RIGHT_WHEEL_F58   ACQUISINATOR_ID_2
#define REAR_RIGHT_WHEEL_F46   ACQUISINATOR_ID_11
#define REAR_RIGHT_WHEEL_F1011 ACQUISINATOR_ID_14  // ?
#define REAR_RIGHT_WHEEL_F36   ACQUISINATOR_ID_3   // ?
#define REAR_RIGHT_WHEEL_F17   ACQUISINATOR_ID_17  // ?

#define REAR_LEFT_WHEEL_F27   ACQUISINATOR_ID_6
#define REAR_LEFT_WHEEL_F58   ACQUISINATOR_ID_13
#define REAR_LEFT_WHEEL_F46   ACQUISINATOR_ID_12
#define REAR_LEFT_WHEEL_F1011 ACQUISINATOR_ID_9
#define REAR_LEFT_WHEEL_F36   ACQUISINATOR_ID_10
#define REAR_LEFT_WHEEL_F17   ACQUISINATOR_ID_4

/*
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F27 ACQUISINATOR_ID_
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F58 ACQUISINATOR_ID_
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F46 ACQUISINATOR_ID_
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F1011 ACQUISINATOR_ID_
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F36 ACQUISINATOR_ID_
#define FRONT_RIGHT_WHEEL_ACQUISINATOR_F17 ACQUISINATOR_ID_

#define FRONT_LEFT_WHEEL_F27 ACQUISINATOR_ID_
#define FRONT_LEFT_WHEEL_F58 ACQUISINATOR_ID_
#define FRONT_LEFT_WHEEL_F46 ACQUISINATOR_ID_
#define FRONT_LEFT_WHEEL_F1011 ACQUISINATOR_ID_
#define FRONT_LEFT_WHEEL_F36 ACQUISINATOR_ID_
#define FRONT_LEFT_WHEEL_F17 ACQUISINATOR_ID_
*/

#include <stdint.h>

#if ACQUISINATOR_ID == 0
#error INVALID ACQUISINATOR_ID == 0
#endif

#if ACQUISINATOR_ID == ACQUISINATOR_ID_0
#error INVALID ACQUISINATOR_ID == ACQUISINATOR_ID_0
#endif

/****
 * Memory mapping:
 * 
 * 16KB openblt
 * 28KB user program
 * 2KB  configurations
 * 18KB crashdebug reserved space: ram dump
*/

#define OPENBLT_RESERVED_ADDRESS_0           (0x8000000U)
#define OPENBLT_RESERVED_ADDRESS_1           (0x8000800U)
#define OPENBLT_RESERVED_ADDRESS_2           (0x8001000U)
#define OPENBLT_RESERVED_ADDRESS_3           (0x8001800U)
#define OPENBLT_RESERVED_ADDRESS_4           (0x8002000U)
#define OPENBLT_RESERVED_ADDRESS_5           (0x8002800U)
#define OPENBLT_RESERVED_ADDRESS_6           (0x8003000U)
#define OPENBLT_RESERVED_ADDRESS_7           (0x8003800U)
#define USER_PROGRAM_RESERVED_ADDRESS_0      (0x8004000U)
#define USER_PROGRAM_RESERVED_ADDRESS_1      (0x8004800U)
#define USER_PROGRAM_RESERVED_ADDRESS_2      (0x8005000U)
#define USER_PROGRAM_RESERVED_ADDRESS_3      (0x8005800U)
#define USER_PROGRAM_RESERVED_ADDRESS_4      (0x8006000U)
#define USER_PROGRAM_RESERVED_ADDRESS_5      (0x8006800U)
#define USER_PROGRAM_RESERVED_ADDRESS_6      (0x8007000U)
#define USER_PROGRAM_RESERVED_ADDRESS_7      (0x8007800U)
#define USER_PROGRAM_RESERVED_ADDRESS_8      (0x8008000U)
#define USER_PROGRAM_RESERVED_ADDRESS_9      (0x8008800U)
#define USER_PROGRAM_RESERVED_ADDRESS_10     (0x8009000U)
#define USER_PROGRAM_RESERVED_ADDRESS_11     (0x8009800U)
#define USER_PROGRAM_RESERVED_ADDRESS_12     (0x800A000U)
#define USER_PROGRAM_RESERVED_ADDRESS_13     (0x800A800U)
#define ACQUISINATOR_CONFIG_RESERVED_ADDRESS (0x800B000U)
#define CRASH_DEBUG_RESERVED_ADDRESS_0       (0x800B800U)
#define CRASH_DEBUG_RESERVED_ADDRESS_1       (0x800C000U)
#define CRASH_DEBUG_RESERVED_ADDRESS_2       (0x800C800U)
#define CRASH_DEBUG_RESERVED_ADDRESS_3       (0x800D000U)
#define CRASH_DEBUG_RESERVED_ADDRESS_4       (0x800D800U)
#define CRASH_DEBUG_RESERVED_ADDRESS_5       (0x800E000U)
#define CRASH_DEBUG_RESERVED_ADDRESS_6       (0x800E800U)
#define CRASH_DEBUG_RESERVED_ADDRESS_7       (0x800F000U)
#define CRASH_DEBUG_RESERVED_ADDRESS_8       (0x800F800U)

typedef enum { ltc1865_DIFF, ltc1865_DIFF_INVERTED, ltc1865_SE_CH1, ltc1865_SE_CH2, LTC1865_N_CHANNELS } ltc1865_channel_t;

#define ACQUISINATORE_NO_FILTER         (0U)
#define ACQUISINATORE_SIMPLE_MOVING_AVG (1U)
#define ACQUISINATORE_GAUSSIAN_FILTER   (2U)
#define ACQUISINATORE_OLD_MOVING_AVG    (3U)
#define ACQUISINATORE_FILTER_TYPE       ACQUISINATORE_SIMPLE_MOVING_AVG

// The ADC is sampled by the TIM2 interrupt at a fixed rate and filtered sample by sample, then one
// value every ACQUISINATORE_ADC_SEND_EVERY samples is sent on CAN. Two filters, both cancel the 50 Hz
// mains interference picked up by the bridge instead of letting it alias into slow drifts:
//  - AVG_20MS: moving average over exactly one mains period (zeros at 50 Hz and all its harmonics).
//    Bandwidth ~22 Hz (-3 dB), no overshoot, 10 ms delay. Lowest noise: static and slow tests.
//  - NOTCH_LP: notches at 50, 100, 150 Hz + 4th order Butterworth low-pass at SEND_RATE / 4 (75 Hz at
//    300 Hz). Flat to ~30 Hz at 300 Hz, ~6 ms delay, ~13% overshoot and ~100 ms ringing on a
//    step, about twice the noise of AVG_20MS (wider band). Dynamic tests.
#define ACQUISINATORE_ADC_FILTER_AVG_20MS (1U)
#define ACQUISINATORE_ADC_FILTER_NOTCH_LP (2U)

// ---- the two settings to change ----
#define ACQUISINATORE_ADC_FILTER    ACQUISINATORE_ADC_FILTER_AVG_20MS
// frames per second on CAN: 300 for bench tests; lower in the car to load the bus less
// (~130 bit per frame: 300 Hz = 4% of 1 Mbit/s). Must divide 1500: 300, 250, 150, 100, 75, 60, 50...
#define ACQUISINATORE_ADC_SEND_RATE (300U)
// ------------------------------------

#define ACQUISINATORE_ADC_SAMPLE_RATE  (1500U)  // Hz, TIM2 update rate
#define ACQUISINATORE_ADC_AVG_SAMPLES  (30U)    // AVG_20MS: 30 / 1500 Hz = 20 ms = one 50 Hz period
#define ACQUISINATORE_ADC_SEND_EVERY   (ACQUISINATORE_ADC_SAMPLE_RATE / ACQUISINATORE_ADC_SEND_RATE)
// NOTCH_LP: the low-pass follows the send rate, so a lower rate does not alias
#define ACQUISINATORE_ADC_LP_CUTOFF_HZ ((float)ACQUISINATORE_ADC_SEND_RATE / 4.0f)
#define ACQUISINATORE_ADC_NOTCH_Q      (5.0f)  // higher = narrower notch but longer ringing

#if ACQUISINATORE_ADC_FILTER != ACQUISINATORE_ADC_FILTER_AVG_20MS && \
    ACQUISINATORE_ADC_FILTER != ACQUISINATORE_ADC_FILTER_NOTCH_LP
#error "unknown ACQUISINATORE_ADC_FILTER"
#endif
#if ACQUISINATORE_ADC_SAMPLE_RATE % ACQUISINATORE_ADC_SEND_RATE != 0U
#error "ACQUISINATORE_ADC_SEND_RATE must divide ACQUISINATORE_ADC_SAMPLE_RATE"
#endif
#if (ACQUISINATORE_ADC_AVG_SAMPLES * 1000U / ACQUISINATORE_ADC_SAMPLE_RATE) % 20U != 0U
#error "the ADC average must last a whole number of 50 Hz periods (multiple of 20 ms)"
#endif
#if (ACQUISINATORE_ADC_AVG_SAMPLES * 1000U) % ACQUISINATORE_ADC_SAMPLE_RATE != 0U
#error "the ADC average must last a whole number of milliseconds"
#endif

// the only frame sent (format in Tools/acquisinator_hydra_adc.dbc), free on the primary network
#define ACQUISINATORE_ADC_CAN_ID (0x7B0U)

typedef enum {
    acquisinatore_led_code_no_error = 0,
    acquisinatore_led_code_all_ok,
    acquisinatore_led_code_can_not_working,
    acquisinatore_led_code_spi_error,
    acquisinatore_led_code_read_write_flash,
    acquisinatore_led_code_flashed_firmware_with_wrong_id,
    acquisinatore_led_code_n_values
} acquisinatore_led_code_t;

void acquisinatore_turn_led(int on);
uint32_t get_timestamp_ms(void);
void system_reset(void);

#define ACQUISINATORE_VREF_INT (3.29f)

#define STRAIN_GAUGE_R1   (352.0f)
#define STRAIN_GAUGE_R2   (350.0f)
#define STRAIN_GAUGE_R3   (352.0f)
#define STRAIN_GAUGE_RNOM (350.0f)
#define STRAIN_GAUGE_G    (100.0f)
#define STRAIN_GAUGE_VREF (1.501f)
#define STRAIN_GAUGE_VS   (3.294f)
#define STRAIN_GAUGE_K    (2.01f)

#endif
