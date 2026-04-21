obj-m += basic_char.o

PWD  := $(shell pwd)
KDIR ?= $(PWD)/../../linux-raspi-6.8.0-1047

ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean