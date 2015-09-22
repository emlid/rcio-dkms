obj-m += rcio_core.o
obj-m += rcio_spi.o

PWD := $(shell pwd)

ccflags-y := -std=gnu99

default:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules
	dtc -@ -I dts -O dtb rcio-overlay.dts -o rcio-overlay.dtb

clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	$(RM) rcio-overlay.dtb
