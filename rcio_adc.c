#include <linux/delay.h>
#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"

struct rcio_state *rcio;

static ssize_t adc_get_raw_adc(struct rcio_state *state, u8 channel);

static ssize_t channel_show(struct kobject *kobj, struct kobj_attribute *attr,
            char *buf)
{
    ssize_t channel = -1;

    if (!strcmp(attr->attr.name, "ch0")) {
        channel = adc_get_raw_adc(rcio, 0);
    } else if (!strcmp(attr->attr.name, "ch1")) {
        channel = adc_get_raw_adc(rcio, 1);
    } else if (!strcmp(attr->attr.name, "ch2")) {
        channel = adc_get_raw_adc(rcio, 2);
    } else if (!strcmp(attr->attr.name, "ch3")) {
        channel = adc_get_raw_adc(rcio, 3);
    } else if (!strcmp(attr->attr.name, "ch4")) {
        channel = adc_get_raw_adc(rcio, 4);
    } else if (!strcmp(attr->attr.name, "ch5")) {
        channel = adc_get_raw_adc(rcio, 5);
    }

    if (channel < 0) {
        return -EBUSY;
    }

    return sprintf(buf, "%d\n", channel);
}

static struct kobj_attribute ch0_attribute = __ATTR(ch0, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch1_attribute = __ATTR(ch1, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch2_attribute = __ATTR(ch2, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch3_attribute = __ATTR(ch3, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch4_attribute = __ATTR(ch4, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch5_attribute = __ATTR(ch5, S_IRUGO, channel_show, NULL);

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
    .name = "adc",
    .attrs = attrs,
};

static ssize_t adc_get_raw_adc(struct rcio_state *state, u8 channel)
{
    u16 reg;

    if (state->register_get(state, PX4IO_PAGE_RAW_ADC_INPUT, channel, &reg, 1)) {
        return reg;
    }

    return -EBUSY;
}

int rcio_adc_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (ret < 0) {
        printk(KERN_INFO "sysfs failed\n");
    }

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_adc_probe);
MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO ADC driver");
MODULE_LICENSE("GPL v2");
