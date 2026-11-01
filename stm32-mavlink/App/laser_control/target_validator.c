/**
 ******************************************************************************
 * @file    target_validator.c
 * @brief   Target position validation implementation
 ******************************************************************************
 */

#include "target_validator.h"
#include "../common/config.h"
#include "main.h"  // For HAL_GetTick()

/* ============ 私有变量 ============ */

// 新鲜度检查：记录上次的坐标
static int32_t last_lon_e7 = 0;
static int32_t last_lat_e7 = 0;

// 频率限制：记录上次上报时刻
static uint32_t last_report_time_ms = 0;

/* ============ 公共函数实现 ============ */

/**
 * @brief 目标位置综合验证
 */
bool Target_Validate(const GimbalStatus_t *status, TargetPosition_t *target) {
    // 检查1：吊舱就绪
    if (!status->gimbal_ready) {
        return false;
    }

    // 检查2：必须是稳定跟踪状态
    if (status->tracking_state != TRACKING_STABLE) {
        return false;
    }

    // 检查3：坐标新鲜度（是否变化）
    if (status->target_lon_e7 == last_lon_e7 &&
        status->target_lat_e7 == last_lat_e7) {
        return false;  // 坐标未变化，不上报
    }

    // 检查4：上报频率限制（≤1Hz）
    uint32_t now = HAL_GetTick();
    if ((now - last_report_time_ms) < TARGET_REPORT_INTERVAL_MS) {
        return false;  // 距离上次上报不足1秒
    }

    // 全部通过，填充输出
    target->lat_e7 = status->target_lat_e7;
    target->lon_e7 = status->target_lon_e7;
    target->alt_m = (float)status->target_alt_m;
    target->distance_dm = status->laser_distance_dm;
    target->timestamp_ms = status->timestamp_ms;
    target->valid = true;

    // 更新记录
    last_lon_e7 = status->target_lon_e7;
    last_lat_e7 = status->target_lat_e7;
    last_report_time_ms = now;

    return true;
}
