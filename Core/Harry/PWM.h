//
// Created by harry on 25. 12. 1..
//

#ifndef POWER_HARRY_H
#define POWER_HARRY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>

void harryPwmInit(TIM_HandleTypeDef *htim);

void harryIOInit(UART_HandleTypeDef *huart);

void harryADCInit(ADC_HandleTypeDef *hadc1);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class PWM {
public:
    TIM_HandleTypeDef *htim;
    uint32_t freq_hz;
    float duty_pct;
    float deadtime_pct;

    PWM();
    uint32_t Harry_GetTimerClock();
    void PwmUpdate();
    void PwmInit(TIM_HandleTypeDef *htim, uint32_t freq_hz, float duty_pct, float deadtime_pct);
    void restartPwm(void)
    {   
        HAL_TIM_PWM_Stop_IT(this->htim, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Stop_IT(this->htim,TIM_CHANNEL_1);
        HAL_TIM_PWM_Start_IT(this->htim, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Start_IT(this->htim,TIM_CHANNEL_1);
    }

    void setFrequency(uint32_t freq_hz) {
        this->freq_hz = freq_hz;
        this->PwmUpdate();
        this->restartPwm();
    };
    void setDutyCycle(float duty_pct) {
        this->duty_pct = duty_pct;
        this->PwmUpdate();
        this->restartPwm();
    };
    void setDeadtime(float deadtime_pct) {
        this->deadtime_pct = deadtime_pct;
        this->PwmUpdate();
        this->restartPwm();
    };
};

extern PWM global_pwms[4];

#endif /* __cplusplus */

#endif //POWER_HARRY_H
