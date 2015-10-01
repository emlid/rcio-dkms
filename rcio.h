#ifndef _RCIO_H
#define _RCIO_H

#include <linux/mutex.h>

struct rcio_adapter {
	void *client;
	struct mutex lock;

    int (*read)(struct rcio_adapter *state, char *buffer, size_t length); 
    int (*write)(struct rcio_adapter *state, const char *buffer, size_t length); 
};

int rcio_probe(struct rcio_adapter *state);
int rcio_remove(struct rcio_adapter *state);

#endif /* _RCIO_H */
