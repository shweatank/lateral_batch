obj-m += ioremap.o
obj-m += uart_driver.o
obj-m += blink_led_using_dts.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

