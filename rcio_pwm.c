#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"

struct rcio_state *rcio;

static void pwm_set_raw_value(struct rcio_state *state, u8 channel, u16 value);

static ssize_t channel_store(struct kobject *kobj, struct kobj_attribute *attr,
            const char *buf, size_t count)
{
    int value;
    int ret;

    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        return ret;
    }

    printk(KERN_INFO "%s", buf);
    if (!strcmp(attr->attr.name, "ch0")) {
        pwm_set_raw_value(rcio, 0, value);
    } else if (!strcmp(attr->attr.name, "ch1")) {
        pwm_set_raw_value(rcio, 1, value);
    } else if (!strcmp(attr->attr.name, "ch2")) {
        pwm_set_raw_value(rcio, 2, value);
    } else if (!strcmp(attr->attr.name, "ch3")) {
        pwm_set_raw_value(rcio, 3, value);
    } else if (!strcmp(attr->attr.name, "ch4")) {
        pwm_set_raw_value(rcio, 4, value);
    } else if (!strcmp(attr->attr.name, "ch5")) {
        pwm_set_raw_value(rcio, 5, value);
    }

    return count;
}

static struct kobj_attribute ch0_attribute = __ATTR(ch0, S_IWUSR, NULL, channel_store);
static struct kobj_attribute ch1_attribute = __ATTR(ch1, S_IWUSR, NULL, channel_store);
static struct kobj_attribute ch2_attribute = __ATTR(ch2, S_IWUSR, NULL, channel_store);
static struct kobj_attribute ch3_attribute = __ATTR(ch3, S_IWUSR, NULL, channel_store);
static struct kobj_attribute ch4_attribute = __ATTR(ch4, S_IWUSR, NULL, channel_store);
static struct kobj_attribute ch5_attribute = __ATTR(ch5, S_IWUSR, NULL, channel_store);

static struct attribute *attrs[] = {
    &ch0_attribute.attr,
    &ch1_attribute.attr,
    &ch2_attribute.attr,
    &ch3_attribute.attr,
    &ch4_attribute.attr,
    &ch5_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .name = "pwm",
    .attrs = attrs,
};

static void pwm_set_raw_value(struct rcio_state *state, u8 channel, u16 value)
{
    state->register_set_byte(state, PX4IO_PAGE_DIRECT_PWM, channel, value);
}

int rcio_pwm_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FORCE_SAFETY_OFF, PX4IO_FORCE_SAFETY_MAGIC) < 0) {
        printk("SAFETY ON\n");
    }

    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_ARMING, 
                PX4IO_P_SETUP_ARMING_IO_ARM_OK | 
                PX4IO_P_SETUP_ARMING_FMU_ARMED |
                PX4IO_P_SETUP_ARMING_ALWAYS_PWM_ENABLE) < 0) {
        printk("ARMING OFF\n");
    }

    if (ret < 0) {
        printk(KERN_INFO "sysfs failed\n");
    }

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_pwm_probe);
MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO PWM driver");
MODULE_LICENSE("GPL v2");
