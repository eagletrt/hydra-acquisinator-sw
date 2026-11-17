/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "can.h"

/* USER CODE BEGIN 0 */
#include "can-communications-api.h"
/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN;
  hcan.Init.Prescaler = 2;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN)
  {
  /* USER CODE BEGIN CAN_MspInit 0 */

  /* USER CODE END CAN_MspInit 0 */
    /* CAN clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN interrupt Init */
    HAL_NVIC_SetPriority(CAN_TX_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CAN_TX_IRQn);
    HAL_NVIC_SetPriority(CAN_RX0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CAN_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN_RX1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CAN_RX1_IRQn);
  /* USER CODE BEGIN CAN_MspInit 1 */

  /* USER CODE END CAN_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN)
  {
  /* USER CODE BEGIN CAN_MspDeInit 0 */

  /* USER CODE END CAN_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN_TX_IRQn);
    HAL_NVIC_DisableIRQ(CAN_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN_RX1_IRQn);
  /* USER CODE BEGIN CAN_MspDeInit 1 */

  /* USER CODE END CAN_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/*!
 * \brief Accept every standard frame into FIFO0, start the peripheral and
 *     enable the RX interrupt. Call after can_communications_api_init.
 */
void can_start(void) {
  const CAN_FilterTypeDef filter = {
    .FilterBank = 0U,
    .FilterMode = CAN_FILTERMODE_IDMASK,
    .FilterScale = CAN_FILTERSCALE_32BIT,
    .FilterIdHigh = 0U,
    .FilterIdLow = 0U,
    .FilterMaskIdHigh = 0U,
    .FilterMaskIdLow = 0U,
    .FilterFIFOAssignment = CAN_FILTER_FIFO0,
    .FilterActivation = CAN_FILTER_ENABLE,
    .SlaveStartFilterBank = 14U,
  };
  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_CAN_Start(&hcan) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    Error_Handler();
  }
}

/*!
 * \brief can-communications send callback.
 *
 * \details bxCAN has only 3 TX mailboxes and PAL drops a frame whose send
 *     fails, so a full mailbox set is reported as a transmission error.
 */
enum CanCommunicationReturnCode can_send(const struct CanCommunicationFrame *frame) {
  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U) {
    return CAN_COMMUNICATION_RC_TRANSMISSION_ERROR;
  }

  const CAN_TxHeaderTypeDef header = {
    .StdId = frame->id,
    .IDE = CAN_ID_STD,
    .RTR = CAN_RTR_DATA,
    .DLC = frame->length,
    .TransmitGlobalTime = DISABLE,
  };
  uint32_t mailbox = 0U;
  // HAL takes a non-const data buffer but never writes to it
  if (HAL_CAN_AddTxMessage(&hcan, &header, (uint8_t *)frame->data, &mailbox) != HAL_OK) {
    return CAN_COMMUNICATION_RC_TRANSMISSION_ERROR;
  }
  return CAN_COMMUNICATION_RC_OK;
}

/*!
 * \brief can-communications receive callback: nothing to receive yet.
 */
enum CanCommunicationReturnCode can_on_receive(const struct CanCommunicationFrame *frame) {
  (void)frame;
  return CAN_COMMUNICATION_RC_OK;
}

/*!
 * \brief RX ISR: move every pending frame into the can-communications RX queue.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_isr) {
  CAN_RxHeaderTypeDef header = { 0 };
  struct CanCommunicationFrame frame = { 0 };
  while (HAL_CAN_GetRxFifoFillLevel(hcan_isr, CAN_RX_FIFO0) > 0U) {
    if (HAL_CAN_GetRxMessage(hcan_isr, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {
      return;
    }
    if (header.IDE != CAN_ID_STD || header.RTR != CAN_RTR_DATA) {
      continue;
    }
    frame.id = header.StdId;
    frame.length = (uint8_t)header.DLC;
    (void)can_communications_api_add_to_rx_buffer(&frame);
  }
}

/* USER CODE END 1 */

