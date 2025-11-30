//
// Created by harry on 25. 12. 1..
//

#include "PWM.h"
#include "stm32f7xx_hal_tim.h"

PWM global_pwms[1];

PWM::PWM() {
    htim = nullptr;
}

uint32_t PWM::Harry_GetTimerClock() {
    TIM_HandleTypeDef *handle = this->htim;
    
    if (handle == nullptr) {
        return 0U;
    }

    uint32_t clock;
    if ((handle->Instance == TIM1) || (handle->Instance == TIM8) ||
        (handle->Instance == TIM9) || (handle->Instance == TIM10) ||
        (handle->Instance == TIM11)) {
        clock = HAL_RCC_GetPCLK2Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1) {
            clock *= 2U;
        }
    } else {
        clock = HAL_RCC_GetPCLK1Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
            clock *= 2U;
        }
    }

    return clock;
}

void PWM::PwmUpdate()
{
    if ((htim == nullptr) || (status.freq_hz == 0U)) {
        return;
    }

    uint32_t tim_clk = this->Harry_GetTimerClock();
    if (tim_clk == 0U) {
        return;
    }

    uint32_t ticks_per_period = tim_clk / status.freq_hz;
    if (ticks_per_period == 0U) {
        ticks_per_period = 1U;
    }

    uint32_t auto_reload = ticks_per_period - 1U;
    htim->Init.Period = auto_reload;
    __HAL_TIM_SET_AUTORELOAD(htim, auto_reload);

    float duty = status.duty_pct;
    if (duty < 0.0f) {
        duty = 0.0f;
    } else if (duty > 100.0f) {
        duty = 100.0f;
    }

    uint32_t pulse = (uint32_t)(((float)(auto_reload + 1U) * duty) / 100.0f);
    if (pulse > auto_reload) {
        pulse = auto_reload;
    }
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, pulse);

    float deadtime = status.deadtime_pct;
    if (deadtime < 0.0f) {
        deadtime = 0.0f;
    } else if (deadtime > 100.0f) {
        deadtime = 100.0f;
    }

    if (IS_TIM_ADVANCED_INSTANCE(htim->Instance)) {
        uint32_t deadtime_ticks = (uint32_t)(((float)(auto_reload + 1U) * deadtime) / 100.0f);
        if (deadtime_ticks > 0xFFU) {
            deadtime_ticks = 0xFFU;
        }

        MODIFY_REG(htim->Instance->BDTR, TIM_BDTR_DTG, deadtime_ticks);
    }

}
void PWM::PwmInit(TIM_HandleTypeDef *htim, uint32_t freq_hz, float duty_pct, float deadtime_pct)
{
    this->htim = htim;
    this->status.freq_hz = freq_hz;
    this->status.duty_pct = duty_pct;
    this->status.deadtime_pct = deadtime_pct;
    this->PwmUpdate();

    HAL_TIM_Base_Init(this->htim);
    HAL_TIM_PWM_Init(this->htim);
    this->restartPwm();
}
