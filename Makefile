KERN_DIR = /lib/modules/$(shell uname -r)

obj-m	+= char_dev.o

all:
	make -C $(KERN_DIR)/build M=$(PWD) modules

load:
	sudo insmod char_dev.ko
	sudo mknod /dev/static_chardev0 c 235 0
	sudo mknod /dev/static_chardev1 c 235 1
	sudo chmod 666 /dev/static_chardev*
	ls /dev/static_chardev* -al

test:
	echo "Device0 Data" > /dev/static_chardev0
	cat /dev/static_chardev0
	echo "Device1 Data" > /dev/static_chardev1
	cat /dev/static_chardev1
	gcc -o char_test char_test.c
	./char_test

# 卸载模块
unload:
	sudo rm -f /dev/static_chardev*
	sudo rmmod char_dev

clean:
	make -C $(KERN_DIR)/build M=$(PWD) clean

.PHONY: all load unload clean