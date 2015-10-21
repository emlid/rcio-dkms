#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#include "rcio.h"
#include "protocol.h"

struct rcio_state *rcio;
struct rcio_pwm *pwm;

static int rcio_pwm_safety_off(struct rcio_state *state);

static int rcio_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void rcio_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static int rcio_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns);
static int rcio_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm);
static void rcio_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm);

static int rcio_pwm_create_sysfs_handle(void);

struct rcio_pwm {
    struct pwm_chip chip;
    struct rcio_state *rcio;
};

static const struct pwm_ops rcio_pwm_ops = {
    .enable = rcio_pwm_enable,
    .disable = rcio_pwm_disable,
    .config = rcio_pwm_config,
    .request = rcio_pwm_request,
    .free = rcio_pwm_free,
    .owner = THIS_MODULE,
};

static inline struct rcio_pwm *to_rcio_pwm(struct pwm_chip *chip)
{
    return container_of(chip, struct rcio_pwm, chip);
}

#define RCIO_PWM_MAX_CHANNELS 8
static u16 values[RCIO_PWM_MAX_CHANNELS] = {0};

static u16 frequency = 50;
static bool frequency_updated = false;

static bool armed = false;

int rcio_pwm_update(struct rcio_state *state)
{
    if (frequency_updated) {
        if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_DEFAULTRATE, frequency) < 0) {
            printk(KERN_INFO "Frequency not set\n");
        }
        frequency_updated = false;
    }

    if (armed) {
        return state->register_set(state, PX4IO_PAGE_DIRECT_PWM, 0, values, RCIO_PWM_MAX_CHANNELS);
    }

    return true;
}

static int rcio_pwm_safety_off(struct rcio_state *state)
{
    return state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FORCE_SAFETY_OFF, PX4IO_FORCE_SAFETY_MAGIC);
}

int rcio_pwm_probe(struct rcio_state *state)
{
    int ret;

    rcio = state;

    if (rcio_pwm_safety_off(state) < 0) {
        pr_err("SAFETY ON");
        return -ENOTCONN;
    }

    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_ARMING, 
                PX4IO_P_SETUP_ARMING_IO_ARM_OK | 
                PX4IO_P_SETUP_ARMING_FMU_ARMED |
                PX4IO_P_SETUP_ARMING_ALWAYS_PWM_ENABLE) < 0) {
        pr_err("ARMING OFF");
        return -ENOTCONN;
    }
    
    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_DEFAULTRATE, frequency) < 0) {
        pr_err("Frequency not set");
        return -ENOTCONN;
    }   
    
    ret = rcio_pwm_create_sysfs_handle();

    if (ret < 0) {
        pr_warn("Generic PWM interface for RCIO not created\n");
        return ret;
    }

    return 0;
}

int rcio_pwm_remove(struct rcio_state *state)
{
    int ret;

    ret = pwmchip_remove(&pwm->chip);

    if (ret < 0)
        return ret;

    kfree(pwm);    

    return 0;
}

static int rcio_pwm_create_sysfs_handle(void)
{
    pwm = kzalloc(sizeof(struct rcio_pwm), GFP_KERNEL);
    
    if (!pwm)
        return -ENOMEM;

    pwm->chip.ops = &rcio_pwm_ops;
    pwm->chip.npwm = RCIO_PWM_MAX_CHANNELS;
    pwm->chip.can_sleep = false;
    pwm->chip.dev = rcio->adapter->dev;

    return pwmchip_add(&pwm->chip);
}

static int rcio_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
    armed = true;

    return 0;
}

static void rcio_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
    armed = false;
}

static int rcio_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns)
{
    struct rcio_pwm *handle = to_rcio_pwm(chip);

    u16 duty_ms = duty_ns / 1000;

    u16 new_frequency = 1000000000 / period_ns;
    if (new_frequency != frequency) {
        frequency = new_frequency;
        frequency_updated = true;
    }

    values[pwm->hwpwm] = duty_ms;

//    printk(KERN_INFO "hwpwm=%d duty=%d period=%d duty_ms=%u freq=%u\n", pwm->hwpwm, duty_ns, period_ns, duty_ms, frequency);

    return 0;
}

static int rcio_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
    return 0;
}

static void rcio_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{

}


EXPORT_SYMBOL_GPL(rcio_pwm_probe);
EXPORT_SYMBOL_GPL(rcio_pwm_remove);
EXPORT_SYMBOL_GPL(rcio_pwm_update);
MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO PWM driver");
MODULE_LICENSE("GPL v2");
