#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/gpio/driver.h>
#include <linux/delay.h>
#include "rcio.h"
#include "protocol.h"
#include "rcio_pwm.h"

//#define DEBUG

#define GPIO_CHIP_OFFSET 500
#define GPIO_PIN_OFFSET 0

#define rcio_gpio_err(__dev, format, args...)\
        dev_err(__dev, "rcio_gpio: " format, ##args)
#define rcio_gpio_warn(__dev, format, args...)\
        dev_warn(__dev, "rcio_gpio: " format, ##args)

#define rcio_gpio_debug(__dev, format, args...)\
        dev_dbg(__dev, "rcio_gpio: " format, ##args)

bool gpio_supported = false;

uint16_t pwm_ignore_writings_mask = 0;

static struct rcio_gpio {
    int counter;
    int pin_states_updated;
    uint16_t pin_states[RCIO_PWM_MAX_CHANNELS];
    struct rcio_state *rcio;
} gpio;

bool rcio_gpio_update(struct rcio_state *state);
bool rcio_gpio_force_update(struct rcio_state *state);

#define update_required (gpio.pin_states_updated > 0)
#define update_dequeue  gpio.pin_states_updated--
#define update_enqueue    gpio.pin_states_updated++

static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int read_result;
    int motors_running_count = 0;
    uint16_t gpio_exported, pwm_exported, setup_features;

    read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1);
    read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);
    read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FEATURES, &setup_features, 1);

    motors_running_count = pwm_check_device_motors_running_count(gpio.rcio);
    return sprintf(buf, "pwm exported: 0x%x, gpio exported: 0x%x, pwm running: %d.\nFeatures: 0x%x\n", pwm_exported, gpio_exported, motors_running_count, setup_features);
}

static ssize_t reset_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "Write everything here to clear all GPIO pins\n");
}
static ssize_t reset_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int result;
    uint16_t gpio_exported = 0;

    //putting all gpios to low
    for (int i = 0; i < RCIO_PWM_MAX_CHANNELS; i++) {
        if (PX4IO_GPIO_GET_PIN_GPIO_ENABLED(gpio.pin_states[i])) {
            PX4IO_GPIO_SET_PIN_STATE_LOW(gpio.pin_states[i]);
        }
    }
    result = gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO, 0, &(gpio.pin_states[0]), RCIO_PWM_MAX_CHANNELS);

    //clearing them
    for (int i = 0; i < RCIO_PWM_MAX_CHANNELS; i++) gpio.pin_states[i] = 0;

    result = gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);

    rcio_gpio_warn(gpio.rcio->adapter->dev, "All GPIO pins cleared\n");

    return count;
}

static void pwmignore_do_ignore_pin(uint16_t pin_number) {
	pwm_ignore_writings_mask |= ((uint16_t)(1) << pin_number);
}
static void pwmignore_do_unignore_pin(uint16_t pin_number) {
	pwm_ignore_writings_mask &= ~((uint16_t)(1) << pin_number);
}

static ssize_t pwmignore_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%x\nThis is the mask where each bit shows whether pwm writings to this channel will be ignored\n", pwm_ignore_writings_mask);
}

#define inside_range(x, y, z) ((x >= y) && (x <= z))
static ssize_t pwmignore_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    uint16_t max_mask = (uint16_t)((1U << RCIO_PWM_MAX_CHANNELS) - 1U);
    long ignore_mask = 0;
    int result = kstrtol(buf, 16, &ignore_mask);

    if ((result == 0) && inside_range(ignore_mask, 0, max_mask)) {
        rcio_gpio_warn(gpio.rcio->adapter->dev, "Now pins under 0x%x will be ignored for PWM\n", (uint16_t)ignore_mask);
        pwm_ignore_writings_mask= ignore_mask;
    } else {
        rcio_gpio_err(gpio.rcio->adapter->dev, "Invalid value for pwmignore, this should be from 0x0000 to 0x%x\n", max_mask);
        return -EINVAL;
    }
    return count;
}

static struct kobj_attribute status_attribute = __ATTR_RO(status);
static struct kobj_attribute reset_attribute = __ATTR_RW(reset);
static struct kobj_attribute pwmignore_attribute = __ATTR_RW(pwmignore);

static struct attribute *attrs[] = {
    &status_attribute.attr, &reset_attribute.attr, &pwmignore_attribute.attr, NULL,
};

static struct attribute_group attr_group = {
    .name = "gpio",
    .attrs = attrs,
};

static int gpio_chip_get(struct gpio_chip *chip, unsigned offset) {
    uint16_t pin_state;
    int result = 0;
    offset += GPIO_PIN_OFFSET;
    result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_GPIO, offset, &pin_state, 1);
    rcio_gpio_debug(gpio.rcio->adapter->dev, "Pin [%d] is now 0x%x, result %d", offset, pin_state, result);
    return PX4IO_GPIO_GET_PIN_STATE(pin_state);
}

static int gpio_chip_request(struct gpio_chip *chip, unsigned offset) {
    uint16_t pwm_exported, gpio_exported;
    int read_result, write_result;
    int pwm_running = 0;
    offset += GPIO_PIN_OFFSET;
    
    pwm_running = pwm_check_device_motors_running_count(gpio.rcio);

    if (pwm_running < 0) {
        return pwm_running;
    } else if (pwm_running > 0) {
        //some of motors are running now. we are not allowed to change pin configuration now.
        rcio_gpio_warn(gpio.rcio->adapter->dev, "Exporting warning: you have some of PWM outputs running.\n");
    }

    read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1);
    if (read_result < 0) return read_result;

    if (pwm_exported & (1 << offset)) {
        //this pin is already exported as pwm one
        rcio_gpio_warn(gpio.rcio->adapter->dev, "Exporting warning: this pin [%d] is already exported as PWM\n", offset);
        
        //unexporting it as pwm on device
        pwm_exported &= ~(1 << offset);
        write_result = gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1);
        //and ignoring it in rcio_dkms
        pwmignore_do_ignore_pin(offset);

        if (write_result < 0) return write_result;
    } 
    
	read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);
	gpio_exported |= (1 << offset);
	write_result = gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);

	if (write_result < 0) return write_result;

	PX4IO_GPIO_SET_PIN_GPIO_ENABLE(gpio.pin_states[offset]);
	update_enqueue;

	rcio_gpio_warn(gpio.rcio->adapter->dev, "Exporting pin [%d] OK\n", (int)offset);
	return 0;
    //}
}

