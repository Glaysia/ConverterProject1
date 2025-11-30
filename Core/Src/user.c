#include "user.h"
#include "stm32f7xx_ll_tim.h"

static uint32_t pwm_is_apb2_timer(const TIM_TypeDef *instance);
static uint32_t pwm_get_timer_clock(const TIM_HandleTypeDef *htim);
static uint32_t pwm_get_max_arr(const TIM_HandleTypeDef *htim);

HAL_StatusTypeDef pwm_set_frequency(TIM_HandleTypeDef *htim, uint32_t frequency_hz)
{
  if ((htim == NULL) || (frequency_hz == 0U))
  {
    return HAL_ERROR;
  }

  uint32_t timer_clock = pwm_get_timer_clock(htim);
  if (timer_clock == 0U)
  {
    return HAL_ERROR;
  }
  uint64_t ticks_per_period = ((uint64_t)timer_clock) / (uint64_t)frequency_hz;

  if (ticks_per_period == 0U)
  {
    return HAL_ERROR;
  }

  uint64_t max_period = (uint64_t)pwm_get_max_arr(htim) + 1ULL;
  uint32_t prescaler = (uint32_t)((ticks_per_period + max_period - 1ULL) / max_period);

  if (prescaler == 0U)
  {
    prescaler = 1U;
  }

  if (prescaler > 0x10000U)
  {
    return HAL_ERROR;
  }

  uint64_t arr64 = ticks_per_period / prescaler;

  if (arr64 == 0U)
  {
    return HAL_ERROR;
  }

  uint32_t arr = (uint32_t)(arr64 - 1ULL);
  uint32_t was_enabled = (htim->Instance->CR1 & TIM_CR1_CEN);

  if (was_enabled != 0U)
  {
    __HAL_TIM_DISABLE(htim);
  }

  __HAL_TIM_SET_PRESCALER(htim, prescaler - 1U);
  htim->Init.Prescaler = prescaler - 1U;
  __HAL_TIM_SET_AUTORELOAD(htim, arr);
  htim->Init.Period = arr;
  __HAL_TIM_SET_COUNTER(htim, 0U);
  htim->Instance->EGR = TIM_EGR_UG;

  if (was_enabled != 0U)
  {
    __HAL_TIM_ENABLE(htim);
  }

  return HAL_OK;
}

HAL_StatusTypeDef pwm_set_dutycycle(TIM_HandleTypeDef *htim, uint32_t channel, float duty_cycle_percent)
{
  if ((htim == NULL) || (IS_TIM_CHANNELS(channel) == 0U) || (channel == TIM_CHANNEL_ALL) ||
      (IS_TIM_CCX_INSTANCE(htim->Instance, channel) == 0U))
  {
    return HAL_ERROR;
  }

  if (duty_cycle_percent < 0.0f)
  {
    duty_cycle_percent = 0.0f;
  }
  else if (duty_cycle_percent > 100.0f)
  {
    duty_cycle_percent = 100.0f;
  }

  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
  uint64_t period_counts = (uint64_t)arr + 1ULL;

  double ratio = ((double)duty_cycle_percent) / 100.0;
  if (ratio > 1.0)
  {
    ratio = 1.0;
  }

  uint32_t compare = (uint32_t)(ratio * (double)period_counts);

  if (compare > arr)
  {
    compare = arr;
  }

  __HAL_TIM_SET_COMPARE(htim, channel, compare);
  return HAL_OK;
}

HAL_StatusTypeDef pwm_set_deadtime(TIM_HandleTypeDef *htim, float deadtime_pct)
{
  if ((htim == NULL) || (IS_TIM_ADVANCED_INSTANCE(htim->Instance) == 0U))
  {
    return HAL_ERROR;
  }

  if (deadtime_pct < 0.0f)
  {
    deadtime_pct = 0.0f;
  }
  else if (deadtime_pct > 100.0f)
  {
    deadtime_pct = 100.0f;
  }

  uint32_t timer_clock = pwm_get_timer_clock(htim);
  if (timer_clock == 0U)
  {
    return HAL_ERROR;
  }

  uint32_t prescaler = htim->Instance->PSC + 1U;
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
  uint64_t ticks_per_period = ((uint64_t)arr + 1ULL) * (uint64_t)prescaler;

  if (ticks_per_period == 0ULL)
  {
    return HAL_ERROR;
  }

  double ratio = ((double)deadtime_pct) / 100.0;
  double period_ns = ((double)ticks_per_period * 1e9) / (double)timer_clock;
  double deadtime_ns_double = period_ns * ratio;
  uint32_t deadtime_ns = 0U;

  if (deadtime_ns_double >= (double)UINT32_MAX)
  {
    deadtime_ns = UINT32_MAX;
  }
  else if (deadtime_ns_double > 0.0)
  {
    deadtime_ns = (uint32_t)deadtime_ns_double;
  }

  uint8_t dtg = __LL_TIM_CALC_DEADTIME(timer_clock, htim->Init.ClockDivision, deadtime_ns);

  if ((deadtime_ns != 0U) && (dtg == 0U))
  {
    return HAL_ERROR;
  }

  MODIFY_REG(htim->Instance->BDTR, TIM_BDTR_DTG, dtg);
  return HAL_OK;
}

static uint32_t pwm_is_apb2_timer(const TIM_TypeDef *instance)
{
#if defined(TIM1)
  if (instance == TIM1)
  {
    return 1U;
  }
#endif
#if defined(TIM8)
  if (instance == TIM8)
  {
    return 1U;
  }
#endif
#if defined(TIM9)
  if (instance == TIM9)
  {
    return 1U;
  }
#endif
#if defined(TIM10)
  if (instance == TIM10)
  {
    return 1U;
  }
#endif
#if defined(TIM11)
  if (instance == TIM11)
  {
    return 1U;
  }
#endif
  return 0U;
}

static uint32_t pwm_get_timer_clock(const TIM_HandleTypeDef *htim)
{
  RCC_ClkInitTypeDef clk_config = {0};
  uint32_t flash_latency = 0;

  HAL_RCC_GetClockConfig(&clk_config, &flash_latency);

  uint32_t clock = 0U;

  if (pwm_is_apb2_timer(htim->Instance) != 0U)
  {
    clock = HAL_RCC_GetPCLK2Freq();
    if (clk_config.APB2CLKDivider != RCC_HCLK_DIV1)
    {
      clock *= 2U;
    }
  }
  else
  {
    clock = HAL_RCC_GetPCLK1Freq();
    if (clk_config.APB1CLKDivider != RCC_HCLK_DIV1)
    {
      clock *= 2U;
    }
  }

  return clock;
}

static uint32_t pwm_get_max_arr(const TIM_HandleTypeDef *htim)
{
  if (IS_TIM_32B_COUNTER_INSTANCE(htim->Instance) != 0U)
  {
    return 0xFFFFFFFFU;
  }

  return 0xFFFFU;
}
