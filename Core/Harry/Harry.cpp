//
// Common Harry helper hooks
//

#include "Harry.h"
#include "PWM.h"
#include "main.h"

#include <stdint.h>

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
        Error_Handler();
        return 0;
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


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PWM pwm1 = global_pwms[0];
    if (htim == pwm1.htim) {
        // HAL_ADC_Start_IT(g_harry_adc);
    }
}

}

void harryPwmInit(TIM_HandleTypeDef *htim)
{
    PWM *pwm0 = &global_pwms[0];
    pwm0->PwmInit(htim);
}
