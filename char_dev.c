#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>  // copy_to_user, copy_from_user

#define DEVICE_NAME "static_chardev"  // 设备名称
#define DEVICE_MAJOR 0  // 自动分配主设备号
#define BUFFER_SIZE 1024  // FIFO缓冲区大小

static int open_count = 0;
static struct cdev static_cdev;  // 字符设备结构
static dev_t dev_num;  // 设备号

// 打开设备
static int char_dev_open(struct inode *inode, struct file *file)
{
    open_count++;
    printk(KERN_INFO "Device opened %d times\n", open_count);
    return 0;
}

// 关闭设备
static int char_dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Device closed\n");
    return 0;
}

// 读取设备
static ssize_t char_dev_read(struct file *file, char __user *user_buffer,
                            size_t count, loff_t *position)
{
    printk(KERN_INFO "Reading %zu bytes from device\n", (size_t)0);
    return (ssize_t)0;
}

// 写入设备
static ssize_t char_dev_write(struct file *file, const char __user *user_buffer,
                             size_t count, loff_t *position)
{
    printk(KERN_INFO "Writing %zu bytes to device\n", count);
    return count;
}

// 文件操作结构体
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = char_dev_read,
    .write = char_dev_write,
    .open = char_dev_open,
    .release = char_dev_release,
};

// 模块初始化
static int __init char_dev_init(void)
{
    int ret;

    // 动态分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }

    // 初始化字符设备
    cdev_init(&static_cdev, &fops);
    static_cdev.owner = THIS_MODULE;

    // 添加字符设备
    ret = cdev_add(&static_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    printk(KERN_INFO "Character device registered with major number %d\n", 
           MAJOR(dev_num));
    return 0;
}

// 模块清理
static void __exit char_dev_exit(void)
{
    cdev_del(&static_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "Character device unregistered\n");
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("eternal-echo");
MODULE_DESCRIPTION("A simple character device driver for reading and writing");
