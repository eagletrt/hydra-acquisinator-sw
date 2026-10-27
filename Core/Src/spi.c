/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    spi.c
 * @brief   This file provides code for the configuration
 *          of the SPI instances.
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
#include "spi.h"

/* USER CODE BEGIN 0 */

#include "acquisinatore.h"

#define MOVING_AVG_WINDOW (50)

uint16_t ltc1865_raw_values[LTC1865_N_CHANNELS];
uint16_t ltc1865_raw_filtered_values[LTC1865_N_CHANNELS];
float ltc1865_values_in_V[LTC1865_N_CHANNELS];

uint16_t acquisinatore_current_mean[LTC1865_N_CHANNELS];
uint16_t acquisinatore_history[LTC1865_N_CHANNELS][MOVING_AVG_WINDOW];
uint64_t acquisinatore_hsum[LTC1865_N_CHANNELS];
size_t acquisinatore_history_len[LTC1865_N_CHANNELS];
int acquisinatore_completed_loop[LTC1865_N_CHANNELS];

void wait_5us(void);

// both bytes shifted into SDI select the channel of the next conversion
static uint8_t ltc1865_channel_selection_bits(ltc1865_channel_t c) {
    switch (c) {
        case ltc1865_SE_CH1:
            return 0xFF;
        case ltc1865_SE_CH2:
            return 0xAA;
        case ltc1865_DIFF_INVERTED:
            return 0x55;
        case ltc1865_DIFF:
        default:
            return 0x00;
    }
}

int ltc1865_select_channel(ltc1865_channel_t c) {
    uint8_t channel_selection_bits[2] = {ltc1865_channel_selection_bits(c), ltc1865_channel_selection_bits(c)};
    HAL_GPIO_WritePin(GPIOA, SPI_CS_Pin, GPIO_PIN_SET);
    // wait_5us();
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOA, SPI_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef spi_trasm_res = HAL_SPI_Transmit(&hspi1, channel_selection_bits, 2, 100);
    if (spi_trasm_res == HAL_OK) {
        return 1;
    }
    return 0;
}

uint16_t ltc1865_spi_rcv(void) {
    uint16_t cell_value = 0;
    HAL_GPIO_WritePin(GPIOA, SPI_CS_Pin, GPIO_PIN_SET);
    // wait_5us();
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOA, SPI_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Receive(&hspi1, (uint8_t *)&cell_value, 2, 100) != HAL_OK) {
        return -1;
    }
    return cell_value;
}

#if ACQUISINATORE_FILTER_TYPE == ACQUISINATORE_SIMPLE_MOVING_AVG

uint16_t moving_avg(uint16_t *A, size_t L) {
    double tot = 0.0;
    for (size_t i = 0; i < L; i++) {
        tot += (double)A[i];
    }
    return (uint16_t)(tot / L);
}

#endif

float ltc1865_read(ltc1865_channel_t channel) {
    if (ltc1865_select_channel(channel) == 0) {
        return -1;
    }
    uint16_t cval = ltc1865_spi_rcv();
    if (cval == -1) {
        return -1;
    }
    ltc1865_raw_values[channel] = (cval << 8) | (cval >> 8);
    uint16_t current_raw_value, current_filtered_value;
    current_raw_value = ltc1865_raw_values[channel];

#if ACQUISINATORE_FILTER_TYPE == ACQUISINATORE_NO_FILTER
    current_filtered_value = current_raw_value;
#elif ACQUISINATORE_FILTER_TYPE == ACQUISINATORE_SIMPLE_MOVING_AVG
    acquisinatore_mov_avg_window[channel][acquisinatore_mov_avg_window_idx[channel]] = current_raw_value;
    acquisinatore_mov_avg_window_idx[channel]                                        = (acquisinatore_mov_avg_window_idx[channel] + 1) %
                                                ACQUISINATORE_SIMPLE_MOVING_AVG_KERNEL_SIZE;
    current_filtered_value = moving_avg(acquisinatore_mov_avg_window[channel], ACQUISINATORE_SIMPLE_MOVING_AVG_KERNEL_SIZE);
#elif ACQUISINATORE_FILTER_TYPE == ACQUISINATORE_GAUSSIAN_FILTER
#error Gaussian filter not implemented
#elif ACQUISINATORE_FILTER_TYPE == ACQUISINATORE_OLD_MOVING_AVG
#error Old moving avg is deprecated, please use ACQUISINATORE_SIMPLE_MOVING_AVG
#endif

    ltc1865_raw_filtered_values[channel] = current_filtered_value;
    ltc1865_values_in_V[channel]         = (ACQUISINATORE_VREF_INT * ((float)ltc1865_raw_filtered_values[channel])) / 65536.0f;
    return ltc1865_values_in_V[channel];
}

