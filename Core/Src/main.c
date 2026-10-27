/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "dac.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "acquisinator_config.h"
#include "error_codes.h"
#include "math.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#if ACQUISINATOR_ID == 0
#error INVALID ACQUISINATOR_ID == 0
#endif

#if ACQUISINATOR_ID == ACQUISINATOR_ID_0
#error INVALID ACQUISINATOR_ID == ACQUISINATOR_ID_0
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// written by the TIM2 interrupt (acquisinatore_adc_tick), read by the main loop
typedef struct {
    float filtered;  // filtered value, in ADC codes (not rounded)
    uint16_t last;   // last single code
} acquisinatore_adc_snapshot_t;

static volatile acquisinatore_adc_snapshot_t adc_snapshot;
static volatile uint8_t adc_snapshot_ready = 0;
static volatile uint32_t adc_spi_errors    = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

float read_float_from_flash(float *address) {
    if (address == NULL)
        return 0.0f;
    return *address;
}

float acquisinatore_link_deformation_from_V_to_elongation(float value_in_V) {
    return (
        (STRAIN_GAUGE_RNOM -
         ((STRAIN_GAUGE_R2 * STRAIN_GAUGE_R3) -
          (STRAIN_GAUGE_R2 * ((value_in_V - STRAIN_GAUGE_VREF) / STRAIN_GAUGE_G) * (STRAIN_GAUGE_R1 + STRAIN_GAUGE_R2)) / STRAIN_GAUGE_VS) /
             (STRAIN_GAUGE_R1 +
              ((((value_in_V - STRAIN_GAUGE_VREF) / STRAIN_GAUGE_G) * (STRAIN_GAUGE_R1 + STRAIN_GAUGE_R2)) / STRAIN_GAUGE_VS))) /
        (STRAIN_GAUGE_RNOM * STRAIN_GAUGE_K));
}

void acquisinatore_turn_led(int on) {
    if (on) {
        HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);
    }
}

uint32_t get_timestamp_ms(void) {
    return HAL_GetTick();
}

void system_reset(void) {
    HAL_NVIC_SystemReset();
}

#if ACQUISINATORE_ADC_FILTER == ACQUISINATORE_ADC_FILTER_AVG_20MS

// moving average over one mains period; returns 0 until the window is full
static int acquisinatore_adc_filter(uint16_t code, float *out) {
    static uint16_t window[ACQUISINATORE_ADC_AVG_SAMPLES];
    static uint32_t sum    = 0;
    static uint32_t idx    = 0;
    static uint32_t filled = 0;

    sum += code;
    sum -= window[idx];
    window[idx] = code;
    idx         = (idx + 1U) % ACQUISINATORE_ADC_AVG_SAMPLES;
    if (filled < ACQUISINATORE_ADC_AVG_SAMPLES) {
        filled++;
        return 0;
    }
    *out = (float)sum / (float)ACQUISINATORE_ADC_AVG_SAMPLES;
    return 1;
}

#elif ACQUISINATORE_ADC_FILTER == ACQUISINATORE_ADC_FILTER_NOTCH_LP

// biquad in direct form II transposed, coefficients from the RBJ audio EQ cookbook (a0 = 1)
typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} biquad_t;

#define ADC_N_BIQUADS (5U)  // notch 50, 100, 150 Hz + two sections of the 4th order low-pass
static biquad_t adc_biquads[ADC_N_BIQUADS];

static void biquad_set(biquad_t *f, float b0, float b1, float b2, float a0, float a1, float a2) {
    f->b0 = b0 / a0;
    f->b1 = b1 / a0;
    f->b2 = b2 / a0;
    f->a1 = a1 / a0;
    f->a2 = a2 / a0;
}

