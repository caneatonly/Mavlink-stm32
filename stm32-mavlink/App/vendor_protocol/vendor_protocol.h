/**
 ******************************************************************************
 * @file    vendor_protocol.h
 * @brief   TZ130TM gimbal vendor protocol codec (T-series V1.4)
 * @note    Single-request communication mode (单次请求式)
 ******************************************************************************
 */

#ifndef __VENDOR_PROTOCOL_H__
#define __VENDOR_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

/* ========== 帧解析器状态机 ========== */

/**
 * @brief 厂家协议帧解析状态
 */
typedef enum {
    VENDOR_PARSE_IDLE = 0,      // 空闲，等待帧头
    VENDOR_PARSE_HEADER1,       // 收到第一字节
    VENDOR_PARSE_HEADER2,       // 收到第二字节
    VENDOR_PARSE_HEADER3,       // 收到第三字节
    VENDOR_PARSE_LENGTH,        // 收到长度字节
    VENDOR_PARSE_PAYLOAD,       // 接收Payload中
    VENDOR_PARSE_XOR            // 接收XOR校验
} VendorParseState_t;

/**
 * @brief 厂家协议帧解析器
 * @note  流式状态机，支持半包、粘包、错误恢复
 */
typedef struct {
    VendorParseState_t state;   // 当前状态
    uint8_t  is_status_frame;   // 1=状态帧(FC 2C 55), 0=控制帧(FB 2C AA)
    uint8_t  length;            // LEN字段值（命令2字节+数据N字节）
    uint8_t  payload[64];       // Payload缓冲区（最大62字节）
    uint8_t  index;             // 当前接收索引
    uint8_t  xor_calc;          // XOR校验计算值
} VendorParser_t;

/**
 * @brief 帧解析结果
 */
typedef enum {
    VENDOR_PARSE_RESULT_NONE = 0,       ///< 需要更多数据
    VENDOR_PARSE_RESULT_STATUS_OK,      ///< 成功解析状态帧
    VENDOR_PARSE_RESULT_ERROR           ///< 解析错误（XOR不匹配等）
} VendorParseResult_t;

/* 0x0072, out_buf requires 11 bytes; inputs are signed centidegrees. */
uint8_t Vendor_BuildAnglePointingFrame(uint8_t *out_buf, int16_t yaw_deg100,
                                      int16_t pitch_deg100);

/* ========== 帧解析函数 ========== */

/**
 * @brief 初始化解析器
 * @param parser 解析器指针
 */
void Vendor_ParserInit(VendorParser_t *parser);

/**
 * @brief 喂入单字节进行解析（流式状态机）
 * @param parser 解析器指针
 * @param byte   输入字节
 * @return 解析结果
 * @note  支持半包、粘包、错误恢复
 */
VendorParseResult_t Vendor_ParseByte(VendorParser_t *parser, uint8_t byte);

/**
 * @brief 从解析后的payload提取状态数据
 * @param payload  已解析的payload（60字节数据区）
 * @param status   输出状态结构（输出）
 * @return true=成功, false=失败
 * @note  payload[0..1]是命令字0x0014，payload[2..]是60字节数据
 */
bool Vendor_ExtractStatus(const uint8_t *payload, GimbalStatus_t *status);

/* ========== 辅助函数 ========== */

/**
 * @brief 计算XOR校验
 * @param data 数据指针
 * @param len  数据长度
 * @return XOR校验值
 */
uint8_t Vendor_CalcXOR(const uint8_t *data, uint8_t len);

/**
 * @brief 写入32位整数（小端序）
 * @param buf   缓冲区
 * @param value 值
 */
void Vendor_WriteLE_Int32(uint8_t *buf, int32_t value);

/**
 * @brief 写入16位整数（小端序）
 * @param buf   缓冲区
 * @param value 值
 */
void Vendor_WriteLE_Int16(uint8_t *buf, int16_t value);

/**
 * @brief 读取32位整数（小端序）
 * @param buf 缓冲区
 * @return 读取的值
 */
int32_t Vendor_ReadLE_Int32(const uint8_t *buf);

/**
 * @brief 读取16位整数（小端序）
 * @param buf 缓冲区
 * @return 读取的值
 */
int16_t Vendor_ReadLE_Int16(const uint8_t *buf);

/**
 * @brief 读取16位无符号整数（小端序）
 * @param buf 缓冲区
 * @return 读取的值
 */
uint16_t Vendor_ReadLE_UInt16(const uint8_t *buf);

#endif /* __VENDOR_PROTOCOL_H__ */
