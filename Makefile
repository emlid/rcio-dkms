obj-m += rcio_core.o
obj-m += rcio_spi.o
obj-m += rcio_adc.o
obj-m += rcio_pwm.o
obj-m += rcio_rcin.o
obj-m += rcio_status.o

ccflags-y := -std=gnu99

KVERSION ?= $(shell uname -r)
KERNEL_SOURCE ?= /lib/modules/$(KVERSION)/build

all:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules
	/usr/local/bin/dtc -@ -I dts -O dtb rcio-overlay.dts -o rcio-overlay.dtb
	cp rcio-overlay.dtb /boot/overlays
	cp rcio-overlay.dtb /boot/overlays/rcio.dtbo

install:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
	$(RM) rcio-overlay.dtb
