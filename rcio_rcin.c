#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"
#include "rcio_rcin_priv.h"

struct rcio_state *rcio;

static int rcin_get_raw_value(struct rcio_state *state, u8 channel);

static ssize_t channel_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int value = -1;

    if (!strcmp(attr->attr.name, "ch0")) {
        value = rcin_get_raw_value(rcio, 0);
    } else if (!strcmp(attr->attr.name, "ch1")) {
        value = rcin_get_raw_value(rcio, 1);
    } else if (!strcmp(attr->attr.name, "ch2")) {
        value = rcin_get_raw_value(rcio, 2);
    } else if (!strcmp(attr->attr.name, "ch3")) {
        value = rcin_get_raw_value(rcio, 3);
    } else if (!strcmp(attr->attr.name, "ch4")) {
        value = rcin_get_raw_value(rcio, 4);
    } else if (!strcmp(attr->attr.name, "ch5")) {
        value = rcin_get_raw_value(rcio, 5);
    }

    if (value < 0) {
        return value;
    }

    return sprintf(buf, "%d\n", value);
}

static struct kobj_attribute ch0_attribute = __ATTR(ch0, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch1_attribute = __ATTR(ch1, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch2_attribute = __ATTR(ch2, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch3_attribute = __ATTR(ch3, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch4_attribute = __ATTR(ch4, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch5_attribute = __ATTR(ch5, S_IRUSR, channel_show, NULL);

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
    .name = "rcin",
    .attrs = attrs,
};

static int rcin_get_raw_value(struct rcio_state *state, u8 channel)
{
    uint16_t status;
    struct rc_input_values arg;
    struct rc_input_values *rc_val = (struct rc_input_values *)&arg;
    int ret;

    if ((ret = state->register_get(state, PX4IO_PAGE_STATUS, PX4IO_P_STATUS_FLAGS, &status, 1)) < 0) {
        return ret;
    }

    /* if no R/C input, don't try to fetch anything */
    if (!(status & PX4IO_P_STATUS_FLAGS_RC_OK)) {
        return -ENOTCONN;
    }

    /* sort out the source of the values */
    if (status & PX4IO_P_STATUS_FLAGS_RC_PPM) {
        rc_val->input_source = RC_INPUT_SOURCE_PX4IO_PPM;

    } else if (status & PX4IO_P_STATUS_FLAGS_RC_DSM) {
        rc_val->input_source = RC_INPUT_SOURCE_PX4IO_SPEKTRUM;

    } else if (status & PX4IO_P_STATUS_FLAGS_RC_SBUS) {
        rc_val->input_source = RC_INPUT_SOURCE_PX4IO_SBUS;

    } else if (status & PX4IO_P_STATUS_FLAGS_RC_ST24) {
        rc_val->input_source = RC_INPUT_SOURCE_PX4IO_ST24;

    } else {
        rc_val->input_source = RC_INPUT_SOURCE_UNKNOWN;
    }

    /* read raw R/C input values */
    if (state->register_get(state, PX4IO_PAGE_RAW_RC_INPUT, PX4IO_P_RAW_RC_BASE, &(rc_val->values[0]), 6) < 0){
        return -EIO;
    }

    return rc_val->values[channel];
}

int rcio_rcin_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (ret < 0) {
        printk(KERN_INFO "sysfs failed\n");
    }

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_rcin_probe);
MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO RC Input driver");
MODULE_LICENSE("GPL v2");
