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
#include "rcio_status.h"

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

static struct rcio_state rcio_state;

struct task_struct *task;

int worker(void *data)
{
    struct rcio_state *state = (struct rcio_state *) data;
    int fail_counter = 0;
    bool pwm_updated = false;
    bool adc_updated = false;
    bool rcin_updated = false;

    while (!kthread_should_stop()) {
        pwm_updated = rcio_pwm_update(state);
        adc_updated = rcio_adc_update(state);
        rcin_updated = rcio_rcin_update(state);
        rcio_status_update(state);

        if (pwm_updated || adc_updated || rcin_updated) {
            fail_counter = 0;
        } else {
            fail_counter++;
        }

        usleep_range(1000, 1500);
    } 

    return 0;
}

static int rcio_init(struct rcio_adapter *adapter)
{
    rcio_state.object = kobject_create_and_add("rcio", kernel_kobj);

    if (rcio_state.object == NULL) {
        return -EINVAL;
    }

    rcio_state.adapter = adapter;
    rcio_state.register_get = register_get;
    rcio_state.register_set = register_set;
    rcio_state.register_get_byte = register_get_byte;
    rcio_state.register_set_byte = register_set_byte;
    rcio_state.register_modify = register_modify;
    mutex_init(&rcio_state.adapter->lock);


    if (rcio_adc_probe(&rcio_state) < 0) {
        goto errout_adc;
    }

    if (rcio_pwm_probe(&rcio_state) < 0) {
        goto errout_pwm;
    }

    if (rcio_rcin_probe(&rcio_state) < 0) {
        goto errout_rcin;
    }

    if (rcio_status_probe(&rcio_state) < 0) {
        goto errout_status;
    }

    task = kthread_run(&worker, (void *)&rcio_state,"rcio_worker");

    return 0;

errout_status:
errout_rcin:
    rcio_pwm_remove(&rcio_state);
errout_pwm:
errout_adc:
    kobject_put(rcio_state.object);
    return -EIO;
}

static void rcio_stop(void)
{
    kobject_put(rcio_state.object);
}


int rcio_probe(struct rcio_adapter *adapter)
{
    if (rcio_init(adapter) < 0) {
        goto errout_init;
    }

    return 0;

errout_init:
    return -EBUSY;
}

int rcio_remove(struct rcio_adapter *adapter)
{
    int ret;

    kthread_stop(task);

    mutex_destroy(&rcio_state.adapter->lock);
    ret = rcio_pwm_remove(&rcio_state);

    rcio_stop();

    return ret;
}

EXPORT_SYMBOL_GPL(rcio_state);
EXPORT_SYMBOL_GPL(rcio_probe);
EXPORT_SYMBOL_GPL(rcio_remove);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
