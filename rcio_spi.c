#include <linux/delay.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "rcio.h"

struct rcio_state st;

static int rcio_spi_probe(struct spi_device *spi)
{
	int ret;

	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 5000000;
	spi->bits_per_word = 8;

	ret = spi_setup(spi);

	if (ret < 0)
		return ret;

	st.client = spi;

    u8 cmd[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    for (int i = 0; i < 10; i++) {
        ret = spi_write_then_read(spi, cmd, sizeof(cmd), NULL, 0);
    }

	return rcio_probe(&st);
}

static int rcio_spi_remove(struct spi_device *spi)
{
    return rcio_remove(&st);
}

static const struct spi_device_id rcio_id[] = {
	{ "rcio", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, rcio_id);

static struct spi_driver rcio_driver = {
	.driver = {
		.name = "rcio",
		.owner = THIS_MODULE,
	},
	.id_table = rcio_id,
	.probe = rcio_spi_probe,
	.remove = rcio_spi_remove,
};
module_spi_driver(rcio_driver);

MODULE_AUTHOR("Gerogii Staroselskii <georgii.staroselskii@emlid.com>");
MODULE_DESCRIPTION("RCIO spi driver");
MODULE_LICENSE("GPL v2");
