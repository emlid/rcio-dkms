#ifndef _RCIO_H
#define _RCIO_H

#include <linux/mutex.h>

struct rcio_state {
	void *client;
	struct mutex lock;

    int (*read)(struct rcio_state *state, char *buffer, size_t length); 
    int (*write)(struct rcio_state *state, const char *buffer, size_t length); 
};

int rcio_probe(struct rcio_state *state);
int rcio_remove(struct rcio_state *state);

#endif /* _RCIO_H */
