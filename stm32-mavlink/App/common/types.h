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
    uint8_t  servo_state;           ///< payload[6], 伺服状态

    // 姿态角度（调试用）
    int16_t  azimuth_deg100;        ///< payload[7..8], 方位角×100
    int16_t  pitch_deg100;          ///< payload[9..10], 俯仰角×100

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

/* ============ 激光控制状态机 ============ */

/**
 * @brief 激光测距状态
 */
typedef enum {
    LASER_STATE_IDLE = 0,       ///< 空闲/关闭
    LASER_STATE_STARTING,       ///< 正在启动（发送0x003E后）
    LASER_STATE_ACTIVE,         ///< 激光开启运行中
    LASER_STATE_STOPPING        ///< 正在停止（发送0x003F后）
} LaserState_t;

/**
 * @brief 激光控制状态机数据
 */
typedef struct {
    LaserState_t state;             ///< 当前状态
    uint8_t  stable_count;          ///< 稳定跟踪连续帧计数
    uint32_t start_time_ms;         ///< 激光开启时刻（用于超时检测）
} LaserControl_t;

/* ============ ROI命令数据 ============ */

/**
 * @brief ROI地理目标命令数据
 * @note  从MAVLink COMMAND_INT(195)解析得到
 */
typedef struct {
    int32_t  lat_e7;       ///< 纬度×10^7 (from COMMAND_INT.x)
    int32_t  lon_e7;       ///< 经度×10^7 (from COMMAND_INT.y)
    float    alt_m;        ///< 海拔高度(米) (from COMMAND_INT.z)
    uint8_t  frame;        ///< 高度基准 (MAV_FRAME)
    uint32_t timestamp_ms; ///< 接收时刻时间戳
    bool     valid;        ///< 数据有效标志
} RoiCommand_t;

/* ============ 目标定位数据（带时间戳）============ */

/**
 * @brief 目标地理位置数据（用于上报）
 * @note  包含从吊舱接收时刻的时间戳
 */
typedef struct {
    int32_t  lat_e7;           ///< 纬度×10^7
    int32_t  lon_e7;           ///< 经度×10^7
    float    alt_m;            ///< 海拔高度(米)
    uint16_t distance_dm;      ///< 激光测距距离×0.1m (分米)

    // 时间戳（从吊舱状态接收时刻）
    uint32_t timestamp_ms;     ///< STM32本地时间 HAL_GetTick()

    bool     valid;            ///< 数据有效性标志
} TargetPosition_t;

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
#define VENDOR_CMD_GEO_POINTING     0x001B  ///< 地理指引
#define VENDOR_CMD_GEO_TRACKING     0x001C  ///< 地理跟踪
#define VENDOR_CMD_LASER_SINGLE     0x003D  ///< 单次激光测距
#define VENDOR_CMD_LASER_START      0x003E  ///< 开启连续激光测距
#define VENDOR_CMD_LASER_STOP       0x003F  ///< 停止激光测距
#define VENDOR_CMD_STATUS_FEEDBACK  0x0014  ///< 状态反馈

/**
 * @brief 厂家协议帧头定义
 */
#define VENDOR_HEADER_CTRL_0        0xFB    ///< 控制帧头字节0
#define VENDOR_HEADER_CTRL_1        0x2C    ///< 控制帧头字节1
#define VENDOR_HEADER_CTRL_2        0xAA    ///< 控制帧头字节2

#define VENDOR_HEADER_STATUS_0      0xFC    ///< 状态帧头字节0
#define VENDOR_HEADER_STATUS_1      0x2C    ///< 状态帧头字节1
#define VENDOR_HEADER_STATUS_2      0x55    ///< 状态帧头字节2

/* ============ 系统统计信息 ============ */

/**
 * @brief 系统运行统计数据
 * @note  用于调试和监控
 */
typedef struct {
    uint32_t uart1_rx_bytes;            ///< UART1接收总字节数
    uint32_t uart2_rx_bytes;            ///< UART2接收总字节数

    // MAVLink统计
    uint32_t mavlink_roi_received;      ///< 接收ROI命令计数
    uint32_t mavlink_invalid_coords;    ///< 坐标越界拒绝计数
    uint32_t mavlink_frame_rejected;    ///< Frame类型拒绝计数
    uint32_t mavlink_wrong_target;      ///< 系统ID不匹配计数
    uint32_t mavlink_target_sent;       ///< 目标位置上报计数

    // 厂家协议统计
    uint32_t vendor_parse_ok;           ///< 厂家协议解析成功计数
    uint32_t vendor_parse_error;        ///< 厂家协议解析错误计数
    uint32_t geo_frames_sent;           ///< 发送地理指向帧计数
    uint32_t laser_start_count;         ///< 激光启动计数
    uint32_t laser_stop_count;          ///< 激光停止计数
} SystemStats_t;

#endif /* __TYPES_H__ */
