/**
 ******************************************************************************
 * @file    mavlink_handler.c
 * @brief   MAVLink protocol interface implementation
 ******************************************************************************
 */

#include "mavlink_handler.h"
#include "mavlink_c_lib/common/mavlink.h"
#include "../common/config.h"
#include "main.h"  
#include <string.h>

/* ============ 私有变量 ============ */

// MAVLink接收解析器状态
static mavlink_status_t mav_rx_status;
static uint8_t mav_channel = MAVLINK_COMM_0;

/* ============ 公共函数实现 ============ */

/**
 * @brief 初始化MAVLink系统
 */
void MAVLink_Init(void) {
    // 清零解析器状态
    memset(&mav_rx_status, 0, sizeof(mav_rx_status));

    // 清零MAVLink相关统计
    #if DEBUG_STATS_ENABLE
    g_stats.mavlink_roi_received = 0;
    g_stats.mavlink_invalid_coords = 0;
    g_stats.mavlink_frame_rejected = 0;
    g_stats.mavlink_target_sent = 0;
    #endif
}

/**
 * @brief 逐字节解析MAVLink流
 */
MAVLinkParseResult_t MAVLink_ParseByte(uint8_t byte, RoiCommand_t *roi_out) {
    mavlink_message_t msg;

    // 喂入字节给官方解析器
    if (mavlink_parse_char(mav_channel, byte, &msg, &mav_rx_status)) {
        // 成功解析一帧完整消息

        // 第1层过滤：只处理COMMAND_INT消息
        if (msg.msgid == MAVLINK_MSG_ID_COMMAND_INT) {
            mavlink_command_int_t cmd;
            mavlink_msg_command_int_decode(&msg, &cmd);

            // 第2层过滤：只处理ROI命令
            if (cmd.command == MAV_CMD_DO_SET_ROI_LOCATION) {

                // 第3层过滤：系统ID（可选）
                #if MAVLINK_ENABLE_TARGET_FILTER
                    // 仅接受发给特定系统的命令或广播
                    if (cmd.target_system != MAVLINK_PIXHAWK_SYSTEM_ID &&
                        cmd.target_system != 0) {
                        #if DEBUG_STATS_ENABLE
                        g_stats.mavlink_wrong_target++;
                        #endif
                        return MAVLINK_PARSE_RESULT_OTHER;
                    }
                #endif

                // 第4层校验：Frame类型（可选）
                #if MAVLINK_FRAME_STRICT_CHECK
                    bool frame_ok = false;

                    // 根据config.h配置判断接受哪些frame
                    #if ACCEPT_MAV_FRAME_GLOBAL
                        if (cmd.frame == MAV_FRAME_GLOBAL ||
                            cmd.frame == MAV_FRAME_GLOBAL_INT) {
                            frame_ok = true;
                        }
                    #endif

                    #if ACCEPT_MAV_FRAME_RELATIVE_ALT
                        if (cmd.frame == MAV_FRAME_GLOBAL_RELATIVE_ALT) {
                            frame_ok = true;
                        }
                    #endif

                    if (!frame_ok) {
                        #if DEBUG_STATS_ENABLE
                        g_stats.mavlink_frame_rejected++;
                        #endif
                        return MAVLINK_PARSE_RESULT_REJECTED;
                    }
                #endif

                // 第5层校验：坐标范围（必须）
                if (cmd.x < LAT_MIN_E7 || cmd.x > LAT_MAX_E7 ||
                    cmd.y < LON_MIN_E7 || cmd.y > LON_MAX_E7) {
                    #if DEBUG_STATS_ENABLE
                    g_stats.mavlink_invalid_coords++;
                    #endif
                    return MAVLINK_PARSE_RESULT_REJECTED;
                }

                // 全部通过，提取数据
                // 注意：MAVLink的x=纬度, y=经度
                roi_out->lat_e7 = cmd.x;
                roi_out->lon_e7 = cmd.y;
                roi_out->alt_m = cmd.z;
                roi_out->frame = cmd.frame;
                roi_out->timestamp_ms = HAL_GetTick();
                roi_out->valid = true;

                // 统计
                #if DEBUG_STATS_ENABLE
                g_stats.mavlink_roi_received++;
                #endif

                return MAVLINK_PARSE_RESULT_ROI_CMD;
            }

            // 其他COMMAND_INT命令
            return MAVLINK_PARSE_RESULT_OTHER;
        }

        // 其他MAVLink消息
        return MAVLINK_PARSE_RESULT_OTHER;
    }

    // 还没收集够完整帧
    return MAVLINK_PARSE_RESULT_NONE;
}

/**
 * @brief 构建并发送目标位置消息
 */
bool MAVLink_SendTargetPosition(const TargetPosition_t *target,
                                 uint8_t *out_buf,
                                 uint16_t *out_len) {
    // 安全检查：编译时开关
    #if ENABLE_ID86_TRANSMISSION == 0
        return false;
    #endif

    // 安全检查：输入有效性
    if (!target || !target->valid || !out_buf || !out_len) {
        return false;
    }

    // 构建消息结构
    mavlink_message_t msg;
    mavlink_set_position_target_global_int_t pos;
    memset(&pos, 0, sizeof(pos));

    // 填充位置数据
    pos.time_boot_ms = target->timestamp_ms;
    pos.lat_int = target->lat_e7;        // int32, ×10^7
    pos.lon_int = target->lon_e7;        // int32, ×10^7
    pos.alt = target->alt_m;             // float, 米

    // 速度/加速度/姿态全部置0（不使用）
    pos.vx = 0.0f;
    pos.vy = 0.0f;
    pos.vz = 0.0f;
    pos.afx = 0.0f;
    pos.afy = 0.0f;
    pos.afz = 0.0f;
    pos.yaw = 0.0f;
    pos.yaw_rate = 0.0f;

    // type_mask: 仅位置字段有效
    // 0x0DF8 = 0b0000 1101 1111 1000
    // bit0-2=0: 位置xyz有效
    // bit3-5=1: 速度xyz无效
    // bit6-8=1: 加速度xyz无效
    // bit9-10=1: yaw/yaw_rate无效
    pos.type_mask = 0x0DF8;

    // WGS84坐标系
    pos.coordinate_frame = MAV_FRAME_GLOBAL;

    // 目标：发给Pixhawk飞控
    pos.target_system = MAVLINK_PIXHAWK_SYSTEM_ID;
    pos.target_component = MAVLINK_PIXHAWK_COMPONENT_ID;

    // 编码消息（使用Pixhawk身份代发）
    mavlink_msg_set_position_target_global_int_encode(
        MAVLINK_PIXHAWK_SYSTEM_ID,      // 发送者system_id = Pixhawk
        MAVLINK_PIXHAWK_COMPONENT_ID,   // 发送者component_id = 飞控
        &msg,
        &pos
    );

    // 序列化到字节流
    *out_len = mavlink_msg_to_send_buffer(out_buf, &msg);

    // 统计
    #if DEBUG_STATS_ENABLE
    g_stats.mavlink_target_sent++;
    #endif

    return true;
}
