#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"
#include "rcio_rcin_priv.h"

#define RCIO_RCIN_MAX_CHANNELS 8

struct rcio_state *rcio;

static int rcin_get_raw_values(struct rcio_state *state, struct rc_input_values *rc_val);

static u16 measurements[RCIO_RCIN_MAX_CHANNELS] = {0};

static ssize_t channel_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int value = -1;

    if (!strcmp(attr->attr.name, "ch0")) {
        value = measurements[0];
    } else if (!strcmp(attr->attr.name, "ch1")) {
        value = measurements[1];
    } else if (!strcmp(attr->attr.name, "ch2")) {
        value = measurements[2];
    } else if (!strcmp(attr->attr.name, "ch3")) {
        value = measurements[3];
    } else if (!strcmp(attr->attr.name, "ch4")) {
        value = measurements[4];
    } else if (!strcmp(attr->attr.name, "ch5")) {
        value = measurements[5];
    } else if (!strcmp(attr->attr.name, "ch6")) {
        value = measurements[6];
    } else if (!strcmp(attr->attr.name, "ch7")) {
        value = measurements[7];
    }

    if (value < 0) {
        return value;
    }

    return sprintf(buf, "%d\n", value);
}

static bool connected; 
static ssize_t connected_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", connected? 1: 0);
}

static struct kobj_attribute ch0_attribute = __ATTR(ch0, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch1_attribute = __ATTR(ch1, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch2_attribute = __ATTR(ch2, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch3_attribute = __ATTR(ch3, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch4_attribute = __ATTR(ch4, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch5_attribute = __ATTR(ch5, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch6_attribute = __ATTR(ch6, S_IRUSR, channel_show, NULL);
static struct kobj_attribute ch7_attribute = __ATTR(ch7, S_IRUSR, channel_show, NULL);
static struct kobj_attribute connected_attribute = __ATTR(connected, S_IRUSR, connected_show, NULL);

static struct attribute *attrs[] = {
    &ch0_attribute.attr,
    &ch1_attribute.attr,
    &ch2_attribute.attr,
    &ch3_attribute.attr,
    &ch4_attribute.attr,
    &ch5_attribute.attr,
    &ch6_attribute.attr,
    &ch7_attribute.attr,
    &connected_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .name = "rcin",
    .attrs = attrs,
};

unsigned long timeout;

bool rcio_rcin_update(struct rcio_state *state)
{
    int ret;
    struct rc_input_values report;

    if (time_before(jiffies, timeout)) {
        return false;
    }

    ret = rcin_get_raw_values(state, &report);

    if (ret == -ENOTCONN) {
        connected = false;
        return true;
    } else if (ret < 0) {
        connected = false;
        return false;
    }

    connected = true;

    for (int i = 0; i < RCIO_RCIN_MAX_CHANNELS; i++) {
        if (report.values[i] > 2500 || report.values[i] < 800) {
           continue; 
        }

        measurements[i] = report.values[i];
    }
    
    timeout = jiffies + HZ / 100; /* timeout in 100 mS */

    return true;
}

int rcio_rcin_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    timeout = jiffies + HZ / 100; /* timeout in 100 ms */

    ret = sysfs_create_group(rcio->object, &attr_group);

    if (ret < 0) {
        printk(KERN_INFO "sysfs failed\n");
    }

    connected = false;

    return 0;
}

static int rcin_get_raw_values(struct rcio_state *state, struct rc_input_values *rc_val)
{
    uint16_t status;
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
    if (state->register_get(state, PX4IO_PAGE_RAW_RC_INPUT, PX4IO_P_RAW_RC_BASE, 
                &(rc_val->values[0]), RCIO_RCIN_MAX_CHANNELS) < 0) {
        return -EIO;
    }
    
    return 0;
}

EXPORT_SYMBOL_GPL(rcio_rcin_probe);
EXPORT_SYMBOL_GPL(rcio_rcin_update);

MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO RC Input driver");
MODULE_LICENSE("GPL v2");
