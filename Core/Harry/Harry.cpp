//
// Common Harry helper hooks
//

#include "Harry.h"
#include "PWM.h"
#include "main.h"

#include <stdint.h>

extern "C" {

/* Global pointers backing the printf/ADC helper hooks. */
static UART_HandleTypeDef *g_harry_uart = NULL;
static ADC_HandleTypeDef *g_harry_adc = NULL;

/* Remember which UART transports debug prints. */
void harryIOInit(UART_HandleTypeDef *huart)
{
    g_harry_uart = huart;
}

/* Blocking printf backend used by syscalls.c hooks. */
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

/* C stdio wrapper that keeps standard behavior intact. */
int putchar(int ch)
{
    return __io_putchar(ch);
}

/* Store the ADC instance for later helper routines or callbacks. */
void harryADCInit(ADC_HandleTypeDef *hadc1, uint16_t adc_dma_buffer[], uint32_t ADC_DMA_BUF_LEN)
{
    g_harry_adc = hadc1;

    if (HAL_ADC_Start_DMA(hadc1, (uint32_t *)adc_dma_buffer, ADC_DMA_BUF_LEN) != HAL_OK)
    {
        Error_Handler();
    }
}

/* IRQ hook fired by HAL when the PWM timer rolls over. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PWM *pwm0 = &global_pwms[0];
    if (htim == pwm0->htim) {
        if (pwm0->newStatus != pwm0->oldStatus){
            pwm0->PwmUpdate();
            pwm0->restartPwm();
        }
        // HAL_ADC_Start_IT(g_harry_adc);
    }
}

}

/* Initialize PWM helper 0 using its internal defaults. */
void harryPwmInit(TIM_HandleTypeDef *htim)
{
    PWM *pwm0 = &global_pwms[0];
    pwm0->PwmInit(htim);
}
