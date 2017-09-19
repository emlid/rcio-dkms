#include <linux/delay.h>
#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"

#define RCIO_ADC_MAX_CHANNELS_COUNT 8

static struct rcio_state *rcio;

static u16 measurements[RCIO_ADC_MAX_CHANNELS_COUNT];

bool rcio_adc_update(struct rcio_state *state);

static ssize_t channel_show(struct kobject *kobj, struct kobj_attribute *attr,
            char *buf)
{
    ssize_t channel = -1;

    if (!strcmp(attr->attr.name, "ch0")) {
        channel = measurements[0];
    } else if (!strcmp(attr->attr.name, "ch1")) {
        channel = measurements[1];
    } else if (!strcmp(attr->attr.name, "ch2")) {
        channel = measurements[2];
    } else if (!strcmp(attr->attr.name, "ch3")) {
        channel = measurements[3];
    } else if (!strcmp(attr->attr.name, "ch4")) {
        channel = measurements[4];
    } else if (!strcmp(attr->attr.name, "ch5")) {
        channel = measurements[5];
    } else if (!strcmp(attr->attr.name, "ch6")) {
        channel = measurements[6];
    } else if (!strcmp(attr->attr.name, "ch7")) {
        channel = measurements[7];
    }
    

    if (channel < 0) {
        return -EBUSY;
    }

    return sprintf(buf, "%d\n", (int)channel);
}

static struct kobj_attribute ch0_attribute = __ATTR(ch0, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch1_attribute = __ATTR(ch1, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch2_attribute = __ATTR(ch2, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch3_attribute = __ATTR(ch3, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch4_attribute = __ATTR(ch4, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch5_attribute = __ATTR(ch5, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch6_attribute = __ATTR(ch6, S_IRUGO, channel_show, NULL);
static struct kobj_attribute ch7_attribute = __ATTR(ch7, S_IRUGO, channel_show, NULL);

static struct attribute *attrs[] = {
    &ch0_attribute.attr,
    &ch1_attribute.attr,
    &ch2_attribute.attr,
    &ch3_attribute.attr,
    &ch4_attribute.attr,
    &ch5_attribute.attr,
    &ch6_attribute.attr,
    &ch7_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .name = "adc",
    .attrs = attrs,
};

static unsigned long timeout;

bool rcio_adc_update(struct rcio_state *state)
{
    if (time_before(jiffies, timeout)) {
        return false;
    }

    if (state->register_get(state, PX4IO_PAGE_RAW_ADC_INPUT, 0, measurements, RCIO_ADC_MAX_CHANNELS_COUNT) < 0) {
        return false;
    }

    timeout = jiffies + HZ / 50; /* timeout in 0.02s */
    return true;
}


int rcio_adc_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    timeout = jiffies + HZ / 50; /* timeout in 0.02s */
    
    //switching off channels we dont use
    attrs[state->adc_channels_count] = NULL;

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (ret < 0) {
        printk(KERN_INFO "sysfs failed\n");
    }

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_adc_probe);
EXPORT_SYMBOL_GPL(rcio_adc_update);
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO ADC driver");
MODULE_LICENSE("GPL v2");
