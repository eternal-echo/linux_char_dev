#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>  // copy_to_user, copy_from_user
#include <linux/proc_fs.h>  // 添加proc文件支持接口
#include <linux/seq_file.h> // 添加seq接口支持

#include "char_dev.h"

static int chardev_major = CHARDEV_DEVICE_MAJOR;
static int chardev_minor = CHARDEV_DEVICE_MINOR;
static int chardev_count = CHARDEV_DEVICE_COUNT;
static char chardev_name[] = CHARDEV_DEVICE_NAME;

static char_dev_t *chardevs = NULL;
/* proc 文件接口，实现通过 /proc/chardev 查看当前设备信息 */
static void *chardev_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= chardev_count)
        return NULL;
    return (void *)(chardevs + *pos);
}

static void *chardev_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= chardev_count)
        return NULL;
    return (void *)(chardevs + *pos);
}

static void chardev_seq_stop(struct seq_file *s, void *v)
{
    // 无需清理
}

static int chardev_seq_show(struct seq_file *s, void *v)
{
    char_dev_t *dev = (char_dev_t *)v;
    int idx = dev - chardevs;
    if (idx < chardev_count) {
        size_t len = ring_buffer_space_used(&dev->buffer);
        seq_printf(s, "Device %d: buffer length = %zu\n", idx, len);
        // 获取读指针和长度
        char *raddr;
        size_t rlen;
        RING_BUFFER_GET_READ(&dev->buffer, raddr, rlen);
        seq_printf(s, "Data: %.*s\n", (int)rlen, raddr);
    }
    return 0;
}

static const struct seq_operations chardev_seq_ops = {
    .start = chardev_seq_start,
    .next  = chardev_seq_next,
    .stop  = chardev_seq_stop,
    .show  = chardev_seq_show,
};

static int chardev_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &chardev_seq_ops);
}

static const struct proc_ops chardev_proc_ops = {
    .proc_open = chardev_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

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
    char *raddr;
    size_t rlen, total = 0;
    ssize_t ret = 0;
    
    mutex_lock(&chardev->lock);

    while (count > 0) {
        RING_BUFFER_GET_READ(&chardev->buffer, raddr, rlen);
        if (rlen == 0) {
            break;
        }
        if (rlen > count) {
            rlen = count;
        }
        if (copy_to_user(user_buffer + total, raddr, rlen)) {
            ret = -EFAULT;
            goto out;
        }
        ring_buffer_seek_read(&chardev->buffer, rlen);
        total += rlen;
        count -= rlen;
    }

    *position += total;  // 更新读取位置
    ret = total;

    printk(KERN_INFO "Reading %zu bytes from device\n", total);

out:
    mutex_unlock(&chardev->lock);
    return ret;
}


// 写入设备
static ssize_t char_dev_write(struct file *file, const char __user *user_buffer,
                              size_t count, loff_t *position)
{
    char_dev_t *chardev = file->private_data;
    char *waddr;
    size_t wlen, total = 0;
    ssize_t ret = 0;

    mutex_lock(&chardev->lock);

    while (count > 0) {
        RING_BUFFER_GET_WRITE(&chardev->buffer, waddr, wlen);
        if (wlen == 0) {
            ret = -ENOSPC;
            goto out;
        }
        if (wlen > count) {
            wlen = count;
        }
        if (copy_from_user(waddr, user_buffer + total, wlen)) {
            ret = -EFAULT;
            goto out;
        }
        ring_buffer_seek_write(&chardev->buffer, wlen);
        total += wlen;
        count -= wlen;
    }

    *position += total;  
    ret = total;
    printk(KERN_INFO "Writing %zu bytes to device\n", total);

out:
    mutex_unlock(&chardev->lock);
    return ret;
}

// ioctl 处理函数
static long char_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    char_dev_t *chardev = file->private_data;
    int result = 0;

    mutex_lock(&chardev->lock);
    switch (cmd) {
    case IOCTL_GET_LEN:
        size_t len = ring_buffer_space_used(&chardev->buffer);
        if (copy_to_user((int __user *)arg, &len, sizeof(int))){
            result = -EFAULT;
            goto out;
        }
        printk(KERN_INFO "IOCTL_GET_LEN: %zu\n", len);
        break;

    case IOCTL_CLR_BUF:
        ring_buffer_init(&chardev->buffer);
        printk(KERN_INFO "IOCTL_CLR_BUF: Buffer cleared\n");
        break;

    default:
        return -EINVAL;
    }

out:
    mutex_unlock(&chardev->lock);
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

    remove_proc_entry("chardev", NULL);

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
        ring_buffer_init(&chardevs[i].buffer);
        mutex_init(&chardevs[i].lock);  // 初始化 mutex
        result = cdev_add(&chardevs[i].cdev, dev + i, 1);
        if (result < 0) {
            printk(KERN_ERR "Failed to add char device\n");
            goto fail_cdev;
        }
    }

    struct proc_dir_entry *proc_entry = proc_create("chardev", 0, NULL, &chardev_proc_ops);
    if (!proc_entry) {
        printk(KERN_ERR "Failed to create /proc/chardev\n");
        goto fail_cdev;
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
    return result;
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("eternal-echo");
MODULE_DESCRIPTION("A simple character device driver for reading and writing");
