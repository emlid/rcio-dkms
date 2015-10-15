#include <linux/delay.h>
#include <linux/module.h>

#include "rcio.h"
#include "protocol.h"

static u16 adc_get_raw_adc(struct rcio_state *state, u8 channel)
{
    return state->register_get_byte(state, PX4IO_PAGE_RAW_ADC_INPUT, channel);
}

int rcio_adc_probe(struct rcio_state *state)
{
    for (int i = 0; i < 6; i++) {
        printk(KERN_INFO "ch%d: %d\n", i, adc_get_raw_adc(state, i));
    }

    return 0;
}

EXPORT_SYMBOL_GPL(rcio_adc_probe);
MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO ADC driver");
MODULE_LICENSE("GPL v2");
