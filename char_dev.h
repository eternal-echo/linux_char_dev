#ifndef CHAR_DEV_H
#define CHAR_DEV_H

#include <linux/cdev.h>

#define CHARDEV_DEVICE_NAME "chardev"
#define CHARDEV_DEVICE_MAJOR 0
#define CHARDEV_DEVICE_MINOR 0
#define CHARDEV_DEVICE_COUNT 2
#define BUFFER_SIZE 1024

#define IOCTL_GET_LEN  _IOR('c', 1, int)  // 读取缓冲区长度
#define IOCTL_CLR_BUF  _IO('c', 2)        // 清空缓冲区

struct char_dev {
    struct cdev cdev;
    char buffer[BUFFER_SIZE];
    size_t len;
};
typedef struct char_dev char_dev_t;

#endif