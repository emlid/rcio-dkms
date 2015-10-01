#ifndef _RCIO_H
#define _RCIO_H

#include <linux/mutex.h>

struct rcio_state
{
    struct kobject *object;
    struct rcio_adapter *adapter;
    int (*register_set)(struct rcio_state *state, u8 page, u8 offset, const u16 *values, u8 num_values);
    int (*register_get)(struct rcio_state *state, u8 page, u8 offset, u16 *values, u8 num_values);
    int (*register_set_byte)(struct rcio_state *state, u8 page, u8 offset, u16 value);
    u16 (*register_get_byte)(struct rcio_state *state, u8 page, u8 offset);
};

struct rcio_adapter {
	void *client;
	struct mutex lock;

    int (*read)(struct rcio_adapter *state, u16 address, char *buffer, size_t length); 
    int (*write)(struct rcio_adapter *state, u16 address, const char *buffer, size_t length); 
};

int rcio_probe(struct rcio_adapter *state);
int rcio_remove(struct rcio_adapter *state);

#endif /* _RCIO_H */
