//
// Created by harry on 25. 12. 1..
//

#ifndef POWER_HARRY_H
#define POWER_HARRY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "Harry.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>




#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct PwmStatus{
    uint32_t freq_hz;
    float duty_pct;
    float deadtime_pct;
};

inline bool operator==(const PwmStatus &lhs, const PwmStatus &rhs)
{
    return (lhs.freq_hz == rhs.freq_hz) &&
           (lhs.duty_pct == rhs.duty_pct) &&
           (lhs.deadtime_pct == rhs.deadtime_pct);
}

inline bool operator!=(const PwmStatus &lhs, const PwmStatus &rhs)
{
    return !(lhs == rhs);
}

class PWM {
public:
    TIM_HandleTypeDef *htim;
    PwmStatus newStatus;
    PwmStatus oldStatus;

    PWM();
    uint32_t Harry_GetTimerClock() const;
    void PwmUpdate();
    void PwmInit(TIM_HandleTypeDef *htim,
                 uint32_t freq_hz = 100000U,
                 float duty_pct = 49.0f,
                 float deadtime_pct = 2.5f);
    PwmStatus getPwmStatusFromRegister() const;

    void setFrequency(uint32_t freq_hz);
    void setDutyCycle(float duty_pct);
    void setDeadtime(float deadtime_pct);
};

extern PWM global_pwms[1];

#endif /* __cplusplus */

#endif //POWER_HARRY_H
