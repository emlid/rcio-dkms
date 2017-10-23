#ifndef _RCIO_SAFETY_H
#define _RCIO_SAFETY_H

#include "rcio.h"

bool rcio_safety_probe(struct rcio_state* state);
bool rcio_safety_update(struct rcio_state *state);

#endif
