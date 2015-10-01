#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#include "rcio.h"

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

static bool rcio_init(void)
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

    return true;

errout_allocated:
    kobject_put(rcio_kobj);
    return false;
}

static void rcio_exit(void)
{
    kobject_put(rcio_kobj);
}


int rcio_probe(struct rcio_adapter *state)
{
    int ret;
    const char buffer[] = {0x02, 0x07, 0x01};

    if (!rcio_init()) {
        goto errout_init;
    }

    ret = state->write(state, buffer, sizeof(buffer));

    if (ret < 0) {
        return ret;
    }

    return 0;

errout_init:
    return -EBUSY;
}

int rcio_remove(struct rcio_adapter *state)
{
    int ret;

    rcio_exit();

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_kobj);
EXPORT_SYMBOL_GPL(rcio_probe);
EXPORT_SYMBOL_GPL(rcio_remove);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
