#include <cstdio>
#include "harry.h"
#include "main.h"

// cubeMX에서 생성한 외부 변수들
extern COM_InitTypeDef BspCOMInit;
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim1;

// main.c에 정의된 전역 변수들을 참조
extern uint16_t PC13_Counter;
extern uint16_t PWM_Counter;
extern uint16_t DAC_Counter;
extern uint16_t dac_value;
extern uint32_t g_tim1_pwm_freq_hz;
extern uint32_t g_tim1_deadtime_percent;

// static 변수: 이전 데드타임 퍼센트 저장용
static uint32_t tim1_deadtime_percent_prev;

/* C++ 전용 Harry 클래스: 내부에서만 사용 */
class Harry {
public:
    void printStudentId() const {
        std::printf("2021440107\n");
    }
};

/* C 코드와의 인터페이스를 위한 래퍼 함수 */
extern "C" void Harry_printStudentId(void) {
    Harry harry;
    harry.printStudentId();
}

extern "C" bool user_init(void) {
    /*
     * user_init
     *
     * 이 함수는 main.c 쪽에서 한 번만 호출되는
     * "사용자 정의 초기화 루틴"입니다.
     *
     * 호출 시점(권장):
     *   - HAL_Init()
     *   - SystemClock_Config()
     *   - MX_GPIO_Init(), MX_ADC1_Init(), MX_DAC1_Init(), MX_TIM1_Init()
     *   위 함수들이 모두 끝난 뒤, while(1) 루프에 들어가기 전에 호출하는 것을 가정합니다.
     *
     * 하는 일 요약:
     *   1) DAC CH2를 시작하고, 초기 출력 값을 0으로 설정
     *   2) ADC를 인터럽트 모드로 시작 (변환 완료 시 HAL_ADC_ConvCpltCallback에서 처리)
     *   3) TIM1의 PWM 주파수 / 데드타임을 전역 변수(g_tim1_pwm_freq_hz, g_tim1_deadtime_percent)에 맞게 설정
     *   4) TIM1 타이머 베이스, PWM, 보조 채널(PWMN)을 모두 시작
     *
     * 주의:
     *   - 이 함수는 HAL 반환값을 확인하여, 어느 단계에서든 실패하면 false를 반환합니다.
     */

    /* 1) DAC CH2 시작 및 초기 값 0으로 설정 */
    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_2) != HAL_OK) {
        return false;
    }
    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0) != HAL_OK) {
        return false;
    }

    /* 2) ADC를 인터럽트 모드로 시작 (단발 변환, 인터럽트마다 다시 시작) */
    if (HAL_ADC_Start_IT(&hadc1) != HAL_OK) {
        return false;
    }

    /* 3) TIM1 PWM 주파수 및 데드타임 설정 */
    TIM1_SetFrequencyHz(g_tim1_pwm_freq_hz);
    TIM1_SetDeadtimePercent(g_tim1_deadtime_percent);

    /* 4) TIM1 카운터/ PWM / 보조 PWM 채널 시작 */
    if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK) {
        return false;
    }
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        return false;
    }
    if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        return false;
    }

    /* 모든 단계가 정상적으로 완료된 경우에만 true 반환 */
    return true;
}

extern "C" uint32_t TIM1_GetTimerClockHz(void)
{
  RCC_ClkInitTypeDef clk_config = {0};
  uint32_t flash_latency = 0;
  HAL_RCC_GetClockConfig(&clk_config, &flash_latency);

  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  if (pclk2 == 0U) return 0U;
  return (clk_config.APB2CLKDivider == RCC_HCLK_DIV1) ? pclk2 : (pclk2 * 2U);
}
extern "C" uint32_t TIM1_GetFrequencyHz(void)
{
  uint32_t timer_clk_hz = TIM1_GetTimerClockHz();
  if (timer_clk_hz == 0U) return 0U;

  uint32_t factor =
      ((htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED1) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED2) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED3)) ? 2U : 1U;

  uint32_t presc_plus1 = htim1.Instance->PSC + 1U;
  uint32_t arr_plus1 = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;

  uint64_t denom = (uint64_t)factor * (uint64_t)presc_plus1 * (uint64_t)arr_plus1;
  if (denom == 0ULL) return 0U;

  uint32_t freq_hz = (uint32_t)((((uint64_t)timer_clk_hz * 1000ULL) + (denom / 2ULL)) / denom); /* rounded */
  return freq_hz/1000;
}

