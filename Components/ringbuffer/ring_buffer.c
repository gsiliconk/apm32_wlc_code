#include "ring_buffer.h"

/* 初始化环形缓冲区。 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint16_t size)
{
    if (rb == NULL) {
        return;
    }

    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

/* 判断环形缓冲区是否为空。 */
bool ring_buffer_empty(ring_buffer_t *rb)
{
    if (rb == NULL) {
        return true;
    }

    return (rb->head == rb->tail);
}

/* 判断环形缓冲区是否已满。 */
bool ring_buffer_full(ring_buffer_t *rb)
{
    uint16_t next_tail;

    if ((rb == NULL) || (rb->buffer == NULL) || (rb->size == 0)) {
        return false;
    }

    next_tail = (rb->tail + 1) % rb->size;

    return (next_tail == rb->head);
}

/* 向环形缓冲区写入一个字节数据。 */
bool ring_buffer_put(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_tail;

    if ((rb == NULL) || (rb->buffer == NULL) || (rb->size == 0)) {
        return false;
    }

    next_tail = (rb->tail + 1) % rb->size;

    if (next_tail == rb->head) {
        return false;
    }

    rb->buffer[rb->tail] = data;
    rb->tail = next_tail;

    return true;
}

/* 从环形缓冲区读取一个字节数据。 */
bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data)
{
    if ((rb == NULL) || (data == NULL) || (rb->buffer == NULL) || (rb->size == 0)) {
        return false;
    }

    if (rb->head == rb->tail) {
        return false;
    }

    *data = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % rb->size;

    return true;
}

/* 获取环形缓冲区中已有数据的字节数。 */
uint16_t ring_buffer_size(ring_buffer_t *rb)
{
    if ((rb == NULL) || (rb->buffer == NULL) || (rb->size == 0)) {
        return 0;
    }

    if (rb->tail >= rb->head) {
        return rb->tail - rb->head;
    } else {
        return rb->size - rb->head + rb->tail;
    }
}

/* 获取从当前读取位置开始的连续可读数据长度。 */
uint16_t ring_buffer_linear_block_size(ring_buffer_t *rb)
{
    if ((rb == NULL) || (rb->buffer == NULL) || (rb->size == 0)) {
        return 0;
    }

    if (rb->tail >= rb->head) {
        return rb->tail - rb->head;
    } else {
        return rb->size - rb->head;
    }
}