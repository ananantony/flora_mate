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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_HEARTBEAT_PC13_GPIO_Port, LED_HEARTBEAT_PC13_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, RLY_RSV_CH6_Pin|RLY_MAIN_CH5_Pin|RLY_VALVE_Z1_Pin|RLY_VALVE_Z2_Pin
                          |RLY_VALVE_Z3_Pin|RLY_VALVE_Z4_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_HEARTBEAT_PC13_Pin */
  GPIO_InitStruct.Pin = LED_HEARTBEAT_PC13_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_HEARTBEAT_PC13_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_K1_Pin KEY_K2_Pin KEY_K3_Pin KEY_K4_Pin
                           SENSOR_RSV_PA8_Pin */
  GPIO_InitStruct.Pin = KEY_K1_Pin|KEY_K2_Pin|KEY_K3_Pin|KEY_K4_Pin
                          |SENSOR_RSV_PA8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : RLY_RSV_CH6_Pin RLY_MAIN_CH5_Pin RLY_VALVE_Z1_Pin RLY_VALVE_Z2_Pin
                           RLY_VALVE_Z3_Pin RLY_VALVE_Z4_Pin */
  GPIO_InitStruct.Pin = RLY_RSV_CH6_Pin|RLY_MAIN_CH5_Pin|RLY_VALVE_Z1_Pin|RLY_VALVE_Z2_Pin
                          |RLY_VALVE_Z3_Pin|RLY_VALVE_Z4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