extern "C" void TIM1_SetFrequencyHz(uint32_t freq_hz)
{
  if (freq_hz == 0U) return;
  uint32_t timer_clk_hz = TIM1_GetTimerClockHz();
  if (timer_clk_hz == 0U) return;

  uint32_t factor =
      ((htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED1) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED2) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED3)) ? 2U : 1U;

  uint64_t denom_for_psc = (uint64_t)factor * 65536ULL * (uint64_t)freq_hz;
  uint64_t presc_plus1 = (denom_for_psc == 0ULL) ? 1ULL : ((uint64_t)timer_clk_hz + denom_for_psc - 1ULL) / denom_for_psc;
  if (presc_plus1 < 1ULL) presc_plus1 = 1ULL;
  if (presc_plus1 > 65536ULL) presc_plus1 = 65536ULL;

  uint64_t denom = (uint64_t)factor * presc_plus1 * (uint64_t)freq_hz;
  if (denom == 0ULL) return;
  uint64_t arr_plus1 = ((uint64_t)timer_clk_hz + (denom / 2ULL)) / denom; /* rounded */
  if (arr_plus1 < 1ULL) arr_plus1 = 1ULL;
  if (arr_plus1 > 65536ULL) arr_plus1 = 65536ULL;

  uint32_t was_enabled = (htim1.Instance->CR1 & TIM_CR1_CEN);
  if (was_enabled) __HAL_TIM_DISABLE(&htim1);

  __HAL_TIM_SET_PRESCALER(&htim1, (uint32_t)(presc_plus1 - 1ULL));
  __HAL_TIM_SET_AUTORELOAD(&htim1, (uint32_t)(arr_plus1 - 1ULL));
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(arr_plus1 / 2ULL)); /* ~50% duty */
  __HAL_TIM_SET_COUNTER(&htim1, 0U);
  htim1.Instance->EGR |= TIM_EGR_UG;

  TIM1_SetDeadtimePercent(g_tim1_deadtime_percent);

  if (was_enabled) __HAL_TIM_ENABLE(&htim1);
  g_tim1_pwm_freq_hz = TIM1_GetFrequencyHz();

}

extern "C" uint32_t TIM1_DeadtimeTicksToRegister(uint32_t ticks)
{
  if (ticks <= 127U)
  {
    return ticks;
  }
  if (ticks <= 254U)
  {
    uint32_t encoded = ((ticks + 1U) / 2U) + 64U;
    return (encoded > 191U) ? 191U : encoded;
  }
  if (ticks <= 504U)
  {
    uint32_t encoded = ((ticks + 7U) / 8U) + 160U;
    return (encoded > 223U) ? 223U : encoded;
  }
  if (ticks <= 1008U)
  {
    uint32_t encoded = ((ticks + 15U) / 16U) + 192U;
    return (encoded > 255U) ? 255U : encoded;
  }
  return 255U;
}

extern "C" uint32_t TIM1_GetDeadtimePercent(void)
{
  return tim1_deadtime_percent_prev;
}

extern "C" void TIM1_SetDeadtimePercent(uint32_t percent)
{
  if (percent > 100U)
  {
    percent = 100U;
  }

  uint32_t arr_plus1 = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
  uint32_t factor =
      ((htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED1) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED2) ||
       (htim1.Init.CounterMode == TIM_COUNTERMODE_CENTERALIGNED3))
          ? 2U
          : 1U;

  uint64_t period_ticks = (uint64_t)arr_plus1 * (uint64_t)factor;
  uint64_t desired_ticks = (period_ticks * percent) / 100ULL;
  if (desired_ticks > 1008ULL)
  {
    desired_ticks = 1008ULL;
  }

  uint32_t deadtime_reg = TIM1_DeadtimeTicksToRegister((uint32_t)desired_ticks);

  HAL_TIMEx_ConfigDeadTime(&htim1, deadtime_reg);
  HAL_TIMEx_ConfigAsymmetricalDeadTime(&htim1, deadtime_reg); /* keep falling/rising edges aligned */
  htim1.Instance->EGR |= TIM_EGR_COMG; /* latch new DT when preload is on */
  tim1_deadtime_percent_prev = percent;
}