static void biquad_notch(biquad_t *f, float f0, float q) {
    float w = 2.0f * (float)M_PI * f0 / (float)ACQUISINATORE_ADC_SAMPLE_RATE;
    float alpha = sinf(w) / (2.0f * q), c = cosf(w);
    biquad_set(f, 1.0f, -2.0f * c, 1.0f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
}

static void biquad_lowpass(biquad_t *f, float fc, float q) {
    float w = 2.0f * (float)M_PI * fc / (float)ACQUISINATORE_ADC_SAMPLE_RATE;
    float alpha = sinf(w) / (2.0f * q), c = cosf(w);
    biquad_set(f, (1.0f - c) / 2.0f, 1.0f - c, (1.0f - c) / 2.0f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
}

static void acquisinatore_adc_filter_init(void) {
    biquad_notch(&adc_biquads[0], 50.0f, ACQUISINATORE_ADC_NOTCH_Q);
    biquad_notch(&adc_biquads[1], 100.0f, ACQUISINATORE_ADC_NOTCH_Q);
    biquad_notch(&adc_biquads[2], 150.0f, ACQUISINATORE_ADC_NOTCH_Q);
    // Butterworth 4th order = two 2nd order sections with these Q
    biquad_lowpass(&adc_biquads[3], ACQUISINATORE_ADC_LP_CUTOFF_HZ, 0.5412f);
    biquad_lowpass(&adc_biquads[4], ACQUISINATORE_ADC_LP_CUTOFF_HZ, 1.3066f);
}

// notch chain + low-pass; the state starts at steady state on the first code (no start transient)
static int acquisinatore_adc_filter(uint16_t code, float *out) {
    static uint8_t primed = 0;
    float x = (float)code;
    for (uint32_t i = 0; i < ADC_N_BIQUADS; i++) {
        biquad_t *f = &adc_biquads[i];
        if (!primed) {
            // every section has unity DC gain: output = input, states as for a constant input
            f->z2 = f->b2 * x - f->a2 * x;
            f->z1 = f->b1 * x - f->a1 * x + f->z2;
        }
        float y = f->b0 * x + f->z1;
        f->z1   = f->b1 * x - f->a1 * y + f->z2;
        f->z2   = f->b2 * x - f->a2 * y;
        x       = y;
    }
    primed = 1;
    *out   = x;
    return 1;
}

#endif

/**
 * Called by the TIM2 interrupt at ACQUISINATORE_ADC_SAMPLE_RATE: one ADC sample per call, all
 * equally spaced, through the filter chosen with ACQUISINATORE_ADC_FILTER. Every
 * ACQUISINATORE_ADC_SEND_EVERY samples a snapshot is handed to the main loop, which sends it on CAN.
 */
static void acquisinatore_adc_tick(void) {
    static uint32_t since_send    = 0;
    static uint8_t first_transfer = 1;

    uint16_t code;
    if (ltc1865_transfer(ltc1865_SE_CH2, &code) != 0) {
        adc_spi_errors++;
        return;  // the sample is lost, the filter is not polluted
    }
    if (first_transfer) {
        first_transfer = 0;  // the channel of this conversion was not selected yet
        return;
    }

    float filtered;
    if (!acquisinatore_adc_filter(code, &filtered)) {
        return;  // the filter is not ready yet
    }
    if (++since_send >= ACQUISINATORE_ADC_SEND_EVERY) {
        since_send            = 0;
        adc_snapshot.filtered = filtered;
        adc_snapshot.last     = code;
        adc_snapshot_ready    = 1;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_CAN_Init();
  MX_DAC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

    acquisinatore_set_led_code(acquisinatore_led_code_all_ok);

    uint32_t spi_errors_seen = 0;
#if ACQUISINATORE_ADC_FILTER == ACQUISINATORE_ADC_FILTER_NOTCH_LP
    acquisinatore_adc_filter_init();
#endif
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {  // starts the ADC sampling
        Error_Handler();
    }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
        if (adc_snapshot_ready) {
            __disable_irq();
            acquisinatore_adc_snapshot_t snapshot = {.filtered = adc_snapshot.filtered, .last = adc_snapshot.last};
            adc_snapshot_ready = 0;
            __enable_irq();

            // the notch/low-pass can overshoot past the ADC range on a step: clamp before rounding
            float filtered = fminf(fmaxf(snapshot.filtered, 0.0f), 65535.0f);
            // raw data only (traction machine test): the conversion to strain is done offline
            acquisinatore_send_adc(ACQUISINATORE_VREF_INT * 1000.0f * snapshot.filtered / 65536.0f, snapshot.last,
                                   (uint16_t)(filtered + 0.5f));
        }
        if (adc_spi_errors != spi_errors_seen) {
            spi_errors_seen = adc_spi_errors;
            acquisinatore_set_led_code(acquisinatore_led_code_spi_error);
        }
        acquisinatore_led_code_routine();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
    if (htim->Instance == TIM2) {
        acquisinatore_adc_tick();
    }
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    HAL_NVIC_SystemReset();
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
