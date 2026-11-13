/**
 ******************************************************************************
 * @file    vendor_protocol.c
 * @brief   TZ130TM gimbal vendor protocol implementation
 * @note    Based on T-series protocol V1.4 (2025-04-01)
 ******************************************************************************
 */

#include "vendor_protocol.h"
#include <string.h>

/* ========== 辅助函数实现 ========== */

/**
 * @brief 计算XOR校验
 * @note  按厂家规范：从LEN字节到数据末字节逐字节异或
 */
uint8_t Vendor_CalcXOR(const uint8_t *data, uint8_t len)
{
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < len; i++) {
        xor_val ^= data[i];
    }
    return xor_val;
}

/**
 * @brief 写入32位整数（小端序）
 */
void Vendor_WriteLE_Int32(uint8_t *buf, int32_t value)
{
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    buf[2] = (uint8_t)((value >> 16) & 0xFF);
    buf[3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief 写入16位整数（小端序）
 */
void Vendor_WriteLE_Int16(uint8_t *buf, int16_t value)
{
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * @brief 读取32位整数（小端序）
 */
int32_t Vendor_ReadLE_Int32(const uint8_t *buf)
{
    uint32_t bits = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                    ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/**
 * @brief 读取16位整数（小端序）
 */
int16_t Vendor_ReadLE_Int16(const uint8_t *buf)
{
    int16_t value = 0;
    value |= ((int16_t)buf[0]);
    value |= ((int16_t)buf[1] << 8);
    return value;
}

/**
 * @brief 读取16位无符号整数（小端序）
 */
uint16_t Vendor_ReadLE_UInt16(const uint8_t *buf)
{
    uint16_t value = 0;
    value |= ((uint16_t)buf[0]);
    value |= ((uint16_t)buf[1] << 8);
    return value;
}

/* 0x0072: yaw/pitch are signed centidegrees, little endian. */
uint8_t Vendor_BuildAnglePointingFrame(uint8_t *out_buf, int16_t yaw_deg100,
                                      int16_t pitch_deg100) {
    out_buf[0] = VENDOR_HEADER_CTRL_0;
    out_buf[1] = VENDOR_HEADER_CTRL_1;
    out_buf[2] = VENDOR_HEADER_CTRL_2;
    out_buf[3] = 6;
    Vendor_WriteLE_Int16(out_buf + 4, VENDOR_CMD_ANGLE_POINTING);
    Vendor_WriteLE_Int16(out_buf + 6, yaw_deg100);
    Vendor_WriteLE_Int16(out_buf + 8, pitch_deg100);
    out_buf[10] = Vendor_CalcXOR(out_buf + 3, 7);
    return 11;
}

/* ========== 帧解析函数实现 ========== */

/**
 * @brief 初始化解析器
 */
void Vendor_ParserInit(VendorParser_t *parser)
{
    memset(parser, 0, sizeof(VendorParser_t));
    parser->state = VENDOR_PARSE_IDLE;
}

/**
 * @brief 流式状态机解析单字节
 * @note  支持半包、粘包、自动错误恢复
 */
VendorParseResult_t Vendor_ParseByte(VendorParser_t *parser, uint8_t byte)
{
    switch (parser->state) {

        case VENDOR_PARSE_IDLE:
            // 等待帧头第一字节
            if (byte == VENDOR_HEADER_STATUS_0) {  // 0xFC (状态帧)
                parser->is_status_frame = 1;
                parser->state = VENDOR_PARSE_HEADER1;
            } else if (byte == VENDOR_HEADER_CTRL_0) {  // 0xFB (控制帧，暂不处理)
                parser->is_status_frame = 0;
                parser->state = VENDOR_PARSE_HEADER1;
            }
            break;

        case VENDOR_PARSE_HEADER1:
            // 检查第二字节
            if (byte == VENDOR_HEADER_CTRL_1) {  // 0x2C
                parser->state = VENDOR_PARSE_HEADER2;
            } else {
                parser->state = VENDOR_PARSE_IDLE;  // 错误，重新开始
            }
            break;

        case VENDOR_PARSE_HEADER2:
            // 检查第三字节
            if (parser->is_status_frame && byte == VENDOR_HEADER_STATUS_2) {  // 0x55
                parser->state = VENDOR_PARSE_LENGTH;
                parser->xor_calc = 0;
            } else if (!parser->is_status_frame && byte == VENDOR_HEADER_CTRL_2) {  // 0xAA
                parser->state = VENDOR_PARSE_LENGTH;
                parser->xor_calc = 0;
            } else {
                parser->state = VENDOR_PARSE_IDLE;  // 错误，重新开始
            }
            break;

        case VENDOR_PARSE_LENGTH:
            // 接收长度字节
            parser->length = byte;
            parser->index = 0;
            parser->xor_calc ^= byte;

            // 长度合理性检查（2~62字节）
            if (parser->length < 2 || parser->length > 62) {
                parser->state = VENDOR_PARSE_IDLE;  // 长度异常
            } else {
                parser->state = VENDOR_PARSE_PAYLOAD;
            }
            break;

        case VENDOR_PARSE_PAYLOAD:
            // 接收Payload数据
            parser->payload[parser->index++] = byte;
            parser->xor_calc ^= byte;

            if (parser->index >= parser->length) {
                // Payload接收完毕，等待XOR
                parser->state = VENDOR_PARSE_XOR;
            }
            break;

        case VENDOR_PARSE_XOR:
            // 校验XOR
            parser->state = VENDOR_PARSE_IDLE;  // 无论成功失败都回到IDLE

            if (byte == parser->xor_calc) {
                // XOR校验通过
                if (parser->is_status_frame) {
                    return VENDOR_PARSE_RESULT_STATUS_OK;
                }
                // 控制帧暂不处理
            } else {
                // XOR校验失败
                return VENDOR_PARSE_RESULT_ERROR;
            }
            break;

        default:
            parser->state = VENDOR_PARSE_IDLE;
            break;
    }

    return VENDOR_PARSE_RESULT_NONE;  // 需要更多数据
}

/**
 * @brief 从payload提取状态数据
 * @note  payload格式：[0..1]=命令0x0014(LE), [2..61]=60字节数据
 *        数据区偏移是相对payload[2]开始的
 */
bool Vendor_ExtractStatus(const uint8_t *payload, GimbalStatus_t *status)
{
    // 检查命令字是否是0x0014
    uint16_t cmd = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
    if (cmd != VENDOR_CMD_STATUS_FEEDBACK) {  // 0x0014
        return false;
    }

    // 数据区从payload[2]开始，相当于文档中的payload[0]
    const uint8_t *data = &payload[2];

    // 清空结构体
    memset(status, 0, sizeof(GimbalStatus_t));

    // 提取字段（按文档表14的偏移）

    // payload[1]: bit7 = 吊舱准备状态
    status->self_test = data[1];
    status->gimbal_ready = (data[1] >> 7) & 0x01;

    // payload[6]: 伺服状态
    status->servo_state = data[6];

    // payload[7..8]: 方位角×100 (int16 LE)
    status->azimuth_deg100 = Vendor_ReadLE_Int16(&data[7]);

    // payload[9..10]: 俯仰角×100 (int16 LE)
    status->pitch_deg100 = Vendor_ReadLE_Int16(&data[9]);
    status->roll_deg100 = Vendor_ReadLE_Int16(&data[11]);
    status->attitude_azimuth_deg100 = Vendor_ReadLE_Int16(&data[39]);
    status->attitude_pitch_deg100 = Vendor_ReadLE_Int16(&data[41]);
    status->attitude_roll_deg100 = Vendor_ReadLE_Int16(&data[43]);

    // payload[25..26]: 激光距离×0.1m (uint16 LE)
    status->laser_distance_dm = Vendor_ReadLE_UInt16(&data[25]);

    // payload[27..30]: 目标经度×10^7 (int32 LE)
    status->target_lon_e7 = Vendor_ReadLE_Int32(&data[27]);

    // payload[31..34]: 目标纬度×10^7 (int32 LE)
    status->target_lat_e7 = Vendor_ReadLE_Int32(&data[31]);

    // payload[35..36]: 目标海拔(米) (int16 LE)
    status->target_alt_m = Vendor_ReadLE_Int16(&data[35]);

    // payload[46]: 命令回显
    status->cmd_echo = data[46];

    // payload[47]: 姿态完成
    status->attitude_complete = data[47];

    // payload[57]: 跟踪状态
    status->tracking_state = data[57];

    // 标记数据有效
    status->valid = true;

    return true;
}
