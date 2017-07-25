#include <linux/module.h>
#include <linux/slab.h>
#define DEBUG
#include <linux/device.h>
#include <linux/gpio/driver.h>
#include "rcio.h"
#include "protocol.h"

#define rcio_gpio_err(__dev, format, args...)\
        dev_err(__dev, "rcio_gpio: " format, ##args)
#define rcio_gpio_warn(__dev, format, args...)\
        dev_warn(__dev, "rcio_gpio: " format, ##args)



static struct rcio_gpio {
    int counter;
    int led_value;
    int led_updated;
    uint16_t pin_states[16];
    struct rcio_state *rcio;
} gpio;

bool rcio_gpio_update(struct rcio_state *state);


static ssize_t led_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int read_result;
    read_result = (gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_GPIO, 0, gpio.pin_states, 16));
    if (read_result < 0) {
        return sprintf(buf, "led is now %d, read_result READING ERROR %d, led regs were %d %d %d %d %d %d %d %d %d %d %d %d\n", gpio.led_value, read_result, gpio.pin_states[0], gpio.pin_states[1], gpio.pin_states[2], gpio.pin_states[3], gpio.pin_states[4], gpio.pin_states[5], gpio.pin_states[6], gpio.pin_states[7], gpio.pin_states[8], gpio.pin_states[9], gpio.pin_states[10], gpio.pin_states[11]);
    } else {
        return sprintf(buf, "led is now %d, read_result is now %d, led regs are %d %d %d %d %d %d %d %d %d %d %d %d\n", gpio.led_value, read_result, gpio.pin_states[0], gpio.pin_states[1], gpio.pin_states[2], gpio.pin_states[3], gpio.pin_states[4], gpio.pin_states[5], gpio.pin_states[6], gpio.pin_states[7], gpio.pin_states[8], gpio.pin_states[9], gpio.pin_states[10], gpio.pin_states[11]);
    }
}

static ssize_t led_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int16_t new_val;
    new_val = (buf[0] == '0') ? 0 : 1;
    gpio.led_value = new_val;
    gpio.led_updated = 0;
    PX4IO_GPIO_SET_PIN_GPIO_ENABLE(gpio.pin_states[4]);
    if (new_val == 0) {
        PX4IO_GPIO_SET_PIN_STATE_LOW(gpio.pin_states[4]);
    } else {
        PX4IO_GPIO_SET_PIN_STATE_HIGH(gpio.pin_states[4]);
    }
    rcio_gpio_warn(gpio.rcio->adapter->dev, "request to store \"%s\" of len %d, int value %d\n", buf, count, new_val);

    return count;
}

static struct kobj_attribute led_attribute = __ATTR(led,  (S_IWUSR | S_IRUGO), led_show, led_store);

static struct attribute *attrs[] = {
    &led_attribute.attr, NULL,
};

static struct attribute_group attr_group = {
    .name = "gpio",
    .attrs = attrs,
};

static int gpio_chip_get(struct gpio_chip *chip, unsigned offset) {
    rcio_gpio_warn(gpio.rcio->adapter->dev, "Getting pin value (gpio_chip_get on offset %d)\n", offset);
    return PX4IO_GPIO_GET_PIN_STATE(gpio.pin_states[offset]);
}

static int gpio_chip_request(struct gpio_chip *chip, unsigned offset) {
    rcio_gpio_warn(gpio.rcio->adapter->dev, "Exporting pin (gpio_chip_request on offset %d)\n", offset);
    PX4IO_GPIO_SET_PIN_GPIO_ENABLE(gpio.pin_states[offset]);
    return 1;
}

static void gpio_chip_free(struct gpio_chip *chip, unsigned offset) {
    rcio_gpio_warn(gpio.rcio->adapter->dev, "Unexporting pin (gpio_chip_free on offset %d)\n", offset);
    PX4IO_GPIO_SET_PIN_GPIO_DISABLE(gpio.pin_states[offset]);
    return;
}

static void gpio_chip_set(struct gpio_chip *chip, unsigned offset, int value) {
    rcio_gpio_warn(gpio.rcio->adapter->dev, "Setting pin value (gpio_chip_set on offset %d value %d)\n", offset, value);
    PX4IO_GPIO_SET_PIN_GPIO_ENABLE(gpio.pin_states[offset]);
    if (value == 0) {
        PX4IO_GPIO_SET_PIN_STATE_LOW(gpio.pin_states[offset]);
    } else {
        PX4IO_GPIO_SET_PIN_STATE_HIGH(gpio.pin_states[offset]);
    }
    gpio.led_updated = 0;
}

static struct gpio_chip gpiochip = {
    .set = gpio_chip_set,
    .get = gpio_chip_get,
    .request = gpio_chip_request,
    .free = gpio_chip_free,
    .label = "Navio PWM pins as GPIO",
    .base = 500,
    .ngpio = 16,
};

bool rcio_gpio_update(struct rcio_state *state)
{
    int result = 1;
    if (!gpio.led_updated) {
        gpio.led_updated = 1;
        //result = (gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO, 4, &(gpio.led_value), 1));
        result = (gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO, 0, &(gpio.pin_states[0]), 16));
        rcio_gpio_warn(gpio.rcio->adapter->dev, "value updated to %d", gpio.led_value);
    }
    return result;
}

bool rcio_gpio_probe(struct rcio_state *state)
{
    int ret = 0;
    gpio.rcio = state;
    ret = sysfs_create_group(gpio.rcio->object, &attr_group);

    if (ret < 0) {
        rcio_gpio_err(state->adapter->dev, "module not registered int sysfs\n");
        return false;
    } else {
        rcio_gpio_warn(state->adapter->dev, "registered gpio module\n");
    }

    ret = gpiochip_add(&gpiochip);

    if (ret < 0) {
        rcio_gpio_err(state->adapter->dev, "error while adding gpiochip\n");
        return false;
    } else {
        rcio_gpio_warn(state->adapter->dev, "gpopchip added successfully under gpio500\n");
    }

    gpio.led_value = 0;
    memset(gpio.pin_states, 0, sizeof(gpio.pin_states));

    return true;

}

int rcio_gpio_remove(struct rcio_state *state) {

    int ret = 1;

    gpiochip_remove(&gpiochip);

    if (ret < 0)
        return ret;


    return ret;
}

EXPORT_SYMBOL_GPL(rcio_gpio_probe);
EXPORT_SYMBOL_GPL(rcio_gpio_update);
EXPORT_SYMBOL_GPL(rcio_gpio_remove);
MODULE_AUTHOR("Nikita Tomilov <nikita.tomilov@emlid.com>");
MODULE_DESCRIPTION("RCIO GPIO driver");
MODULE_LICENSE("GPL v2");
