obj-m += rcio_core.o 
obj-m += rcio_spi.o
rcio_spi-objs := src/rcio_spi.o
rcio_core-objs := src/rcio_core.o src/rcio_adc.o src/rcio_pwm.o src/rcio_rcin.o src/rcio_status.o src/rcio_safety.o

ccflags-y := -std=gnu99

KVERSION ?= $(shell uname -r)
KERNEL_SOURCE ?= /lib/modules/$(KVERSION)/build

all:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean

package:
	@rm -f *.deb
	@dkms mkdeb --source-only
	@mv -f ../rcio-dkms*.deb .
