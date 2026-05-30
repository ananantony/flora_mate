/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level -- PC13 heartbeat LED (active low, default off = high) */
  HAL_GPIO_WritePin(LED_HEARTBEAT_PC13_GPIO_Port, LED_HEARTBEAT_PC13_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level -- GPIOB: valves Z1-Z4, pump enable, spare MOSFET (default low = off) */
  HAL_GPIO_WritePin(GPIOB, VALVE_Z1_PB12_Pin | VALVE_Z2_PB13_Pin | VALVE_Z3_PB14_Pin | VALVE_Z4_PB15_Pin
                           | PUMP_EN_PB1_Pin | MOSFET_RSV_PB0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level -- GPIOA: valve Z5 spare (default low = off) */
  HAL_GPIO_WritePin(VALVE_Z5_RSV_PA8_GPIO_Port, VALVE_Z5_RSV_PA8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_HEARTBEAT_PC13_Pin */
  GPIO_InitStruct.Pin = LED_HEARTBEAT_PC13_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_HEARTBEAT_PC13_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_K1_Pin KEY_K2_Pin KEY_K3_Pin KEY_K4_Pin (input with pull-up) */
  GPIO_InitStruct.Pin = KEY_K1_Pin | KEY_K2_Pin | KEY_K3_Pin | KEY_K4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : VALVE_Z1..Z4, PUMP_EN, MOSFET_RSV on GPIOB (push-pull output, low by default) */
  GPIO_InitStruct.Pin = VALVE_Z1_PB12_Pin | VALVE_Z2_PB13_Pin | VALVE_Z3_PB14_Pin | VALVE_Z4_PB15_Pin
                        | PUMP_EN_PB1_Pin | MOSFET_RSV_PB0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : VALVE_Z5_RSV_PA8 -- spare valve on PA8, push-pull output, low by default */
  GPIO_InitStruct.Pin = VALVE_Z5_RSV_PA8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(VALVE_Z5_RSV_PA8_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
