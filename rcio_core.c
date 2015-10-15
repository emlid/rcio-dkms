#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include "rcio.h"
#include "rcio_adc.h"
#include "rcio_pwm.h"
#include "rcio_rcin.h"

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

    if (!mutex_trylock(&state->adapter->lock)) {
        return -EBUSY;
    }

    ret = state->adapter->write(state->adapter, (page << 8) | offset, (void *)values, num_values);

    mutex_unlock(&state->adapter->lock);

    return ret;
}

static int register_get(struct rcio_state *state, u8 page, u8 offset, u16 *values, u8 num_values)
{
    int ret;

    if (!mutex_trylock(&state->adapter->lock)) {
        return -EBUSY;
    }

    ret = state->adapter->read(state->adapter, (page << 8) | offset, (void *)values, num_values);

    mutex_unlock(&state->adapter->lock);

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

static int register_modify(struct rcio_state *state, u8 page, u8 offset, u16 clearbits, u16 setbits)
{
    int ret;
    u16 value;

    ret = register_get(state, page, offset, &value, 1);

    if (ret < 0)
        return ret;

    value &= ~clearbits;
    value |= setbits;

    return register_set_byte(state, page, offset, value);
}

struct rcio_state rcio_state;

struct task_struct *task;

int worker(void *data)
{
    struct rcio_state *state = (struct rcio_state *) data;

    while (!kthread_should_stop()) {
        rcio_pwm_update(state);
        rcio_adc_update(state);
        rcio_rcin_update(state);
        usleep_range(1000, 1500);
        schedule();
    } 

    return 0;
}

static bool rcio_init(struct rcio_adapter *adapter)
{
    int retval;

    rcio_state.object = kobject_create_and_add("rcio", kernel_kobj);

    if (rcio_state.object == NULL) {
        return false;
    }

    retval = sysfs_create_group(rcio_state.object, &attr_group);

    if (retval) {
        goto errout_allocated;
    }

    rcio_state.adapter = adapter;
    rcio_state.register_get = register_get;
    rcio_state.register_set = register_set;
    rcio_state.register_get_byte = register_get_byte;
    rcio_state.register_set_byte = register_set_byte;
    rcio_state.register_modify = register_modify;

    if (rcio_adc_probe(&rcio_state) < 0) {
        goto errout_adc;
    }

    if (rcio_pwm_probe(&rcio_state) < 0) {
        goto errout_pwm;
    }

    if (rcio_rcin_probe(&rcio_state) < 0) {
        goto errout_rcin;
    }

    task = kthread_run(&worker, (void *)&rcio_state,"rcio_worker");

    return true;

errout_rcin:
errout_pwm:
errout_adc:
errout_allocated:
    kobject_put(rcio_state.object);
    return false;
}

static void rcio_exit(void)
{
    kthread_stop(task);
    kobject_put(rcio_state.object);
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

EXPORT_SYMBOL_GPL(rcio_state);
EXPORT_SYMBOL_GPL(rcio_probe);
EXPORT_SYMBOL_GPL(rcio_remove);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
