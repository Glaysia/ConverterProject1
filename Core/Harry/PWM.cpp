//
// Created by harry on 25. 12. 1..
//

#include "PWM.h"
#include "stm32f7xx_hal_adc.h"
#include "stm32f7xx_hal_tim.h"

extern "C" {

static UART_HandleTypeDef *g_harry_uart = NULL;
static ADC_HandleTypeDef *g_harry_adc = NULL;

void harryIOInit(UART_HandleTypeDef *huart)
{
    g_harry_uart = huart;
}

int __io_putchar(int ch)
{
    if (g_harry_uart == NULL) {
        return ch;
    }

    uint8_t data = (uint8_t)ch;
    HAL_UART_Transmit(g_harry_uart, &data, 1, HAL_MAX_DELAY);
    return ch;
}

int putchar(int ch)
{
    return __io_putchar(ch);
}

void harryADCInit(ADC_HandleTypeDef *hadc1)
{
    g_harry_adc = hadc1;
}



}

PWM global_pwms[4];

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
    if ((htim == nullptr) || (freq_hz == 0U)) {
        return;
    }

    uint32_t tim_clk = this->Harry_GetTimerClock();
    if (tim_clk == 0U) {
        return;
    }

    uint32_t ticks_per_period = tim_clk / freq_hz;
    if (ticks_per_period == 0U) {
        ticks_per_period = 1U;
    }

    uint32_t auto_reload = ticks_per_period - 1U;
    htim->Init.Period = auto_reload;
    __HAL_TIM_SET_AUTORELOAD(htim, auto_reload);

    float duty = duty_pct;
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

    float deadtime = deadtime_pct;
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


void PWM::PwmInit(TIM_HandleTypeDef *htim, uint32_t freq_hz = 100000, float duty_pct = 49.0, float deadtime_pct=2.5) {
    this->htim = htim;
    this->freq_hz = freq_hz;
    this->duty_pct = duty_pct;
    this->deadtime_pct = deadtime_pct;
    this->PwmUpdate();

    HAL_TIM_Base_Init(this->htim);
    HAL_TIM_PWM_Init(this->htim);
    this->restartPwm();
}

void harryPwmInit(TIM_HandleTypeDef *htim)
{
    PWM *harry = &global_pwms[0];
    harry->PwmInit(htim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    PWM pwm1 = global_pwms[0];
    if (htim == pwm1.htim) {
        // HAL_ADC_Start_IT(g_harry_adc);
    }
}