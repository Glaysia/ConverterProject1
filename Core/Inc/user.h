/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user.h
  * @brief   Helper APIs for application specific PWM control.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef USER_H
#define USER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

HAL_StatusTypeDef pwm_set_frequency(TIM_HandleTypeDef *htim, uint32_t frequency_hz);
HAL_StatusTypeDef pwm_set_dutycycle(TIM_HandleTypeDef *htim, uint32_t channel, float duty_cycle_percent);
HAL_StatusTypeDef pwm_set_deadtime(TIM_HandleTypeDef *htim, float deadtime_pct);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
