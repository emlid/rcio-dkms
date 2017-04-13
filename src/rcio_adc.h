#ifndef _RCIO_ADC_H
#define _RCIO_ADC_H

#include "rcio.h"

int rcio_adc_probe(struct rcio_state* state);
bool rcio_adc_update(struct rcio_state *state);

#endif
