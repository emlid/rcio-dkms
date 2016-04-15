#include <linux/delay.h>
#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"

#define RCIO_ADC_CHANNELS_COUNT 6

static void handle_status(uint16_t status);
static void handle_alarms(uint16_t alarms);

struct rcio_state *rcio;

bool rcio_status_update(struct rcio_state *state);

static bool init_ok;
static bool pwm_ok;
static bool alive;
static ssize_t init_ok_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", init_ok? 1: 0);
}

static ssize_t pwm_ok_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", pwm_ok? 1: 0);
}

static ssize_t alive_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", alive? 1: 0);
}

static struct kobj_attribute init_ok_attribute = __ATTR(init_ok, S_IRUGO, init_ok_show, NULL);
static struct kobj_attribute pwm_ok_attribute = __ATTR(pwm_ok, S_IRUGO, pwm_ok_show, NULL);
static struct kobj_attribute alive_attribute = __ATTR_RO(alive);

static struct attribute *attrs[] = {
    &init_ok_attribute.attr,
    &pwm_ok_attribute.attr,
    &alive_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .name = "status",
    .attrs = attrs,
};

unsigned long timeout;

bool rcio_status_update(struct rcio_state *state)
{
    uint16_t regs[6];

    if (time_before(jiffies, timeout)) {
        return false;
    }

    if (state->register_get(state, PX4IO_PAGE_STATUS, PX4IO_P_STATUS_FLAGS, regs, ARRAY_SIZE(regs)) < 0) {
        alive = false;
        return false;
    }

    alive = true;

    handle_status(regs[0]);
    handle_alarms(regs[1]);

    timeout = jiffies + HZ / 5; /* timeout in 0.5s */
    return true;
}


bool rcio_status_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    timeout = jiffies + HZ / 50; /* timeout in 0.05s */

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (ret < 0) {
        pr_err(KERN_INFO "[RCIO]: status module not registered int sysfs\n");
    }

    init_ok = false;

    return true;
}

static void handle_status(uint16_t status)
{
    if (status & PX4IO_P_STATUS_FLAGS_INIT_OK) {
        init_ok = true;
    } else {
        init_ok = false;
    }
}

static void handle_alarms(uint16_t alarms)
{
    if (alarms & PX4IO_P_STATUS_ALARMS_PWM_ERROR) {
        pwm_ok = false;
    } else {
        pwm_ok = true;
    }
}

EXPORT_SYMBOL_GPL(rcio_status_probe);
EXPORT_SYMBOL_GPL(rcio_status_update);
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO ADC driver");
MODULE_LICENSE("GPL v2");
