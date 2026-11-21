/**
 ******************************************************************************
 * @file    types.h
 * @brief   Common data structure definitions
 ******************************************************************************
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <stdbool.h>

/* ============ 吊舱状态数据结构 ============ */

/**
 * @brief 吊舱完整状态数据
 * @note  从0x0014状态反馈帧解析得到
 */
typedef struct {
    // 系统状态
    uint8_t  gimbal_ready;          ///< bit7 from payload[1], 1=就绪
    uint8_t  self_test;             ///< Complete data[1] self-test byte
    uint8_t  servo_state;           ///< payload[6], 伺服状态

    // 姿态角度（调试用）
    int16_t  azimuth_deg100;        ///< payload[7..8], 方位角×100
    int16_t  pitch_deg100;          ///< payload[9..10], 俯仰角×100
    int16_t  roll_deg100;           ///< data[11..12], framework roll x100
    int16_t  attitude_azimuth_deg100; ///< data[39..40], attitude azimuth x100
    int16_t  attitude_pitch_deg100; ///< data[41..42], attitude pitch x100
    int16_t  attitude_roll_deg100;  ///< data[43..44], attitude roll x100

    // 跟踪与测距
    uint8_t  tracking_state;        ///< payload[57], 跟踪状态 (0x00/0x01/0x02/0x03/0x05)
    uint16_t laser_distance_dm;     ///< payload[25..26], 激光距离×0.1m (分米)

    // 目标地理坐标
    int32_t  target_lon_e7;         ///< payload[27..30], 经度×10^7
    int32_t  target_lat_e7;         ///< payload[31..34], 纬度×10^7
    int16_t  target_alt_m;          ///< payload[35..36], 海拔高度(米)

    // 控制反馈
    uint8_t  cmd_echo;              ///< payload[46], 最后收到的命令字回显
    uint8_t  attitude_complete;     ///< payload[47], 姿态指引完成反馈

    // 时间戳（STM32本地时间）
    uint32_t timestamp_ms;          ///< HAL_GetTick()接收时刻

    // 有效性标志
    bool     valid;                 ///< 整帧数据校验通过且解析成功
} GimbalStatus_t;

/* ============ 厂家协议常量 ============ */

/**
 * @brief 跟踪状态枚举
 * @note  payload[57]字段值
 */
typedef enum {
    TRACKING_IDLE      = 0x00,  ///< 空闲状态
    TRACKING_STABLE    = 0x01,  ///< 稳定跟踪 - 唯一允许测距的状态
    TRACKING_MEMORY    = 0x02,  ///< 记忆跟踪 - 目标可能丢失，不可靠
    TRACKING_LOST      = 0x03,  ///< 目标丢失
    TRACKING_INTERCEPT = 0x05   ///< 截获状态 - 等待稳定
} TrackingState_t;

/**
 * @brief 厂家协议命令字定义
 */
#define VENDOR_CMD_ANGLE_POINTING   0x0072  ///< 数引，方位/俯仰角×100
#define VENDOR_CMD_STATUS_FEEDBACK  0x0014  ///< 状态反馈
#define VENDOR_SERVO_ANGLE_POINTING 0x04

/**
 * @brief 厂家协议帧头定义
 */
#define VENDOR_HEADER_CTRL_0        0xFB    ///< 控制帧头字节0
#define VENDOR_HEADER_CTRL_1        0x2C    ///< 控制帧头字节1
#define VENDOR_HEADER_CTRL_2        0xAA    ///< 控制帧头字节2

#define VENDOR_HEADER_STATUS_0      0xFC    ///< 状态帧头字节0
#define VENDOR_HEADER_STATUS_1      0x2C    ///< 状态帧头字节1
#define VENDOR_HEADER_STATUS_2      0x55    ///< 状态帧头字节2

#endif /* __TYPES_H__ */
