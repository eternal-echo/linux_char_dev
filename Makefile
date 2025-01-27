KERN_DIR = /lib/modules/$(shell uname -r)

obj-m	+= char_dev.o

all:
	make -C $(KERN_DIR)/build M=$(PWD) modules

load:
	sudo insmod char_dev.ko
	sudo mknod /dev/static_chardev c 235 0
	sudo chmod 666 /dev/static_chardev

# 卸载模块
unload:
	sudo rm -f /dev/static_chardev
	sudo rmmod char_dev

clean:
	make -C $(KERN_DIR)/build M=$(PWD) clean

.PHONY: all load unload clean