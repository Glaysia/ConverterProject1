#include "interrupt_dma.h"
#include "main.h"
#include "harry.h"
#include "stm32h533xx.h"
#include <stdint.h>

/* main.c에 정의된 전역 변수들을 참조 */
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim1;

extern volatile uint16_t g_adc_last;
extern uint16_t PC13_Counter;
extern uint16_t PWM_Counter;
extern uint16_t DAC_Counter;
extern uint16_t dac_value;
extern uint32_t g_tim1_pwm_freq_hz;
extern uint32_t g_tim1_deadtime_percent;


/**
 * @brief ADC 변환 완료 인터럽트 콜백
 *
 * - ADC1에서 변환이 완료되면 HAL이 자동으로 이 함수를 호출합니다.
 * - 최신 변환 값을 g_adc_last에 저장한 뒤,
 *   DAC CH2로 그대로 전달하여 입력 전압을 아날로그 출력으로 복사합니다.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        g_adc_last = (uint16_t)HAL_ADC_GetValue(hadc);
        HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, g_adc_last);
    }
}

/**
 * @brief 타이머 주기 경과(TIM Update) 인터럽트 콜백
 *
 * - TIM1의 카운터가 주기를 채우고 갱신 이벤트가 발생할 때 호출됩니다.
 * - 각 주기마다 ADC 단발 변환을 한 번 트리거하고,
 *   PWM_Counter를 증가시켜 주기 수를 소프트웨어에서 추적할 수 있게 합니다.
 * - 주석 처리된 코드 블록을 활용하면,
 *   주기마다 사인파를 생성해 DAC로 출력하거나, 디버깅 메시지를 출력할 수 있습니다.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        HAL_ADC_Start_IT(&hadc1);
        PWM_Counter++;
        uint32_t before_freq_hz = TIM1_GetFrequencyHz();
        if (before_freq_hz != g_tim1_pwm_freq_hz){
            TIM1_SetFrequencyHz(g_tim1_pwm_freq_hz);
        }
        uint32_t before_deadtime_percent = TIM1_GetDeadtimePercent();
        if (before_deadtime_percent != g_tim1_deadtime_percent){
            TIM1_SetDeadtimePercent(g_tim1_deadtime_percent);
        }
    }
}

/**
 * @brief GPIO EXTI Rising Edge 인터럽트 콜백
 *
 * - 외부 인터럽트 라인에서 상승 에지가 감지되면 호출됩니다.
 * - 여기서는 사용자 스위치가 연결된 C13_SWITCH_Pin에 대해서만 동작하며,
 *   버튼을 누를 때마다 PC13_Counter를 증가시킵니다.
 * - 주석 처리된 코드를 이용하면 버튼을 누를 때마다
 *   PWM 주파수를 증가시키는 등의 기능을 쉽게 추가할 수 있습니다.
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == C13_SWITCH_Pin)
    {
        PC13_Counter++;

        /* Increase PWM frequency by 10 Hz each press (example) */
        // g_tim1_pwm_freq_hz += 100U;
        // TIM1_SetFrequencyHz(g_tim1_pwm_freq_hz);

        // enable_printf = !enable_printf;
    }
}
