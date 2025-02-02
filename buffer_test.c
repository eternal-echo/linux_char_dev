#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "buffer.h"

void test_ring_buffer(void) {
    ring_buffer_t rb;
    char write_buf[2048] = "Hello, Ring Buffer!";
    char read_buf[2048] = {0};
    size_t write_len, read_len;

    // 测试1: 初始化
    ring_buffer_init(&rb);
    assert(ring_buffer_space_used(&rb) == 0);
    assert(ring_buffer_space_avail(&rb) == RING_BUFFER_SIZE - 1);
    printf("Test 1 passed: Buffer initialization\n");

    // 测试2: 基本写入读取
    write_len = ring_buffer_write(&rb, write_buf, strlen(write_buf));
    assert(write_len == strlen(write_buf));
    assert(ring_buffer_space_used(&rb) == strlen(write_buf));
    printf("Test 2 passed: Basic write\n");

    read_len = ring_buffer_read(&rb, read_buf, write_len);
    assert(read_len == write_len);
    assert(strcmp(write_buf, read_buf) == 0);
    assert(ring_buffer_space_used(&rb) == 0);
    printf("Test 3 passed: Basic read\n");

    // 测试3: 缓冲区写满
    memset(write_buf, 'A', RING_BUFFER_SIZE);
    write_len = ring_buffer_write(&rb, write_buf, RING_BUFFER_SIZE);
    assert(write_len == RING_BUFFER_SIZE - 1);
    assert(ring_buffer_space_avail(&rb) == 0);
    printf("Test 4 passed: Buffer full\n");

    // 测试4: 环形写入
    ring_buffer_init(&rb);
    // 先写入一些数据
    write_len = ring_buffer_write(&rb, "ABCDEF", 6);
    // 读出部分数据
    read_len = ring_buffer_read(&rb, read_buf, 3);
    assert(read_len == 3);
    // 写入更多数据，测试环形
    write_len = ring_buffer_write(&rb, "123456", 6);
    // 读出所有数据
    memset(read_buf, 0, sizeof(read_buf));
    read_len = ring_buffer_read(&rb, read_buf, RING_BUFFER_SIZE);
    assert(strcmp(read_buf, "DEF123456") == 0);
    printf("Test 5 passed: Circular operation\n");

    printf("All tests passed!\n");
}

int main(void) {
    test_ring_buffer();
    return 0;
}