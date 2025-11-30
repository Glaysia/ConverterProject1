//
// Common Harry helper hooks
//

#ifndef POWER_HARRY_HARRY_H
#define POWER_HARRY_HARRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

void harryIOInit(UART_HandleTypeDef *huart);
int __io_putchar(int ch);
void harryADCInit(ADC_HandleTypeDef *hadc1, uint16_t* adc_dma_buffer, uint32_t adc_dma_buf_len);

#ifdef __cplusplus
}
#endif

void harryPwmInit(TIM_HandleTypeDef *htim);

#endif /* POWER_HARRY_HARRY_H */
