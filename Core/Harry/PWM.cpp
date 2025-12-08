//
// Created by harry on 25. 12. 1..
//

#include "PWM.h"
#include "main.h"

PWM global_pwms[1];

PWM::PWM() {
    htim = nullptr;
    newStatus.freq_hz = 0U;
    newStatus.duty_pct = 0.0f;
    newStatus.deadtime_pct = 0.0f;
    newStatus.freq_hz = 0U;
    newStatus.duty_pct = 0.0f;
    newStatus.deadtime_pct = 0.0f;
}

uint32_t PWM::Harry_GetTimerClock() const {
    TIM_HandleTypeDef *handle = this->htim;
    
    if (handle == nullptr) {
        return 0U;
    }

    uint32_t clock;
    if (handle->Instance == TIM1 ) {
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
    if(this->newStatus.freq_hz<3500){
        Error_Handler();
    }
    if ((htim == nullptr) || (newStatus.freq_hz == 0U)) {
        return;
    }

    uint32_t tim_clk = this->Harry_GetTimerClock();
    if (tim_clk == 0U) {
        return;
    }

    uint32_t ticks_per_period = tim_clk / newStatus.freq_hz;
    if (ticks_per_period == 0U) {
        ticks_per_period = 1U;
    }

    uint32_t auto_reload = ticks_per_period - 1U;
    htim->Init.Period = auto_reload;
    __HAL_TIM_SET_AUTORELOAD(htim, auto_reload);

    float duty = newStatus.duty_pct;
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

    float deadtime = newStatus.deadtime_pct;
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

    /* Keep cached status in sync so HAL callbacks avoid stop/start overhead. */
    this->oldStatus = this->getPwmStatusFromRegister();
    this->newStatus = this->oldStatus;
}

PwmStatus PWM::getPwmStatusFromRegister() const
{
    PwmStatus result = {0U, 0.0f, 0.0f};

    TIM_HandleTypeDef *handle = this->htim;
    if (handle == nullptr) {
        return result;
    }

    uint32_t auto_reload = handle->Instance->ARR;
    uint32_t period_ticks = auto_reload + 1U;
    if (period_ticks == 0U) {
        period_ticks = 0xFFFFFFFFU;
    }

    uint32_t tim_clk = this->Harry_GetTimerClock();
    if ((tim_clk != 0U) && (period_ticks != 0U)) {
        result.freq_hz = tim_clk / period_ticks;
    }

    uint32_t pulse = handle->Instance->CCR1;
    if (period_ticks != 0U) {
        float duty = ((float)pulse * 100.0f) / (float)period_ticks;
        if (duty > 100.0f) {
            duty = 100.0f;
        }
        result.duty_pct = duty;
    }

    if (IS_TIM_ADVANCED_INSTANCE(handle->Instance) && (period_ticks != 0U)) {
        uint32_t deadtime_ticks = handle->Instance->BDTR & TIM_BDTR_DTG;
        float deadtime = ((float)deadtime_ticks * 100.0f) / (float)period_ticks;
        if (deadtime > 100.0f) {
            deadtime = 100.0f;
        }
        result.deadtime_pct = deadtime;
    }

    return result;
}

void PWM::PwmInit(TIM_HandleTypeDef *htim, uint32_t freq_hz, float duty_pct, float deadtime_pct)
{
    this->htim = htim;
    this->newStatus.freq_hz = freq_hz;
    this->newStatus.duty_pct = duty_pct;
    this->newStatus.deadtime_pct = deadtime_pct;
    this->oldStatus = this->newStatus;

    HAL_TIM_Base_Init(this->htim);
    HAL_TIM_PWM_Init(this->htim);
    HAL_TIM_Base_Start_IT(this->htim);
    HAL_TIM_PWM_Start_IT(this->htim, TIM_CHANNEL_1);
    if (IS_TIM_ADVANCED_INSTANCE(this->htim->Instance)) {
        HAL_TIMEx_PWMN_Start_IT(this->htim, TIM_CHANNEL_1);
    }

    this->PwmUpdate();
}

void PWM::setFrequency(uint32_t freq_hz)
{
    this->newStatus.freq_hz = freq_hz;
}

void PWM::setDutyCycle(float duty_pct)
{
    this->newStatus.duty_pct = duty_pct;
}

void PWM::setDeadtime(float deadtime_pct)
{
    this->newStatus.deadtime_pct = deadtime_pct;
}
