#include <linux/delay.h>
#include <linux/module.h>
#define DEBUG
#include <linux/device.h>

#include "rcio.h"
#include "protocol.h"

#define rcio_safety_err(__dev, format, args...)\
        dev_err(__dev, "rcio_safety: " format, ##args)
#define rcio_safety_warn(__dev, format, args...)\
        dev_warn(__dev, "rcio_safety: " format, ##args)

static struct rcio_safety {
    struct rcio_state *rcio;
    unsigned long timeout;
    bool heartbeat_enabled;
    uint16_t heartbeat;
} safety;

bool rcio_safety_update(struct rcio_state *state);

static int rcio_safety_do_heartbeat(struct rcio_state *state) {
    int result = state->register_set(state, PX4IO_PAGE_RCIO_HEARTBEAT, 0, &safety.heartbeat, 1);
    safety.heartbeat++;
    if (safety.heartbeat > 0xFF) safety.heartbeat = 0;
    return result;
}

static ssize_t heartbeat_enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", safety.heartbeat_enabled);
}

static ssize_t heartbeat_enabled_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{

    long value = 0;
    int result = kstrtol(buf, 10, &value);

    if (result == 0) {
        bool heartbeat_enabled = (value != 0);
        rcio_safety_warn(safety.rcio->adapter->dev, "Heartbeat_enabled is set to %d\n", heartbeat_enabled);
        safety.heartbeat_enabled = heartbeat_enabled;
    } else {
        rcio_safety_err(safety.rcio->adapter->dev, "Invalid value for heartbeat_enable");
        return -EINVAL;
    }
    return count;
}

static struct kobj_attribute heartbeat_enabled_attribute = __ATTR_RW(heartbeat_enabled);

static struct attribute *attrs[] = {
    &heartbeat_enabled_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .name = "safety",
    .attrs = attrs,
};

bool rcio_safety_update(struct rcio_state *state)
{
    if (time_before(jiffies, safety.timeout)) {
        return false;
    }

    if (safety.heartbeat_enabled) {
        if (!rcio_safety_do_heartbeat(state)) {
            rcio_safety_err(state->adapter->dev, "Could not do heartbeat\n");
        }
    }

    safety.timeout = jiffies + msecs_to_jiffies(200); /* timeout in 0.2s */
    return true;
}

bool rcio_safety_probe(struct rcio_state *state)
{
    int ret;

    safety.rcio = state;

    safety.timeout = jiffies + msecs_to_jiffies(20); /* timeout in 0.02s */

    ret = sysfs_create_group(safety.rcio->object, &attr_group);

    if (ret < 0) {
        rcio_safety_err(state->adapter->dev, "module not registered int sysfs\n");
        return false;
    }

    safety.heartbeat = 0;
    safety.heartbeat_enabled = true;

    return true;
}

EXPORT_SYMBOL_GPL(rcio_safety_probe);
EXPORT_SYMBOL_GPL(rcio_safety_update);
MODULE_AUTHOR("Nikita Tomilov <nikita.tomilov@emlid.com>");
MODULE_DESCRIPTION("RCIO safety driver");
MODULE_LICENSE("GPL v2");
