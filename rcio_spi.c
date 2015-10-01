#include <linux/delay.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "rcio.h"

static int rcio_spi_write(struct rcio_adapter *state, u16 address, const char *buffer, size_t length)
{
    struct spi_device *spi = state->client;

    return spi_write(spi, buffer, length);
}

static int rcio_spi_read(struct rcio_adapter *state, u16 address, char *buffer, size_t length)
{
    struct spi_device *spi = state->client;

    return spi_read(spi, buffer, length);
}

struct rcio_adapter st;

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
    st.write = rcio_spi_write;
    st.read = rcio_spi_read;

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
