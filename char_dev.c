#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>  // copy_to_user, copy_from_user

#include "char_dev.h"

static int chardev_major = CHARDEV_DEVICE_MAJOR;
static int chardev_minor = CHARDEV_DEVICE_MINOR;
static int chardev_count = CHARDEV_DEVICE_COUNT;
static char chardev_name[] = CHARDEV_DEVICE_NAME;

static char_dev_t *chardevs = NULL;

// 打开设备
static int char_dev_open(struct inode *inode, struct file *file)
{
    // 获取设备结构体
    char_dev_t *chardev = container_of(inode->i_cdev, char_dev_t, cdev);
    // 将设备结构体指针存入file->private_data
    file->private_data = chardev;

    printk(KERN_INFO "Char device opened\n");
    return 0;
}

// 关闭设备
static int char_dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Char device closed\n");
    return 0;
}

// 读取设备
static ssize_t char_dev_read(struct file *file, char __user *user_buffer,
                             size_t count, loff_t *position)
{
    char_dev_t *chardev = file->private_data; // 从file->private_data获取设备结构体指针
    size_t bytes_to_read = (count < chardev->len) ? count : chardev->len;
    if (bytes_to_read == 0)
        return 0;

    if (copy_to_user(user_buffer, chardev->buffer, bytes_to_read))
        return -EFAULT;

    printk(KERN_INFO "Reading %zu bytes from device\n", bytes_to_read);
    chardev->len = 0;
    return bytes_to_read;
}

// 写入设备
static ssize_t char_dev_write(struct file *file, const char __user *user_buffer,
                              size_t count, loff_t *position)
{
    
    char_dev_t *chardev = file->private_data;
    size_t remaining_space = BUFFER_SIZE - chardev->len;
    size_t bytes_to_write = (count < remaining_space) ? count : remaining_space;
    if (bytes_to_write == 0)
        return -ENOSPC;

    if (copy_from_user(chardev->buffer + chardev->len, user_buffer, bytes_to_write))
        return -EFAULT;

    printk(KERN_INFO "Writing %zu bytes to device\n", bytes_to_write);
    chardev->len += bytes_to_write;
    return bytes_to_write;
}

// ioctl 处理函数
static long char_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    char_dev_t *chardev = file->private_data;
    int result = 0;

    switch (cmd) {
    case IOCTL_GET_LEN:
        if (copy_to_user((int __user *)arg, &chardev->len, sizeof(int)))
            return -EFAULT;
        printk(KERN_INFO "IOCTL_GET_LEN: %zu\n", chardev->len);
        break;

    case IOCTL_CLR_BUF:
        memset(chardev->buffer, 0, BUFFER_SIZE);
        chardev->len = 0;
        printk(KERN_INFO "IOCTL_CLR_BUF: Buffer cleared\n");
        break;

    default:
        return -EINVAL;
    }

    return result;
}


// 文件操作结构体
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = char_dev_read,
    .write = char_dev_write,
    .open = char_dev_open,
    .release = char_dev_release,
    .unlocked_ioctl = char_dev_ioctl, // 添加 ioctl 处理
};


// 模块清理
static void __exit char_dev_exit(void)
{
    int i;
    dev_t dev = MKDEV(chardev_major, chardev_minor);

    if (chardevs) {
        for (i = 0; i < chardev_count; i++) {
            cdev_del(&chardevs[i].cdev);
        }
        kfree(chardevs);
    }
    if (chardev_major != 0) {
        unregister_chrdev_region(dev, chardev_count);
    }
}


// 模块初始化
static int __init char_dev_init(void)
{
	int result, i;
	dev_t dev = 0;

    // 根据DEVICE_MAJOR选择动态或静态分配设备号
    if (chardev_major) {
        // 静态分配设备号
        dev = MKDEV(chardev_major, chardev_minor);
        result = register_chrdev_region(dev, chardev_count, chardev_name);
        if (result < 0) {
            printk(KERN_ERR "Failed to register static major %d\n", chardev_major);
            goto fail;
        }
    } else {
        // 动态分配设备号
        result = alloc_chrdev_region(&dev, chardev_minor, chardev_count, chardev_name);
        chardev_major = MAJOR(dev);
        if (result < 0) {
            printk(KERN_ERR "Failed to alloc dynamic major\n");
            goto fail;
        }
    }

    // 分配字符设备结构体
    chardevs = kmalloc(chardev_count * sizeof(char_dev_t), GFP_KERNEL);
    if (!chardevs) {
        result = -ENOMEM;
        goto fail_region;
    }
    memset(chardevs, 0, chardev_count * sizeof(char_dev_t));

    // 初始化字符设备
    for (i = 0; i < chardev_count; i++) {
        cdev_init(&chardevs[i].cdev, &fops);
        chardevs[i].cdev.owner = THIS_MODULE;
        chardevs[i].len = 0;
        result = cdev_add(&chardevs[i].cdev, dev + i, 1);
        if (result < 0) {
            printk(KERN_ERR "Failed to add char device\n");
            goto fail_cdev;
        }
    }

    printk(KERN_INFO "Char device driver initialized\n");
    return 0;

fail_cdev:
    // 回滚已添加的字符设备
    for (i--; i >= 0; i--) {
        cdev_del(&chardevs[i].cdev);
    }
    kfree(chardevs);
    chardevs = NULL;

fail_region:
    if (chardev_major != 0) {
        unregister_chrdev_region(dev, chardev_count);
    }

    printk(KERN_ERR "Failed to initialize char device\n");

fail:
    unregister_chrdev_region(dev, chardev_count);
    return result;
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("eternal-echo");
MODULE_DESCRIPTION("A simple character device driver for reading and writing");
