
KVERSION = $(shell uname -r)
ARCH=x86

obj-m = custom_device.o

all:
	make ARCH=$(ARCH) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

load:
	sudo insmod custom_device.ko

unload:
	sudo rmmod custom_device.ko