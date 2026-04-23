obj-m += spi_ili9225.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

EXTRA_CFLAGS += -Wall

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean