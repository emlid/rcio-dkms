#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "rcio.h"
#include "protocol.h"
#include "rcio_pwm.h"

#define PERIOD_MIN_NS 2040816

#define rcio_pwm_err(__dev, format, args...)\
        dev_err(__dev, "rcio_pwm: " format, ##args)
#define rcio_pwm_err_ratelimited(__dev, format, args...)\
        dev_err_ratelimited(__dev, "rcio_pwm: " format, ##args)

#define rcio_pwm_warn(__dev, format, args...)\
        dev_warn(__dev, "rcio_pwm: " format, ##args)
#define rcio_pwm_warn_ratelimited(__dev, format, args...)\
        dev_warn_ratelimited(__dev, "rcio_pwm: " format, ##args)

#define rcio_pwm_err(__dev, format, args...)\
        dev_err(__dev, "rcio_pwm: " format, ##args)
#define rcio_pwm_warn(__dev, format, args...)\
        dev_warn(__dev, "rcio_pwm: " format, ##args)

bool adv_timer_config_supported;

extern bool gpio_supported;

extern uint16_t pwm_ignore_writings_mask;

struct pwm_output_rc_config {
    uint8_t channel;
    uint16_t rc_min;
    uint16_t rc_trim;
    uint16_t rc_max;
    uint16_t rc_dz;
    uint16_t rc_assignment;
    bool     rc_reverse;
};

struct rcio_pwm *pwm;

static int rcio_pwm_safety_off(struct rcio_state *state);
static int pwm_set_initial_rc_config(struct rcio_state *state);

static int rcio_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm);
static void rcio_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm);
static int rcio_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns);
static int rcio_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm);
static void rcio_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm);

static int rcio_pwm_create_sysfs_handle(struct rcio_state *state);


struct rcio_pwm {
    struct pwm_chip chip;
    const struct pwm_ops *ops;
    struct rcio_state *state;

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

static u16 values[RCIO_PWM_MAX_CHANNELS] = {0};

static u16 alt_frequency = 50;
static bool alt_frequency_updated = false;
static u16 default_frequency = 50;
static bool default_frequency_updated = false;

static u16 frequencies[RCIO_PWM_TIMER_COUNT] = {50, 50, 50, 50};
static u16 new_frequencies[RCIO_PWM_TIMER_COUNT] = {50, 50, 50, 50};
static bool frequencies_update_required[RCIO_PWM_TIMER_COUNT] = {0};


static bool armed = false;
static unsigned long armtimeout;


static int print_freqs_countdown = 3;
static int force_pwmzero_countdown = 0;

typedef enum {
    SET_GRP1 = 0, SET_GRP2, SET_GRP3, SET_GRP4, SET_ALT, SET_DEF, CLEAR
} freq_update_stage_t;

static bool freq_update_noticed = false;
static void rcio_pwm_update_frequency(struct rcio_state *state, freq_update_stage_t stage) {
    uint16_t clear_values[RCIO_PWM_MAX_CHANNELS];
    switch (stage) {
    case CLEAR:
       for (int i = 0; i < RCIO_PWM_MAX_CHANNELS; i++) clear_values[i] = 0;
       state->register_set(state, PX4IO_PAGE_DIRECT_PWM, 0, clear_values, RCIO_PWM_MAX_CHANNELS);
       return;

    case SET_ALT:
        if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_ALTRATE, alt_frequency) < 0) {
            printk(KERN_INFO "alt_frequency not set\n");
        }
        alt_frequency_updated = false;
        break;

    case SET_DEF:
        if (state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_DEFAULTRATE, default_frequency) < 0) {
            printk(KERN_INFO "default_frequency not set\n");
        }
        default_frequency_updated = false;
        break;

        //gcc and clang support this
    case SET_GRP1 ... SET_GRP4:
        state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_PWM_GROUP1_RATE + stage, new_frequencies[stage]);
        rcio_pwm_warn(pwm->chip.dev, "updated freq on grp %d to %d\n", stage, new_frequencies[stage]);
        frequencies[stage] = new_frequencies[stage];
        break;

    default:
        break;
    }
}

int rcio_pwm_force_update_pin(struct rcio_state *state, int pwm_pin_number) {
    return state->register_set(state, PX4IO_PAGE_DIRECT_PWM, pwm_pin_number, values + pwm_pin_number, 1);
}

static int rcio_set_zero_values(struct rcio_state *state) {
	for (int i = 0; i < RCIO_PWM_MAX_ZEROED_CHANNELS; i++) values[i] = 0;
	return true;
}

int rcio_pwm_force_zero_duty(struct rcio_state *state) {
	rcio_pwm_warn(state->adapter->dev, "Forcing all PWM channels to zero...");
	force_pwmzero_countdown += RCIO_PWM_ZERO_SKIP_UPDATE_CYCLES;
	rcio_pwm_update(state);
	return 0;
}

bool rcio_pwm_update(struct rcio_state *state)
{
    bool some_freq_updated = alt_frequency_updated || default_frequency_updated;
    for (int i = 0; i < RCIO_PWM_TIMER_COUNT; i++) {
        some_freq_updated = some_freq_updated || frequencies_update_required[i];
    }

    if (time_before(jiffies, armtimeout) && armtimeout > 0) {
        armed = true;
    } else {
        armed = false;
        print_freqs_countdown = 3;
    }

    if (armed) {
        if (some_freq_updated && (!freq_update_noticed)) {
            //we detected frequency updates, so we have to change them
            //but first we should clear up values on the device
            //not to have this pwm broken
            rcio_pwm_update_frequency(state, CLEAR);
            freq_update_noticed = true;
            return true;
        }

        if (adv_timer_config_supported) {
            //new way
            for (int i = 0; i < RCIO_PWM_TIMER_COUNT; i++) {
                if (frequencies_update_required[i]) {
                    rcio_pwm_update_frequency(state, i);
                    frequencies_update_required[i] = false;
                    return true;
                }
            }

        } else {
            //old way for backwards-compatibility
            if (alt_frequency_updated) {
                rcio_pwm_update_frequency(state, SET_ALT);
                alt_frequency_updated = false;
                return true;
            }

            if (default_frequency_updated) {
                rcio_pwm_update_frequency(state, SET_DEF);
                default_frequency_updated = false;
                return true;
            }
        }

    }

    freq_update_noticed = false;
    
	if (force_pwmzero_countdown > 0) {
		force_pwmzero_countdown--;
		rcio_set_zero_values(state);
	} else if (force_pwmzero_countdown < 0) {
		force_pwmzero_countdown = 0;
	}
    
    if (armed && (!some_freq_updated )) {
        return state->register_set(state, PX4IO_PAGE_DIRECT_PWM, 0, values, RCIO_PWM_MAX_CHANNELS);
    }

    return true;
}

static int rcio_pwm_safety_off(struct rcio_state *state)
{
    return state->register_set_byte(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FORCE_SAFETY_OFF, PX4IO_FORCE_SAFETY_MAGIC);
}

int rcio_hardware_init(struct rcio_state *state)
{
    uint16_t ratemap;

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

    if (adv_timer_config_supported) {
        //new-way
        for (int i = 0; i < RCIO_PWM_TIMER_COUNT; i++) {
            rcio_pwm_update_frequency(state, i);
        }

    } else {
        //old-way for some backwards compatibility
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
    }

    if (pwm_set_initial_rc_config(state) < 0) {
        pr_err("Initial RC config not set");
        return -EINVAL;
    }

    return 0;
}

int rcio_pwm_probe(struct rcio_state *state)
{
    int ret;
    uint16_t setup_features;

    ret = rcio_pwm_create_sysfs_handle(state);

    if (ret < 0) {
        pr_warn("Generic PWM interface for RCIO not created\n");
        return ret;
    }

    ret = (state->register_get(state, PX4IO_PAGE_SETUP, PX4IO_P_SETUP_FEATURES, &setup_features, 1));
    if (!(setup_features & PX4IO_P_SETUP_FEATURES_ADV_FREQ_CONFIG)) {
        rcio_pwm_err(state->adapter->dev, "Advanced frequency configuration is not supported on this firmware\n");
        adv_timer_config_supported = false;
    } else {
        rcio_pwm_warn(state->adapter->dev, "Advanced frequency configuration is supported on this firmware\n");
        adv_timer_config_supported = true;
    }

    ret =  rcio_hardware_init(state);

    rcio_pwm_warn(pwm->chip.dev, "PWM probe success\n");
    
    return ret;
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

static int rcio_pwm_create_sysfs_handle(struct rcio_state *state)
{
    pwm = kzalloc(sizeof(struct rcio_pwm), GFP_KERNEL);

    if (!pwm)
        return -ENOMEM;

    pwm->chip.ops = &rcio_pwm_ops;
    pwm->chip.npwm = state->pwm_channels_count;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0))
    pwm->chip.can_sleep = false;
#endif
    pwm->chip.dev = state->adapter->dev;
    pwm->state = state;

    return pwmchip_add(&pwm->chip);
}

static int rcio_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
    armed = true;

    return 0;
}

static void rcio_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm_dev)
{
    values[pwm_dev->hwpwm] = 0;
    rcio_pwm_force_update_pin(pwm->state, pwm_dev->hwpwm);
    armed = false;
}

static void print_freqs_error(void) {
	if (print_freqs_countdown < 0) return;
	print_freqs_countdown--;
	printk_ratelimited(KERN_ERR "Please note that PWM frequency on pins 0, 4, 8 and 12 has more priority. \
This error could occur if you put a servo and a motor on the same pwm group \
(like servo on 7th/8th channel while using a copter with 6 motors like hexa/y6)\
For additional information please refer to the documentation.");
}

