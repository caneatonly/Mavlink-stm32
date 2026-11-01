/**
 ******************************************************************************
 * @file    ring_buffer.c
 * @brief   Ring buffer implementation
 ******************************************************************************
 */

#include "ring_buffer.h"
#include <string.h>

/**
 * @brief 初始化环形缓冲区
 */
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buf, uint16_t size)
{
    rb->buffer = buf;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief 写入单字节
 */
bool RingBuffer_Write(RingBuffer_t *rb, uint8_t byte)
{
    uint16_t next_head = (rb->head + 1) % rb->size;

    // 检查缓冲区是否满
    if (next_head == rb->tail) {
        return false;  // 缓冲区满
    }

    rb->buffer[rb->head] = byte;
    rb->head = next_head;

    return true;
}

/**
 * @brief 读取单字节
 */
bool RingBuffer_Read(RingBuffer_t *rb, uint8_t *byte)
{
    // 检查缓冲区是否空
    if (rb->head == rb->tail) {
        return false;  // 缓冲区空
    }

    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;

    return true;
}

/**
 * @brief 获取可用字节数
 */
uint16_t RingBuffer_Available(RingBuffer_t *rb)
{
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return rb->size - rb->tail + rb->head;
    }
}

/**
 * @brief 清空缓冲区
 */
void RingBuffer_Clear(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief 查看缓冲区数据（不移动读指针）
 */
bool RingBuffer_Peek(RingBuffer_t *rb, uint16_t offset, uint8_t *byte)
{
    // 检查偏移是否超出可用数据范围
    if (offset >= RingBuffer_Available(rb)) {
        return false;
    }

    uint16_t pos = (rb->tail + offset) % rb->size;
    *byte = rb->buffer[pos];

    return true;
}