#define LTC1865_SPI_TIMEOUT_LOOPS (3000U)  // ~250 us at 72 MHz: a byte takes ~28 us at 281 kHz, the sampling period is 667 us

// Exchanges one byte by polling the registers: safe inside an interrupt, where HAL_GetTick() does
// not advance and the timeouts of the HAL blocking functions would never expire.
static int ltc1865_spi_byte(uint8_t tx, uint8_t *rx) {
    uint32_t loops = 0;
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {
        if (++loops > LTC1865_SPI_TIMEOUT_LOOPS) {
            return -1;
        }
    }
    *(__IO uint8_t *)&SPI1->DR = tx;  // 8 bit access: a 16 bit write would push two bytes
    loops = 0;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U) {
        if (++loops > LTC1865_SPI_TIMEOUT_LOOPS) {
            return -1;
        }
    }
    *rx = *(__IO uint8_t *)&SPI1->DR;
    return 0;
}

/**
 * One LTC1865 transaction, without any wait: meant to be called at a fixed rate (TIM2).
 * CS low reads the conversion started by the previous CS rising edge and shifts in the channel of
 * the next one; CS is left high, so the next conversion starts now and has a whole period to end.
 * Returns 0 and the code, or -1 on SPI timeout (the code is then not valid).
 */
int ltc1865_transfer(ltc1865_channel_t next_channel, uint16_t *code) {
    uint8_t config = ltc1865_channel_selection_bits(next_channel);
    uint8_t msb = 0, lsb = 0;

    SET_BIT(SPI1->CR2, SPI_CR2_FRXTH);  // RXNE on every byte (the HAL changes it in its transfers)
    __HAL_SPI_ENABLE(&hspi1);
    while ((SPI1->SR & SPI_SR_FRLVL) != 0U) {
        (void)*(__IO uint8_t *)&SPI1->DR;  // drop bytes left in the RX FIFO
    }

    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);
    int res = ltc1865_spi_byte(config, &msb);
    if (res == 0) {
        res = ltc1865_spi_byte(config, &lsb);
    }
    uint32_t loops = 0;
    while ((SPI1->SR & SPI_SR_BSY) != 0U && ++loops <= LTC1865_SPI_TIMEOUT_LOOPS) {
    }
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET);  // starts the next conversion

    *code = (uint16_t)((msb << 8) | lsb);
    return res;
}

/* USER CODE END 0 */

SPI_HandleTypeDef hspi1;

/* SPI1 init function */
void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(spiHandle->Instance==SPI1)
  {
  /* USER CODE BEGIN SPI1_MspInit 0 */

  /* USER CODE END SPI1_MspInit 0 */
    /* SPI1 clock enable */
    __HAL_RCC_SPI1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**SPI1 GPIO Configuration
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN SPI1_MspInit 1 */

  /* USER CODE END SPI1_MspInit 1 */
  }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
{

  if(spiHandle->Instance==SPI1)
  {
  /* USER CODE BEGIN SPI1_MspDeInit 0 */

  /* USER CODE END SPI1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_SPI1_CLK_DISABLE();

    /**SPI1 GPIO Configuration
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);

  /* USER CODE BEGIN SPI1_MspDeInit 1 */

  /* USER CODE END SPI1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
