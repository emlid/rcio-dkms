#include <linux/delay.h>
#include <linux/module.h>

#include "rcio.h"

int rcio_adc_probe(struct rcio_state *state)
{
    return 0;
}

MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO ADC driver");
MODULE_LICENSE("GPL v2");
