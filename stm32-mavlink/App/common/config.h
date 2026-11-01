/**
 ******************************************************************************
 * @file    config.h
 * @brief   Global configuration macros for STM32 MAVLink-Gimbal bridge
 ******************************************************************************
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* ============ 功能使能开关 ============ */

/**
 * @brief ID86发送使能（编译时开关）
 * @note  已确认不会触发飞行，可以使能
 */
#define ENABLE_ID86_TRANSMISSION    1

/**
 * @brief 地理指向命令选择（两者互斥）
 */
#define USE_GEO_POINTING_0x001B     1   // 地理指引
#define USE_GEO_TRACKING_0x001C     0   // 地理跟踪

/* ============ 海拔基准配置 ============ */

/**
 * @brief 海拔基准类型枚举
 * @note  方便后续根据实测调整
 */
typedef enum {
    ALTITUDE_REF_MSL = 0,        // 平均海平面高度 (Mean Sea Level)
    ALTITUDE_REF_ELLIPSOID = 1,  // WGS84椭球高
    ALTITUDE_REF_RELATIVE = 2,   // 相对home点高度
} AltitudeReference_t;

/**
 * @brief 当前使用的海拔基准
 * @note  修改此宏即可切换海拔基准，无需改动其他代码
 */
#define ALTITUDE_REFERENCE  ALTITUDE_REF_MSL

/**
 * @brief 允许接受的MAVLink frame类型
 * @note  根据ALTITUDE_REFERENCE自动设置
 */
#if (ALTITUDE_REFERENCE == ALTITUDE_REF_MSL)
    #define ACCEPT_MAV_FRAME_GLOBAL         1
    #define ACCEPT_MAV_FRAME_GLOBAL_INT     1
    #define ACCEPT_MAV_FRAME_RELATIVE_ALT   0
    #define ACCEPT_MAV_FRAME_RELATIVE_ALT_INT 0
#elif (ALTITUDE_REFERENCE == ALTITUDE_REF_RELATIVE)
    #define ACCEPT_MAV_FRAME_GLOBAL         0
    #define ACCEPT_MAV_FRAME_GLOBAL_INT     0
    #define ACCEPT_MAV_FRAME_RELATIVE_ALT   1
    #define ACCEPT_MAV_FRAME_RELATIVE_ALT_INT 1
#else
    #error "Unsupported ALTITUDE_REFERENCE setting"
#endif

/* ============ 激光控制参数 ============ */

/**
 * @brief 稳定跟踪去抖帧数
 * @note  50Hz状态反馈，3帧约60ms
 */
#define STABLE_TRACKING_CONFIRM_FRAMES  3

/**
 * @brief 激光连续测距最大时长（毫秒）
 * @note  厂家建议≤15分钟，软件设10分钟安全余量
 */
#define LASER_MAX_DURATION_MS    (10 * 60 * 1000)

/**
 * @brief 吊舱状态超时阈值（毫秒）
 * @note  50Hz = 20ms周期，4倍周期无数据视为超时
 */
#define STATUS_TIMEOUT_MS        200

/**
 * @brief 目标坐标上报最小间隔（毫秒）
 * @note  激光1Hz测距，上报频率≤1Hz
 */
#define TARGET_REPORT_RATE_MS    1000

/* ============ UART缓冲区大小 ============ */

/**
 * @brief UART1接收缓冲区大小（MAVLink）
 * @note  MAVLink2最大帧280字节，预留2倍余量
 */
#define UART1_RX_BUFFER_SIZE    512

/**
 * @brief UART2接收缓冲区大小（吊舱）
 * @note  吊舱状态帧67字节，预留约4倍余量
 */
#define UART2_RX_BUFFER_SIZE    256

/* ============ MAVLink配置 ============ */

/**
 * @brief Pixhawk系统ID
 * @note  典型值为1，实际应根据飞控配置确认
 */
#define MAVLINK_SYSTEM_ID       1

/**
 * @brief 目标组件ID
 * @note  0表示所有组件，实际应根据飞控配置确认
 */
#define MAVLINK_COMPONENT_ID    0

/**
 * @brief 本设备系统ID
 * @note  作为MAVLink消息发送者的系统ID
 */
#define MAVLINK_OUR_SYSTEM_ID   100

/**
 * @brief 本设备组件ID
 * @note  作为MAVLink消息发送者的组件ID（云台组件）
 */
#define MAVLINK_OUR_COMPONENT_ID  154  // MAV_COMP_ID_GIMBAL

/* ============ 坐标有效范围 ============ */

/**
 * @brief 纬度有效范围（×10^7）
 */
#define LAT_MIN_E7     (-900000000)   // -90°
#define LAT_MAX_E7      (900000000)   // +90°

/**
 * @brief 经度有效范围（×10^7）
 */
#define LON_MIN_E7    (-1800000000)   // -180°
#define LON_MAX_E7     (1800000000)   // +180°

/**
 * @brief 海拔有效范围（米）
 * @note  根据吊舱实际工作范围设定
 */
#define ALT_MIN_M     (-1000)         // -1000m
#define ALT_MAX_M      (10000)        // +10000m

/* ============ 调试选项 ============ */

/**
 * @brief LED调试指示使能
 */
#define DEBUG_LED_ENABLE        1

/**
 * @brief UART3调试日志输出使能（可选功能）
 */
#define DEBUG_UART3_ENABLE      0

/**
 * @brief 统计信息使能
 */
#define DEBUG_STATS_ENABLE      1

#endif /* __CONFIG_H__ */
