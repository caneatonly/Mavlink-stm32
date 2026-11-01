/**
 ******************************************************************************
 * @file    target_validator.h
 * @brief   Target position validation and report rate control
 ******************************************************************************
 */

#ifndef __TARGET_VALIDATOR_H__
#define __TARGET_VALIDATOR_H__

#include <stdbool.h>
#include "../common/types.h"

/**
 * @brief 目标位置综合验证
 * @param status    吊舱状态数据
 * @param target    输出：验证通过后填充的目标位置
 * @return true=验证通过可上报, false=不满足条件
 *
 * @note 验证条件：
 *       1. 吊舱就绪
 *       2. 稳定跟踪状态(0x01)
 *       3. 坐标有变化（新鲜度检查）
 *       4. 距离上次上报≥1秒（频率限制）
 */
bool Target_Validate(const GimbalStatus_t *status, TargetPosition_t *target);

#endif /* __TARGET_VALIDATOR_H__ */
