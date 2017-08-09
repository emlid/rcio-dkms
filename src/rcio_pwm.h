#ifndef _RCIO_PWM_H
#define _RCIO_PWM_H

#include "rcio.h"

#define RCIO_PWM_MAX_CHANNELS 16
#define RCIO_PWM_TIMER_COUNT 4
#define RCIO_PWM_CHANNELS_PER_TIMER 4

int rcio_pwm_probe(struct rcio_state* state);
bool rcio_pwm_update(struct rcio_state *state);
int rcio_pwm_remove(struct rcio_state *state);

#endif
