#ifndef _RCIO_PWM_H
#define _RCIO_PWM_H

#include "rcio.h"

#define RCIO_PWM_MAX_CHANNELS 16
#define RCIO_PWM_TIMER_COUNT 4
#define RCIO_PWM_CHANNELS_PER_TIMER 4
#define RCIO_PWM_MAX_ZEROED_CHANNELS 12
#define RCIO_PWM_ZERO_SKIP_UPDATE_CYCLES 50

int rcio_pwm_probe(struct rcio_state* state);
bool rcio_pwm_update(struct rcio_state *state);
int rcio_pwm_remove(struct rcio_state *state);
int pwm_check_device_motors_running_count(struct rcio_state *state);
int rcio_pwm_force_zero_duty(struct rcio_state *state);

#endif
