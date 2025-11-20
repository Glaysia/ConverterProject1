#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C 코드에서 호출할 래퍼 함수 */
void Harry_printStudentId(void);

/**
 * @brief 사용자 초기화 루틴
 *
 * - HAL 초기화, 클럭/핀 초기화, 주변장치 인스턴스 생성이 끝난 뒤에 호출해야 합니다.
 *   (예: main.c에서 MX_GPIO_Init, MX_ADC1_Init, MX_DAC1_Init, MX_TIM1_Init 이후)
 * - DAC CH2를 0V로 시작하고, ADC를 인터럽트 모드로 시작합니다.
 * - TIM1의 PWM 주파수와 데드타임을 전역 변수(g_tim1_pwm_freq_hz, g_tim1_deadtime_percent)에 맞게 설정합니다.
 * - TIM1 타이머/PWM/N 채널 인터럽트를 모두 활성화합니다.
 *
 * @return true  모든 HAL/TIM1 초기화 단계가 정상적으로 완료됨
 * @return false 다음 중 하나라도 실패한 경우
 *              - HAL_DAC_Start / HAL_DAC_SetValue
 *              - HAL_ADC_Start_IT
 *              - HAL_TIM_Base_Start_IT / HAL_TIM_PWM_Start / HAL_TIMEx_PWMN_Start
 */
bool user_init(void);

/* TIM1 관련 유틸 함수들 (C에서도 사용) */
uint32_t TIM1_GetTimerClockHz(void);
uint32_t TIM1_GetFrequencyHz(void);
void TIM1_SetFrequencyHz(uint32_t freq_hz);
uint32_t TIM1_DeadtimeTicksToRegister(uint32_t ticks);

uint32_t TIM1_GetDeadtimePercent(void);
void TIM1_SetDeadtimePercent(uint32_t percent);

#ifdef __cplusplus
}
#endif
