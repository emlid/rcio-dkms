#ifndef _RCIO_STATUS_H
#define _RCIO_STATUS_H

#include "rcio.h"

bool rcio_status_probe(struct rcio_state* state);
bool rcio_status_update(struct rcio_state *state);

#endif
