//
// Simple PI control loop that keeps the output at 5 V by steering
// the PWM switching frequency between the simulated limits.
//

#include "PWM.h"

#include <stdint.h>

#define CONTROL_INPUT_MIN_HZ      (237100.0f)
#define CONTROL_INPUT_MAX_HZ      (244060.0f)
#define CONTROL_TARGET_VOLTAGE    (5.0f)
#define CONTROL_FULL_ERROR_V      (0.5f)     /* Voltage error that drives most of the freq span */
#define CONTROL_PROP_USAGE        (0.7f)     /* Use 70% of the available span in the P term    */
#define CONTROL_LOOP_BW_FRACTION  (0.10f)    /* Integral bandwidth as a fraction of freq span   */
#define CONTROL_DEFAULT_LOOP_HZ   (2000.0f)
#define CONTROL_ADC_FS_COUNTS     (4095.0f)
#define CONTROL_ADC_FULL_SCALE_V  (5.0f)     /* Adjust if sensing divider does not map 0-5 V    */
#define CONTROL_TWO_PI            (6.28318530718f)

typedef struct
{
    float kp;
    float ki;
    float integrator;
    float last_output;
    float output_min;
    float output_max;
    float sample_period;
    float target_voltage;
    uint8_t initialized;
} ControlState;

static ControlState g_ctrl = {
        0.0f,
        0.0f,
        0.0f,
        CONTROL_INPUT_MIN_HZ,
        CONTROL_INPUT_MIN_HZ,
        CONTROL_INPUT_MAX_HZ,
        1.0f / CONTROL_DEFAULT_LOOP_HZ,
        CONTROL_TARGET_VOLTAGE,
        0U
};

static inline float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static inline float adc_counts_to_voltage(uint16_t adc_sample)
{
    const float lsb = CONTROL_ADC_FULL_SCALE_V / CONTROL_ADC_FS_COUNTS;
    return (float)adc_sample * lsb;
}

static void control_configure(float loop_rate_hz)
{
    const float freq_span = CONTROL_INPUT_MAX_HZ - CONTROL_INPUT_MIN_HZ;
    const float freq_mid = CONTROL_INPUT_MIN_HZ + (freq_span * 0.5f);
    float loop_rate = loop_rate_hz;
    if (loop_rate <= 0.0f) {
        loop_rate = CONTROL_DEFAULT_LOOP_HZ;
    }

    g_ctrl.sample_period = 1.0f / loop_rate;
    g_ctrl.output_min = CONTROL_INPUT_MIN_HZ;
    g_ctrl.output_max = CONTROL_INPUT_MAX_HZ;
    g_ctrl.target_voltage = CONTROL_TARGET_VOLTAGE;
    g_ctrl.integrator = freq_mid;
    g_ctrl.last_output = freq_mid;

    const float usable_span = freq_span * CONTROL_PROP_USAGE;
    const float volts_for_span = (CONTROL_FULL_ERROR_V > 0.0f) ? CONTROL_FULL_ERROR_V : 1.0f;
    g_ctrl.kp = usable_span / volts_for_span;

    float loop_bw_hz = freq_span * CONTROL_LOOP_BW_FRACTION;
    if (loop_bw_hz < 10.0f) {
        loop_bw_hz = 10.0f;
    }
    const float omega_i = CONTROL_TWO_PI * loop_bw_hz;
    g_ctrl.ki = g_ctrl.kp * omega_i * g_ctrl.sample_period;

    g_ctrl.initialized = 1U;
}

static void control_step(float measured_voltage)
{
    const float error = g_ctrl.target_voltage - measured_voltage;
    const float proportional = g_ctrl.kp * error;

    g_ctrl.integrator += g_ctrl.ki * error;
    float command_hz = proportional + g_ctrl.integrator;
    const float limited_hz = clampf(command_hz, g_ctrl.output_min, g_ctrl.output_max);

    if (limited_hz != command_hz) {
        g_ctrl.integrator += (limited_hz - command_hz);
    }

    PWM *pwm = &global_pwms[0];
    pwm->setFrequency((uint32_t)limited_hz);

    g_ctrl.last_output = limited_hz;
}

extern "C" {

void control_init(float loop_rate_hz)
{
    control_configure(loop_rate_hz);
}

void control_set_target_voltage(float volts)
{
    if (volts > 0.0f) {
        g_ctrl.target_voltage = volts;
    }
}

void control_process_voltage(float measured_voltage)
{
    if (g_ctrl.initialized == 0U) {
        control_configure(CONTROL_DEFAULT_LOOP_HZ);
    }
    control_step(measured_voltage);
}

void control_process_adc_sample(uint16_t adc_sample)
{
    const float measured_voltage = adc_counts_to_voltage(adc_sample);
    control_process_voltage(measured_voltage);
}

float control_get_last_command_hz(void)
{
    return g_ctrl.last_output;
}

}
