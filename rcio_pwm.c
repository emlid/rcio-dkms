#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#include "rcio.h"
#include "protocol.h"

struct rcio_state *rcio;
struct rcio_pwm *pwm;

struct pwm_output_rc_config {
    uint8_t channel;
    uint16_t rc_min;
    uint16_t rc_trim;
    uint16_t rc_max;
    uint16_t rc_dz;
    uint16_t rc_assignment;
    bool     rc_reverse;
};

static int rcio_pwm_safety_off(struct rcio_state *state);
static int pwm_set_initial_rc_config(struct rcio_state *state);

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

#define RCIO_PWM_MAX_CHANNELS 15
static u16 values[RCIO_PWM_MAX_CHANNELS] = {0};

static u16 alt_frequency = 50;
static bool alt_frequency_updated = false;
static u16 default_frequency = 50;
static bool default_frequency_updated = false;

static bool armed = false;
unsigned long armtimeout;

bool rcio_pwm_update(struct rcio_state *state)
{
    if (alt_frequency_updated) {
        if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_ALTRATE, alt_frequency) < 0) {
            printk(KERN_INFO "alt_frequency not set\n");
        }
        alt_frequency_updated = false;
    }

    if (default_frequency_updated) {
        if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_DEFAULTRATE, default_frequency) < 0) {
            printk(KERN_INFO "default_frequency not set\n");
        }
        default_frequency_updated = false;
    }

    if (time_before(jiffies, armtimeout) && armtimeout > 0) {
        armed = true;
    } else {
        armed = false;
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
    uint16_t ratemap;

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
    
    ratemap = 0xff;
    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_RATES, ratemap) < 0) {
        return -ENOTCONN;
    }

    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_ALTRATE, alt_frequency) < 0) {
        pr_err("alt_frequency not set");
        return -ENOTCONN;
    }   

    if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_DEFAULTRATE, default_frequency) < 0) {
        pr_err("default_frequency not set");
        return -ENOTCONN;
    }   
    
    if (pwm_set_initial_rc_config(state) < 0) {
        pr_err("Initial RC config not set");
        return -EINVAL;
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
    struct rcio_pwm *handle;
    u16 duty_ms;
    u16 new_frequency;

    handle = to_rcio_pwm(chip);
    armtimeout = jiffies + HZ / 10; /* timeout in 0.1s */
    new_frequency = 1000000000 / period_ns;

    if (pwm->hwpwm < 8) {
        if (new_frequency != alt_frequency && duty_ns != 0) {
            alt_frequency = new_frequency;
            alt_frequency_updated = true;
        }
    } else {
        if (new_frequency != default_frequency) {
            default_frequency = new_frequency;
            default_frequency_updated = true;
        }
    }

    duty_ms = duty_ns / 1000;
    values[pwm->hwpwm] = duty_ms;

//    printk(KERN_INFO "hwpwm=%d duty=%d period=%d duty_ms=%u freq=%u\n", pwm->hwpwm, duty_ns, period_ns, duty_ms, alt_frequency);

    return 0;
}

static int rcio_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
    return 0;
}

static void rcio_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{

}

static int pwm_set_initial_rc_channel_config(struct rcio_state *state, struct pwm_output_rc_config *config)
{
    uint16_t regs[PX4IO_P_RC_CONFIG_STRIDE];
    uint16_t offset;

    if (config->channel >= RCIO_PWM_MAX_CHANNELS) {
        /* fail with error */
        return -E2BIG;
    }

    /* copy values to registers in IO */
    offset = config->channel * PX4IO_P_RC_CONFIG_STRIDE;
    regs[PX4IO_P_RC_CONFIG_MIN]        = config->rc_min;
    regs[PX4IO_P_RC_CONFIG_CENTER]     = config->rc_trim;
    regs[PX4IO_P_RC_CONFIG_MAX]        = config->rc_max;
    regs[PX4IO_P_RC_CONFIG_DEADZONE]   = config->rc_dz;
    regs[PX4IO_P_RC_CONFIG_ASSIGNMENT] = config->rc_assignment;
    regs[PX4IO_P_RC_CONFIG_OPTIONS]    = PX4IO_P_RC_CONFIG_OPTIONS_ENABLED;

    if (config->rc_reverse) {
        regs[PX4IO_P_RC_CONFIG_OPTIONS] |= PX4IO_P_RC_CONFIG_OPTIONS_REVERSE;
    }

    return state->register_set(state, PX4IO_PAGE_RC_CONFIG, offset, regs, PX4IO_P_RC_CONFIG_STRIDE);
}

static int pwm_set_initial_rc_config(struct rcio_state *state)
{
    struct pwm_output_rc_config config = {
        .rc_min = 900,
        .rc_trim = 1500,
        .rc_max = 2000,
        .rc_dz = 10,
    };

    for (int channel = 0; channel < RCIO_PWM_MAX_CHANNELS; channel++) {
        config.channel = channel;
        if (pwm_set_initial_rc_channel_config(state, &config) < 0) {
            pr_err("RC config %d not set", channel);
        } else {
            pr_debug("RC config %d set successfully", channel);
        }

    }

    return 0;
}


EXPORT_SYMBOL_GPL(rcio_pwm_probe);
EXPORT_SYMBOL_GPL(rcio_pwm_remove);
EXPORT_SYMBOL_GPL(rcio_pwm_update);
MODULE_AUTHOR("Georgii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO PWM driver");
MODULE_LICENSE("GPL v2");
