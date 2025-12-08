//
// Common Harry helper hooks
//

#include "Harry.h"
#include "PWM.h"
#include "main.h"

#include <stdint.h>

static float getOutputVoltage(void);
static float clampf(float value, float min_v, float max_v);

/* Divider: sig -> 20k -> tap -> 36k -> gnd. Scale raw counts to source voltage. */
#define ADC_ERROR_CFACTOR (1.02f)
#define ADC_ERROR_VFACTOR (0.0f)
#define ADC_SCALE_FACTOR  (0.0012529058f)*(ADC_ERROR_CFACTOR)+(ADC_ERROR_VFACTOR)  /* 3.3 V * (20 + 36) / 36 / 4096 */
#define ADC_AVG_WINDOW    (200U)
/* PI control targets (voltage reference 4 V, freq min/max in Hz). */
#define CTRL_TARGET_VOLTS   (5.0f)
#define CTRL_FREQ_MIN_HZ    (24800.0f)
#define CTRL_FREQ_MAX_HZ    (100000.0f)
#define CTRL_KP             (60000.0f)
#define CTRL_KI             (50000.0f)
#define CTRL_LOOP_DT_SEC    (0.001f) /* TIM2 tick ~1 kHz */

extern "C" {

/* Global pointers backing the printf/ADC helper hooks. */
static UART_HandleTypeDef *g_harry_uart = NULL;
static ADC_HandleTypeDef *g_harry_adc = NULL;
extern TIM_HandleTypeDef htim2;
volatile float g_adc_scaled_average = 0.0f;

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
void harryADCInit(ADC_HandleTypeDef *hadc1, uint16_t adc_dma_buffer[], uint32_t adc_dma_buf_len)
{
    HAL_TIM_Base_Start_IT(&htim2);
    g_harry_adc = hadc1;
    if (HAL_ADC_Start_DMA(hadc1, (uint32_t *)adc_dma_buffer, adc_dma_buf_len) != HAL_OK)
    {
        Error_Handler();
    }
}

/* Update EMA of the latest ADC sample coming from the DMA ring. */
static void harryUpdateAdcAverage(void)
{
    if ((g_harry_adc == NULL) || (g_harry_adc->DMA_Handle == NULL)) {
        return;
    }

    static float ema = 0.0f; /* simple IIR as an O(1) moving-average approximation */

    uint32_t remaining = __HAL_DMA_GET_COUNTER(g_harry_adc->DMA_Handle);
    uint32_t write_idx = (ADC_DMA_BUF_LEN - remaining) % ADC_DMA_BUF_LEN;
    uint32_t latest_idx = (write_idx + ADC_DMA_BUF_LEN - 1U) % ADC_DMA_BUF_LEN;

    const float sample = (float)adc_dma_buffer[latest_idx];
    ema += (sample - ema) * (1.0f / (float)ADC_AVG_WINDOW);
    g_adc_scaled_average = ema * ADC_SCALE_FACTOR;
}

/* Run a simple PI controller that lowers frequency when voltage is low. */
static void harryRunPiControl(void)
{
    static float integrator = 0.0f;
    static uint8_t initialized = 0U;

    PWM *pwm0 = &global_pwms[0];
    if (pwm0 == NULL) {
        return;
    }

    float freq_cmd = (CTRL_FREQ_MIN_HZ + CTRL_FREQ_MAX_HZ) * 0.5f;
    if (initialized == 0U) {
        integrator = 0.0f;
        initialized = 1U;
    }

    const float error = CTRL_TARGET_VOLTS - g_adc_scaled_average;
    integrator += error * CTRL_KI * CTRL_LOOP_DT_SEC;
    freq_cmd -= (CTRL_KP * error) + integrator;

    /* Anti-windup: bleed the integrator when we hit the rails so we can recover. */
    const float limited_freq = clampf(freq_cmd, CTRL_FREQ_MIN_HZ, CTRL_FREQ_MAX_HZ);
    if (limited_freq != freq_cmd) {
        integrator += (freq_cmd - limited_freq);
    }

    pwm0->setFrequency((uint32_t)limited_freq);
    pwm0->setDutyCycle((float)50.0f);
    pwm0->setDeadtime((float)2.5f);
}

/* IRQ hook fired by HAL when the PWM timer rolls over. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PWM *pwm0 = &global_pwms[0];
    if (htim == pwm0->htim) {
        if (pwm0->newStatus != pwm0->oldStatus){
            pwm0->PwmUpdate();
        }
        // getOutputVoltage()
    }

    if (htim == &htim2) {
        harryUpdateAdcAverage();
        harryRunPiControl();
    }
}

}

/* Initialize PWM helper 0 using its internal defaults. */
void harryPwmInit(TIM_HandleTypeDef *htim)
{
    PWM *pwm0 = &global_pwms[0];
    pwm0->PwmInit(
        htim,
        51000u,
        50.0f,
        2.5f
    );
}


static float clampf(float value, float min_v, float max_v)
{
    if (value < min_v) {
        return min_v;
    }
    if (value > max_v) {
        return max_v;
    }
    return value;
}
