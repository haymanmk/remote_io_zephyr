/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DI_2_Pin GPIO_PIN_2
#define DI_2_GPIO_Port GPIOE
#define DI_3_Pin GPIO_PIN_3
#define DI_3_GPIO_Port GPIOE
#define DI_4_Pin GPIO_PIN_4
#define DI_4_GPIO_Port GPIOE
#define DI_5_Pin GPIO_PIN_5
#define DI_5_GPIO_Port GPIOE
#define DI_6_Pin GPIO_PIN_6
#define DI_6_GPIO_Port GPIOE
#define USER_Btn_Pin GPIO_PIN_13
#define USER_Btn_GPIO_Port GPIOC
#define USER_Btn_EXTI_IRQn EXTI15_10_IRQn
#define DI_10_Pin GPIO_PIN_0
#define DI_10_GPIO_Port GPIOF
#define DI_11_Pin GPIO_PIN_1
#define DI_11_GPIO_Port GPIOF
#define DI_12_Pin GPIO_PIN_2
#define DI_12_GPIO_Port GPIOF
#define DI_13_Pin GPIO_PIN_4
#define DI_13_GPIO_Port GPIOF
#define DI_14_Pin GPIO_PIN_7
#define DI_14_GPIO_Port GPIOF
#define DI_15_Pin GPIO_PIN_8
#define DI_15_GPIO_Port GPIOF
#define DI_16_Pin GPIO_PIN_9
#define DI_16_GPIO_Port GPIOF
#define MCO_Pin GPIO_PIN_0
#define MCO_GPIO_Port GPIOH
#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define RMII_REF_CLK_Pin GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port GPIOA
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define PWM_WS28XX_CH1_Pin GPIO_PIN_6
#define PWM_WS28XX_CH1_GPIO_Port GPIOA
#define RMII_CRS_DV_Pin GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
#define LD1_Pin GPIO_PIN_0
#define LD1_GPIO_Port GPIOB
#define DI_7_Pin GPIO_PIN_7
#define DI_7_GPIO_Port GPIOE
#define DI_8_Pin GPIO_PIN_8
#define DI_8_GPIO_Port GPIOE
#define DI_9_Pin GPIO_PIN_10
#define DI_9_GPIO_Port GPIOE
#define RMII_TXD1_Pin GPIO_PIN_13
#define RMII_TXD1_GPIO_Port GPIOB
#define LD3_Pin GPIO_PIN_14
#define LD3_GPIO_Port GPIOB
#define STLK_RX_Pin GPIO_PIN_8
#define STLK_RX_GPIO_Port GPIOD
#define STLK_TX_Pin GPIO_PIN_9
#define STLK_TX_GPIO_Port GPIOD
#define DO_14_Pin GPIO_PIN_11
#define DO_14_GPIO_Port GPIOD
#define DO_15_Pin GPIO_PIN_12
#define DO_15_GPIO_Port GPIOD
#define DO_16_Pin GPIO_PIN_13
#define DO_16_GPIO_Port GPIOD
#define USB_PowerSwitchOn_Pin GPIO_PIN_6
#define USB_PowerSwitchOn_GPIO_Port GPIOG
#define USB_OverCurrent_Pin GPIO_PIN_7
#define USB_OverCurrent_GPIO_Port GPIOG
#define DO_1_Pin GPIO_PIN_7
#define DO_1_GPIO_Port GPIOC
#define DO_2_Pin GPIO_PIN_8
#define DO_2_GPIO_Port GPIOC
#define DO_3_Pin GPIO_PIN_9
#define DO_3_GPIO_Port GPIOC
#define USB_SOF_Pin GPIO_PIN_8
#define USB_SOF_GPIO_Port GPIOA
#define USB_VBUS_Pin GPIO_PIN_9
#define USB_VBUS_GPIO_Port GPIOA
#define USB_ID_Pin GPIO_PIN_10
#define USB_ID_GPIO_Port GPIOA
#define USB_DM_Pin GPIO_PIN_11
#define USB_DM_GPIO_Port GPIOA
#define USB_DP_Pin GPIO_PIN_12
#define USB_DP_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define DO_4_Pin GPIO_PIN_10
#define DO_4_GPIO_Port GPIOC
#define DO_5_Pin GPIO_PIN_11
#define DO_5_GPIO_Port GPIOC
#define DO_6_Pin GPIO_PIN_12
#define DO_6_GPIO_Port GPIOC
#define DO_7_Pin GPIO_PIN_0
#define DO_7_GPIO_Port GPIOD
#define DO_8_Pin GPIO_PIN_1
#define DO_8_GPIO_Port GPIOD
#define DO_9_Pin GPIO_PIN_2
#define DO_9_GPIO_Port GPIOD
#define DO_10_Pin GPIO_PIN_3
#define DO_10_GPIO_Port GPIOD
#define DO_11_Pin GPIO_PIN_4
#define DO_11_GPIO_Port GPIOD
#define DO_12_Pin GPIO_PIN_6
#define DO_12_GPIO_Port GPIOD
#define DO_13_Pin GPIO_PIN_7
#define DO_13_GPIO_Port GPIOD
#define RMII_TX_EN_Pin GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port GPIOG
#define RMII_TXD0_Pin GPIO_PIN_13
#define RMII_TXD0_GPIO_Port GPIOG
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define LD2_Pin GPIO_PIN_7
#define LD2_GPIO_Port GPIOB
#define DI_1_Pin GPIO_PIN_0
#define DI_1_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
