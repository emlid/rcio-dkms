# rcio-dkms
This is a kernel module for Raspberry Linux allowing it to communicate with Navio modules.


# Building
## Native compilation on Pi

Requirements

- Navio Raspbian running on Pi

Building

    # Get source
    pi@navio:~ $ git clone git@github.com:emlid/rcio-dkms-private.git
    pi@navio:~/rcio-dkms-private $ make

Updating dkms

    # Note: use valid version number (in this case 0.6.6)
    pi@navio:~/rcio-dkms-private $ sudo dkms remove rcio/0.6.6 --all
    # Note: you can use something like 
    # $ version=`dkms status | head -1 | awk -F, '{print $2;}' | sed 's/ /rcio\//g'`
    # $ sudo dkms remove $version --all
    # to auto-detect the current running version
    pi@navio:~/rcio-dkms-private $ sudo dkms install .

Re-launch kernel module

    pi@navio:~/rcio-dkms-private $ sudo modprobe -r rcio_spi
    pi@navio:~/rcio-dkms-private $ sudo insmod rcio_core.ko
    pi@navio:~/rcio-dkms-private $ sudo insmod rcio_spi.ko

Check whether it is alive:

    pi@navio:~ $ cat /sys/kernel/rcio/status/alive 
    # This should print "1"


## Cross-compilation on Linux machine

Requirements:

- gcc-arm-linux-gnueabi
- Navio Raspberry Kernel sources (will be downloaded in the next section)

Building Raspberry Linux Kernel

    # Download them
    laptop:~$ git clone https://github.com/emlid/linux-rpi-private/commits/feat-wifi-broadcast-rebase
    # Switch to the commit which is stated in pi's uname -a output
    # Thus, if
    # pi@navio:~ $ uname -a
    # Linux navio 4.4.36-a7765e7-emlid-v7+ #41 SMP PREEMPT Mon Mar 20 18:48:32 MSK 2017 armv7l GNU/Linux
    # you have to do
    laptop:~/linux-rpi-private $ git checkout a7765e7
    # Setup variables and configs
    laptop:~/linux-rpi-private $ export ARCH=arm 
    laptop:~/linux-rpi-private $ export CROSS_COMPILE=arm-linux-gnueabihf-
    laptop:~/linux-rpi-private $ make bcm2709_emlid_defconfig
    # Make it
    laptop:~/linux-rpi-private $ make -j5

Building rcio

    laptop:~$ git clone git@github.com:emlid/rcio-dkms-private.git
    laptop:~/rcio-dkms-private $ export ARCH=arm
    laptop:~/rcio-dkms-private $ export CROSS_COMPILE=arm-linux-gnueabihf-
    # Point KERNEL_SOURCE to where they actually are
    laptop:~/rcio-dkms-private $ export KERNEL_SOURCE=../linux-rpi-private/
    laptop:~/rcio-dkms-private $ make -j5
    # Here we should have a bunch of *.ko files.

Updating kernel modules on Raspberry

    pi@navio:~ $ mkdir rcio-dkms-crosscompiled


    # Point this to your Raspberry's IP address
    laptop:~/rcio-dkms-private $ export PI_IP=192.168.1.210
    laptop:~/rcio-dkms-private $ export PI_CROSSCOMPILED_PATH='~/rcio-dkms-crosscompiled'
    laptop:~/rcio-dkms-private $ rsync -vP *.ko pi@$PI_IP:"$PI_CROSSCOMPILED_PATH"


    pi@navio:~/rcio-dkms-crosscompiled $ sudo modprobe -r rcio_spi
    pi@navio:~/rcio-dkms-crosscompiled $ sudo insmod rcio_core.ko
    pi@navio:~/rcio-dkms-crosscompiled $ sudo insmod rcio_spi.ko

Check whether it is alive:

    pi@navio:~ $ cat /sys/kernel/rcio/status/alive 
    # This should print "1" again


# Troubleshooting

In case of any troubles

1. Double-check all necessary requirements
2. Double-check all environment variables and paths
3. Try 
    $ make clean

and/or

    $ make distclean

