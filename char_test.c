#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>

#define IOCTL_GET_LEN  _IOR('c', 1, int)  // 读取缓冲区长度
#define IOCTL_CLR_BUF  _IO('c', 2)        // 清空缓冲区

#define DEVICE_PATH "/dev/static_chardev0"
#define PROCESS_COUNT 4
#define TEST_ITERATIONS 100

// assert
#define TEST_ASSERT(condition, message) \
    assert(condition)

// #define TEST_ASSERT(condition, message) \
//     do { \
//         if (!(condition)) { \
//             printf("TEST FAILED: %s\n", message); \
//         } \
//     } while(0)

void basic_test() {
    int fd;
    char write_buf[] = "Hello, Char Device!";
    char read_buf[100];
    int len;
    size_t write_len = strlen(write_buf);

    // 打开设备
    fd = open(DEVICE_PATH, O_RDWR);
    TEST_ASSERT(fd >= 0, "Device open failed");

    // 写入测试
    printf("Writing: %s\n", write_buf);
    ssize_t bytes_written = write(fd, write_buf, write_len);
    TEST_ASSERT(bytes_written == write_len, "Write length mismatch");

    // 获取缓冲区长度
    int ioctl_ret = ioctl(fd, IOCTL_GET_LEN, &len);
    TEST_ASSERT(ioctl_ret == 0, "IOCTL_GET_LEN failed");
    TEST_ASSERT(len == write_len, "Buffer length mismatch");
    printf("Buffer length: %d\n", len);

    // 读取测试
    memset(read_buf, 0, sizeof(read_buf));
    ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));
    TEST_ASSERT(bytes_read == write_len, "Read length mismatch");
    TEST_ASSERT(strcmp(write_buf, read_buf) == 0, "Read data mismatch");
    printf("Read: %s\n", read_buf);

    // 验证读取后的长度
    ioctl_ret = ioctl(fd, IOCTL_GET_LEN, &len);
    TEST_ASSERT(ioctl_ret == 0, "IOCTL_GET_LEN failed after clear");
    TEST_ASSERT(len == 0, "Buffer not cleared properly");
    printf("Buffer length after clear: %d\n", len);

    // 写入测试
    printf("Writing: %s\n", write_buf);
    bytes_written = write(fd, write_buf, write_len);
    TEST_ASSERT(bytes_written == write_len, "Write length mismatch");

    // 清除缓冲区测试
    ioctl_ret = ioctl(fd, IOCTL_CLR_BUF);
    TEST_ASSERT(ioctl_ret == 0, "IOCTL_CLR_BUF failed");

    // 验证clr后的长度
    ioctl_ret = ioctl(fd, IOCTL_GET_LEN, &len);
    TEST_ASSERT(ioctl_ret == 0, "IOCTL_GET_LEN failed after clear");
    TEST_ASSERT(len == 0, "Buffer not cleared properly");
    printf("Buffer length after clear: %d\n", len);


    printf("All tests passed successfully!\n");
    close(fd);
}


void writer_process(int index) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    TEST_ASSERT(fd >= 0, "Writer open failed");

    char write_buf[64];
    for(int i = 0; i < TEST_ITERATIONS; i++) {
        snprintf(write_buf, sizeof(write_buf), "P%d-I%d", index, i);
        ssize_t written = write(fd, write_buf, strlen(write_buf));
        TEST_ASSERT(written == strlen(write_buf), "Write failed");
        usleep(rand() % 1000);  // 随机延时
    }
    close(fd);
    exit(0);
}

void reader_process(int index) {
    int fd = open(DEVICE_PATH, O_RDONLY);
    TEST_ASSERT(fd >= 0, "Reader open failed");

    char read_buf[64];
    for(int i = 0; i < TEST_ITERATIONS; i++) {
        memset(read_buf, 0, sizeof(read_buf));
        ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));
        if(bytes_read > 0) {
            printf("Reader %d got: %s\n", index, read_buf);
        }
        usleep(rand() % 1000);
    }
    close(fd);
    exit(0);
}


void concurrent_test() {
    pid_t pids[PROCESS_COUNT * 2];
    int status;

    // 创建读写进程
    for(int i = 0; i < PROCESS_COUNT; i++) {
        // 创建写进程
        pids[i] = fork();
        if(pids[i] == 0) {
            writer_process(i);
        }
        TEST_ASSERT(pids[i] >= 0, "Fork writer failed");

        // 创建读进程
        pids[i + PROCESS_COUNT] = fork();
        if(pids[i + PROCESS_COUNT] == 0) {
            reader_process(i);
        }
        TEST_ASSERT(pids[i + PROCESS_COUNT] >= 0, "Fork reader failed");
    }

    // 等待所有子进程结束
    for(int i = 0; i < PROCESS_COUNT * 2; i++) {
        waitpid(pids[i], &status, 0);
        if(WIFEXITED(status)) {
            printf("Process %d exited with status %d\n", 
                   pids[i], WEXITSTATUS(status));
        }
    }

    printf("Concurrent test completed!\n");
}

int main() {

    basic_test();
    concurrent_test();
    return 0;
}