/**
 ******************************************************************************
 * @file    ring_buffer.h
 * @brief   Ring buffer implementation for UART data buffering
 ******************************************************************************
 */

#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 环形缓冲区结构
 */
typedef struct {
    uint8_t  *buffer;   ///< 数据缓冲区指针
    uint16_t  size;     ///< 缓冲区大小
    uint16_t  head;     ///< 写入位置
    uint16_t  tail;     ///< 读取位置
} RingBuffer_t;

/**
 * @brief 初始化环形缓冲区
 * @param rb   环形缓冲区指针
 * @param buf  数据缓冲区指针
 * @param size 缓冲区大小
 */
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buf, uint16_t size);

/**
 * @brief 写入单字节
 * @param rb   环形缓冲区指针
 * @param byte 要写入的字节
 * @return true=成功, false=缓冲区满
 */
bool RingBuffer_Write(RingBuffer_t *rb, uint8_t byte);

/**
 * @brief 读取单字节
 * @param rb   环形缓冲区指针
 * @param byte 读取到的字节（输出）
 * @return true=成功, false=缓冲区空
 */
bool RingBuffer_Read(RingBuffer_t *rb, uint8_t *byte);

/**
 * @brief 获取可用字节数
 * @param rb 环形缓冲区指针
 * @return 可读取的字节数
 */
uint16_t RingBuffer_Available(RingBuffer_t *rb);

/**
 * @brief 清空缓冲区
 * @param rb 环形缓冲区指针
 */
void RingBuffer_Clear(RingBuffer_t *rb);

/**
 * @brief 查看缓冲区数据（不移动读指针）
 * @param rb     环形缓冲区指针
 * @param offset 相对当前读位置的偏移
 * @param byte   读取到的字节（输出）
 * @return true=成功, false=偏移超出可用数据范围
 */
bool RingBuffer_Peek(RingBuffer_t *rb, uint16_t offset, uint8_t *byte);

#endif /* __RING_BUFFER_H__ */
