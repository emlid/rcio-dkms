#ifndef _RCIO_GPIO_H
#define _RCIO_GPIO_H

#include "rcio.h"

int rcio_gpio_probe(struct rcio_state* state);
bool rcio_gpio_update(struct rcio_state *state);
bool rcio_gpio_remove(struct rcio_state *state);

#endif
