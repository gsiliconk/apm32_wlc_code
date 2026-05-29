#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 环形缓冲区控制结构体。 */
typedef struct {
    uint8_t *buffer;         /* 数据缓冲区指针 */
    uint16_t size;           /* 缓冲区总长度 */
    volatile uint16_t head;  /* 读索引 */
    volatile uint16_t tail;  /* 写索引 */
} ring_buffer_t;

/* 初始化环形缓冲区。 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint16_t size);

/* 判断环形缓冲区是否为空。 */
bool ring_buffer_empty(ring_buffer_t *rb);

/* 判断环形缓冲区是否已满。 */
bool ring_buffer_full(ring_buffer_t *rb);

/* 向环形缓冲区写入一个字节数据。 */
bool ring_buffer_put(ring_buffer_t *rb, uint8_t data);

/* 从环形缓冲区读取一个字节数据。 */
bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data);

/* 获取环形缓冲区中已有数据的字节数。 */
uint16_t ring_buffer_size(ring_buffer_t *rb);

/* 获取从当前读取位置开始的连续可读数据长度。 */
uint16_t ring_buffer_linear_block_size(ring_buffer_t *rb);

#endif