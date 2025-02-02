#ifndef __BUFFER_H__
#define __BUFFER_H__


// 静态分配环形缓冲区大小
#define RING_BUFFER_SIZE 1024
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

// 获取连续可写区域的起始地址和长度
#define RING_BUFFER_GET_WRITE(rb, addr, len) do { \
    addr = (rb)->data + ((rb)->head & RING_BUFFER_MASK); \
    len = min(RING_BUFFER_SIZE - ((rb)->head & RING_BUFFER_MASK), \
             ring_buffer_space_avail(rb)); \
} while(0)

// 获取连续可读区域的起始地址和长度
#define RING_BUFFER_GET_READ(rb, addr, len) do { \
    addr = (rb)->data + ((rb)->tail & RING_BUFFER_MASK); \
    len = min(RING_BUFFER_SIZE - ((rb)->tail & RING_BUFFER_MASK), \
             ring_buffer_space_used(rb)); \
} while(0)


// 示例使用:
/*
char *waddr; size_t wlen;
RING_BUFFER_GET_WRITE(rb, waddr, wlen);
if (copy_from_user(waddr, user_buf, wlen)) 
    return -EFAULT;
ring_buffer_write_advance(rb, wlen);
*/

typedef struct {
    char data[RING_BUFFER_SIZE];
    size_t head;   // 写指针
    size_t tail;   // 读指针
} ring_buffer_t;

// 初始化
static inline void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

// 返回缓冲区可用空间
static inline size_t ring_buffer_space_avail(ring_buffer_t *rb)
{
    return (rb->tail - rb->head - 1) & (RING_BUFFER_SIZE - 1);
}

// 返回缓冲区已用空间
static inline size_t ring_buffer_space_used(ring_buffer_t *rb)
{
    return (rb->head - rb->tail) & (RING_BUFFER_SIZE - 1);
}

// 前进读写指针
static inline void ring_buffer_seek_read(ring_buffer_t *rb, size_t len)
{
    rb->tail = (rb->tail + len) & RING_BUFFER_MASK;
}

static inline void ring_buffer_seek_write(ring_buffer_t *rb, size_t len)
{
    rb->head = (rb->head + len) & RING_BUFFER_MASK;
}

// 写入数据
static inline size_t ring_buffer_write(ring_buffer_t *rb, const char *data, size_t len)
{
    size_t avail = ring_buffer_space_avail(rb);
    size_t written = len > avail ? avail : len;

    // 写入数据
    for (size_t i = 0; i < written; i++) {
        rb->data[rb->head] = data[i];
        rb->head = (rb->head + 1) & (RING_BUFFER_SIZE - 1);
    }

    return written;
}

// 读取数据
static inline size_t ring_buffer_read(ring_buffer_t *rb, char *data, size_t len)
{
    size_t used = ring_buffer_space_used(rb);
    size_t read = len > used ? used : len;

    // 读取数据
    for (size_t i = 0; i < read; i++) {
        data[i] = rb->data[rb->tail];
        rb->tail = (rb->tail + 1) & (RING_BUFFER_SIZE - 1);
    }

    return read;
}

static inline size_t ring_buffer_peek(ring_buffer_t *rb, size_t offset, char *out, size_t len)
{
    size_t used = ring_buffer_space_used(rb);
    if (offset >= used){
        return 0;
    }

    size_t read_len = min(len, used - offset);
    for (size_t i = 0; i < read_len; i++) {
        size_t idx = (rb->tail + offset + i) & RING_BUFFER_MASK;
        out[i] = rb->data[idx];
    }
    return read_len;
}

#endif