static void gpio_chip_free(struct gpio_chip *chip, unsigned offset) {
    uint16_t gpio_exported;
    int read_result, write_result;
    offset += GPIO_PIN_OFFSET;
    rcio_gpio_warn(gpio.rcio->adapter->dev, "Unexporting pin [%d]\n", offset);

    read_result = gpio.rcio->register_get(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);
    if (read_result <= 0) {
        rcio_gpio_err(gpio.rcio->adapter->dev, "Error on register_get %d\n", read_result);
        return;
    }

    //setting the pin value to zero - gpio value is 0, not exported to everything
    gpio.pin_states[offset] = 0;
    rcio_gpio_force_update(gpio.rcio);

    gpio_exported &= ~(1 << offset);
    write_result = (gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1));

    if (write_result <= 0) {
        rcio_gpio_err(gpio.rcio->adapter->dev, "Error on register_set %d\n", read_result);
    }
    
    pwmignore_do_unignore_pin(offset);
}

static void gpio_chip_set(struct gpio_chip *chip, unsigned offset, int value) {
    rcio_gpio_debug(gpio.rcio->adapter->dev, "Setting pin [%d] to value %d\n", offset, value);
    offset += GPIO_PIN_OFFSET;
    PX4IO_GPIO_SET_PIN_GPIO_ENABLE(gpio.pin_states[offset]);
    if (value == 0) {
        PX4IO_GPIO_SET_PIN_STATE_LOW(gpio.pin_states[offset]);
    } else {
        PX4IO_GPIO_SET_PIN_STATE_HIGH(gpio.pin_states[offset]);
    }
    rcio_gpio_force_update(gpio.rcio);
}


//returns direction for signal "offset", 0=out, 1=in
static int gpio_get_direction(struct gpio_chip *chip, unsigned offset) {
    offset += GPIO_PIN_OFFSET;
    rcio_gpio_debug(gpio.rcio->adapter->dev, "get_direction on %d\n", offset);
    return PX4IO_GPIO_GET_PIN_DIRECTION(gpio.pin_states[offset]);
}
static int gpio_direction_input(struct gpio_chip *chip, unsigned offset) {
    offset += GPIO_PIN_OFFSET;
    rcio_gpio_debug(gpio.rcio->adapter->dev, "direction_input on %d\n", offset);
    PX4IO_GPIO_SET_PIN_DIRECTION_INPUT(gpio.pin_states[offset]);
    rcio_gpio_force_update(gpio.rcio);
    return 0;
}
static int gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value) {
    offset += GPIO_PIN_OFFSET;
	rcio_gpio_debug(gpio.rcio->adapter->dev, "direction_output on %d value %d\n", offset, value);
    PX4IO_GPIO_SET_PIN_DIRECTION_OUTPUT(gpio.pin_states[offset]);
    rcio_gpio_force_update(gpio.rcio);
    return 0;
}

static struct gpio_chip gpiochip = {
    .set = gpio_chip_set,
    .get = gpio_chip_get,
    .get_direction = gpio_get_direction,
    .direction_input = gpio_direction_input,
    .direction_output = gpio_direction_output,
    .request = gpio_chip_request,
    .free = gpio_chip_free,
    .label = "Navio PWM pins as GPIO",
    .base = GPIO_CHIP_OFFSET + GPIO_PIN_OFFSET,
    .ngpio = RCIO_PWM_MAX_CHANNELS - GPIO_PIN_OFFSET,
};

bool rcio_gpio_force_update(struct rcio_state *state) {
    uint16_t result = gpio.rcio->register_set(gpio.rcio, PX4IO_PAGE_GPIO, 0, &(gpio.pin_states[0]), RCIO_PWM_MAX_CHANNELS);
    rcio_gpio_debug(gpio.rcio->adapter->dev, "Updating done.");
    return (result >= 0);

}

bool rcio_gpio_update(struct rcio_state *state)
{
    int result = 1;
    if (!gpio_supported) return true;

    if (update_required) {
        update_dequeue;
        result = rcio_gpio_update(state);
    }
    return (result >= 0);
}

int rcio_gpio_probe(struct rcio_state *state)
{
    int ret = 0;
    uint16_t setup_features;

    ret = (state->register_get(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FEATURES, &setup_features, 1));
    if (!(setup_features & PX4IO_P_SETUP_FEATURES_GPIO)) {
        rcio_gpio_err(state->adapter->dev, "GPIO is not supported on this firmware\n");
        gpio_supported = false;
        return -EOPNOTSUPP;
    } else {
        rcio_gpio_warn(state->adapter->dev, "GPIO is supported on this firmware\n");
        gpio_supported = true;
    }

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
        rcio_gpio_warn(state->adapter->dev, "gpiochip added successfully under gpio%d\n", gpiochip.base);
    }

    memset(gpio.pin_states, 0, sizeof(gpio.pin_states));
    gpio.pin_states_updated = 1;

    return true;

}

int rcio_gpio_remove(struct rcio_state *state) {

    int ret = 1;
    if (!gpio_supported) return 1;

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