#define inside_range(x, lower, upper) ((x >= lower) && (x <= upper))

static bool rcio_pwm_should_change_freq_new_way(struct pwm_chip *chip, struct pwm_device *channel, int pwm_group_number, u16 new_frequency) {
    int control_pin = pwm_group_number * 4;
    bool motors_are_stopped = true;

    if (frequencies[pwm_group_number] == new_frequency) {
        //no changes at all
        return false;
    }

    if (channel->hwpwm == control_pin) {
        //this is a control pin, we can apply changes
        return true;
    }

    //if the new requested frequency is not the one
    //that the control pin has, the user is wrong
    //let's check first if all that pwm values are zero
    //if they are, this operation is PROBABLY safe.

    for (int i = control_pin; i < control_pin + RCIO_PWM_CHANNELS_PER_TIMER; i++) {
        if (values[i] != 0) {
            motors_are_stopped = false;
        }
    }

    //we depend on motors running here
    if (motors_are_stopped) {
		/* intentionally leaving commented printk here
        printk_ratelimited(KERN_WARNING "Only frequency changes on pins 0, 4, 8 and 12 count. However, we will change the frequency by now, since the motors are stopped.");*/
        return true;
    } else {
        print_freqs_error();
        return false;
    }
}

static bool rcio_pwm_should_change_freq_old_way(struct pwm_chip *chip, struct pwm_device *channel, u16 new_frequency, int duty_ns) {
	if (channel->hwpwm < 8) {
		if (new_frequency != alt_frequency && duty_ns != 0) {
			if ((channel->hwpwm == 0) || (channel->hwpwm == 4)) {
				//pin from control group
				return true;
			} else {
				//pin not from control group
				print_freqs_error();
				return false;
			}
		}
	} else {
		if (new_frequency != default_frequency && duty_ns != 0) {
			if ((channel->hwpwm == 8) || (channel->hwpwm == 12)) {
				//pin from control group
				return true;
			} else {
				//pin not from control group
				print_freqs_error();
				return false;
			}
		}
	}
	return false;
}

static bool rcio_pwm_should_change_duty_new_way(struct pwm_chip *chip, struct pwm_device *channel, int pwm_group_number, u16 new_frequency) {
	if (frequencies[pwm_group_number] == new_frequency) {
        //there is no difference in frequency, which is fine and safe
        return true;
    }
    /* there is difference in frequency
       which should have been fixed but is not, 
       so there is a configuration error.
       we should not feed the motors with wrong duty cycles */
    return false;
}

static bool rcio_pwm_should_change_duty_old_way(struct pwm_chip *chip, struct pwm_device *channel, u16 new_frequency) {
	if ((channel->hwpwm < 8) && (new_frequency == alt_frequency)) {
		return true;		
	}
	if ((channel->hwpwm >= 8) && (new_frequency == default_frequency)) {
		return true;		
	}
	/* again the same logic as in _change_duty_new_way */
	return false;
}

static bool is_pwm_ignored(int channel) {
    return ((pwm_ignore_writings_mask) >> channel) & 0x01;
}

static int rcio_pwm_config(struct pwm_chip *chip, struct pwm_device *channel, int duty_ns, int period_ns)
{
    u16 duty_ms;
    u16 new_frequency;
    int pwm_group_number = 0;

    if ((pwm_ignore_writings_mask && is_pwm_ignored(channel->hwpwm) && (duty_ns != 0)) ||
		((force_pwmzero_countdown > 0) && (channel->hwpwm < RCIO_PWM_MAX_ZEROED_CHANNELS))) {
        //rcio_pwm_err(pwm->chip.dev, "pin %d is ignored for writing %d", channel->hwpwm, duty_ns);
        values[channel->hwpwm] = 0;
        return 0;
    }

    armtimeout = jiffies + HZ / 10; /* timeout in 0.1s */
    new_frequency = 1000000000 / period_ns;
    
    if (adv_timer_config_supported) {
        //new way

        if (inside_range(channel->hwpwm, 0, 3)) pwm_group_number = 0;
        if (inside_range(channel->hwpwm, 4, 7)) pwm_group_number = 1;
        if (inside_range(channel->hwpwm, 8, 11)) pwm_group_number = 2;
        if (inside_range(channel->hwpwm, 12, 15)) pwm_group_number = 3;

        if (rcio_pwm_should_change_freq_new_way(chip, channel, pwm_group_number, new_frequency)) {
            new_frequencies[pwm_group_number] = new_frequency;
            rcio_pwm_warn(pwm->chip.dev, "requested update on %d to %d", pwm_group_number, new_frequency);
            frequencies_update_required[pwm_group_number] = true;     
        }
        
        if (rcio_pwm_should_change_duty_new_way(chip, channel, pwm_group_number, new_frequency)) {
			duty_ms = duty_ns / 1000;
            values[channel->hwpwm] = duty_ms;       
        } else {
			//change is not safe, better to force duty to zero
			values[channel->hwpwm] = 0;
        }
        
    } else {
        //old way

		if (rcio_pwm_should_change_freq_old_way(chip, channel, new_frequency, duty_ns)) {
			if (channel->hwpwm < 8) {
				alt_frequency = new_frequency;
				alt_frequency_updated = true;
			} else {
				default_frequency = new_frequency;
				default_frequency_updated = true;
			}
        }

		if (rcio_pwm_should_change_duty_old_way(chip, channel, new_frequency)) {
			duty_ms = duty_ns / 1000;
			values[channel->hwpwm] = duty_ms;       
		} else {
			//change is not safe, better to force duty to zero
			values[channel->hwpwm] = 0;
		}
    }
    return 0;
}

