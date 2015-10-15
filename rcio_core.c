#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#include "rcio.h"
#include "rcio_adc.h"

static int connected;

static ssize_t connected_show(struct kobject *kobj, struct kobj_attribute *attr,
            char *buf)
{
    return sprintf(buf, "%d\n", connected);
}

static ssize_t connected_store(struct kobject *kobj, struct kobj_attribute *attr,
             const char *buf, size_t count)
{
    int ret;

    ret = kstrtoint(buf, 10, &connected);
    if (ret < 0)
        return ret;

    return count;
}

static struct kobj_attribute connected_attribute =
    __ATTR_RW(connected);


static struct attribute *attrs[] = {
    &connected_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

struct kobject *rcio_kobj;

static int register_set(struct rcio_state *state, u8 page, u8 offset, const u16 *values, u8 num_values)
{
    int ret;

    ret = state->adapter->write(state->adapter, (page << 8) | offset, (void *)values, num_values);

    return ret;
}

static int register_get(struct rcio_state *state, u8 page, u8 offset, u16 *values, u8 num_values)
{
    int ret;

    ret = state->adapter->read(state->adapter, (page << 8) | offset, (void *)values, num_values);

    return ret;
}

static int register_set_byte(struct rcio_state *state, u8 page, u8 offset, u16 value)
{
    return register_set(state, page, offset, &value, 1);
}

static u16 register_get_byte(struct rcio_state *state, u8 page, u8 offset)
{
    u16 reg;
    
    register_get(state, page, offset, &reg, 1);

    return reg;
}

struct rcio_state rcio_state;

static bool rcio_init(struct rcio_adapter *adapter)
{
    int retval;

    rcio_kobj = kobject_create_and_add("rcio", kernel_kobj);

    if (rcio_kobj == NULL) {
        return false;
    }

    retval = sysfs_create_group(rcio_kobj, &attr_group);

    if (retval) {
        goto errout_allocated;
    }

    rcio_state.adapter = adapter;
    rcio_state.register_get = register_get;
    rcio_state.register_set = register_set;
    rcio_state.register_get_byte = register_get_byte;
    rcio_state.register_set_byte = register_set_byte;

    if (rcio_adc_probe(&rcio_state) < 0) {
        goto errout_adc;
    }

    return true;

errout_adc:
errout_allocated:
    kobject_put(rcio_kobj);
    return false;
}

static void rcio_exit(void)
{
    kobject_put(rcio_kobj);
}


int rcio_probe(struct rcio_adapter *adapter)
{
    if (!rcio_init(adapter)) {
        goto errout_init;
    }

    return 0;

errout_init:
    return -EBUSY;
}

int rcio_remove(struct rcio_adapter *adapter)
{
    rcio_exit();

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_kobj);
EXPORT_SYMBOL_GPL(rcio_state);
EXPORT_SYMBOL_GPL(rcio_probe);
EXPORT_SYMBOL_GPL(rcio_remove);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
