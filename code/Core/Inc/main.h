/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_HEARTBEAT_PC13_Pin GPIO_PIN_13
#define LED_HEARTBEAT_PC13_GPIO_Port GPIOC
#define PWM_PUMP_TIM2_CH1_Pin GPIO_PIN_0
#define PWM_PUMP_TIM2_CH1_GPIO_Port GPIOA
#define KEY_K1_Pin GPIO_PIN_1
#define KEY_K1_GPIO_Port GPIOA
#define KEY_K2_Pin GPIO_PIN_2
#define KEY_K2_GPIO_Port GPIOA
#define KEY_K3_Pin GPIO_PIN_3
#define KEY_K3_GPIO_Port GPIOA
#define KEY_K4_Pin GPIO_PIN_4
#define KEY_K4_GPIO_Port GPIOA
#define MOSFET_RSV_PB0_Pin GPIO_PIN_0
#define MOSFET_RSV_PB0_GPIO_Port GPIOB
#define PUMP_EN_PB1_Pin GPIO_PIN_1
#define PUMP_EN_PB1_GPIO_Port GPIOB
#define VALVE_Z1_PB12_Pin GPIO_PIN_12
#define VALVE_Z1_PB12_GPIO_Port GPIOB
#define VALVE_Z2_PB13_Pin GPIO_PIN_13
#define VALVE_Z2_PB13_GPIO_Port GPIOB
#define VALVE_Z3_PB14_Pin GPIO_PIN_14
#define VALVE_Z3_PB14_GPIO_Port GPIOB
#define VALVE_Z4_PB15_Pin GPIO_PIN_15
#define VALVE_Z4_PB15_GPIO_Port GPIOB
#define VALVE_Z5_RSV_PA8_Pin GPIO_PIN_8
#define VALVE_Z5_RSV_PA8_GPIO_Port GPIOA
#define USART1_TX_LOG_Pin GPIO_PIN_9
#define USART1_TX_LOG_GPIO_Port GPIOA
#define USART1_RX_CMD_Pin GPIO_PIN_10
#define USART1_RX_CMD_GPIO_Port GPIOA
#define I2C1_SCL_OLED_EEP_Pin GPIO_PIN_6
#define I2C1_SCL_OLED_EEP_GPIO_Port GPIOB
#define I2C1_SDA_OLED_EEP_Pin GPIO_PIN_7
#define I2C1_SDA_OLED_EEP_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