static int rcio_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm_dev)
{
    uint16_t pwm_exported, gpio_exported;
    int ret;
    int pin_number;

    int pwm_running = pwm_check_device_motors_running_count(pwm->state);
    pin_number = pwm_dev->hwpwm;

    if (pwm_ignore_writings_mask && is_pwm_ignored(pin_number)) {
        rcio_pwm_err(pwm->state->adapter->dev, "Ignoring pin %d.\n", pin_number);
        return 0;
    }

    if (pwm_running < 0) {
        //we've got some error. let's passthrough it
        return pwm_running;
    } else if (pwm_running > 0) {

        //some of motors are running now. we are not allowed to change pin configuration now.
        rcio_pwm_err(pwm->state->adapter->dev, "Exporting error: you have some of PWM outputs running. Stop them to change pin configuration.\n");
        return -EPERM;
    }
    pin_number = pwm_dev->hwpwm;
    //if fw does not support gpio, it does not support pwm_exported page, so skip it
    if (!gpio_supported) return 0;

    ret = pwm->state->register_get(pwm->state, PX4IO_PAGE_GPIO_EXPORTED, 0, &gpio_exported, 1);
    if (ret < 0) {
        //passthrough an error of being unable to read gpio exported register
        return ret;
    }

    if (gpio_exported & (1 << pin_number)) {
        //this pin is already exported as pwm one
        rcio_pwm_err(pwm->state->adapter->dev, "Exporting error: this pin [%d] is exported as GPIO\n", pin_number);
        return -EPERM;
    } else {
        //this pin is not stated, lets export it
        ret = (pwm->state->register_get(pwm->state, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1));
        pwm_exported |= (1 << pin_number);
        ret = (pwm->state->register_set(pwm->state, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1));

        if (ret < 0) return ret;

        rcio_pwm_warn(pwm->state->adapter->dev, "Exporting pin [%d] OK\n", (int)pin_number);

        return 0;
    }
}

static void rcio_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm_dev)
{
    uint16_t pwm_exported;
    int read_result, write_result;
    int pin_number = pwm_dev->hwpwm;
    values[pin_number] = 0;

    if (pwm_ignore_writings_mask && is_pwm_ignored(pin_number)) {
        rcio_pwm_err(pwm->state->adapter->dev, "Ignoring pin %d.\n", pin_number);
        return;
    }

    //if fw does not support gpio, it does not support pwm_exported page, so skip it
    if (!gpio_supported) return;

    //rcio_pwm_warn(pwm->state->adapter->dev, "Unexporting pin [%d]\n", pin_number);

    read_result = (pwm->state->register_get(pwm->state, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1));
    if (read_result < 0) return;

    pwm_exported &= ~(1 << pin_number);
    write_result = (pwm->state->register_set(pwm->state, PX4IO_PAGE_PWM_EXPORTED, 0, &pwm_exported, 1));

    return;
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

int pwm_check_device_motors_running_count(struct rcio_state *state) {
    //checking if one of pwm channels duty cycles is not null right now on stm32
    uint16_t pwm_values[RCIO_PWM_MAX_CHANNELS];
    uint16_t spinning_count = 0;

    //retrieving current pwm values
    int read_result = (state->register_get(state, PX4IO_PAGE_DIRECT_PWM, 0, pwm_values, RCIO_PWM_MAX_CHANNELS));

    if (read_result < 0) {
		rcio_pwm_warn(pwm->state->adapter->dev, "Error in pwm running count\n");
        return read_result;
    }
    //counting ones that are not zero
    for (int i = 0; i < RCIO_PWM_MAX_CHANNELS; i++) {
        if (pwm_values[i] != 0) spinning_count++;
    }

    return spinning_count;
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
            pr_warn("RC config %d set successfully", channel);
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
