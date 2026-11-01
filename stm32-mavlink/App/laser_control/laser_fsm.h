/**
 ******************************************************************************
 * @file    laser_fsm.h
 * @brief   Laser ranging state machine
 ******************************************************************************
 */

#ifndef __LASER_FSM_H__
#define __LASER_FSM_H__

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

/**
 * @brief Laser action command
 */
typedef enum {
    LASER_ACTION_NONE = 0,      ///< No action required
    LASER_ACTION_START,         ///< Send 0x003E start command
    LASER_ACTION_STOP           ///< Send 0x003F stop command
} LaserAction_t;

/**
 * @brief Initialize laser control state machine
 * @param ctrl State machine data structure
 */
void Laser_Init(LaserControl_t *ctrl);

/**
 * @brief Update state machine (call on every gimbal status received)
 * @param ctrl State machine data structure
 * @param status Current gimbal status
 * @return Laser action to execute (START/STOP/NONE)
 */
LaserAction_t Laser_Update(LaserControl_t *ctrl, const GimbalStatus_t *status);

/**
 * @brief Confirm laser start command sent
 * @param ctrl State machine data structure
 * @note Call immediately after successfully sending 0x003E
 */
void Laser_ConfirmStart(LaserControl_t *ctrl);

/**
 * @brief Confirm laser stop command sent
 * @param ctrl State machine data structure
 * @note Call immediately after successfully sending 0x003F
 */
void Laser_ConfirmStop(LaserControl_t *ctrl);

/**
 * @brief Check if laser is currently active
 * @param ctrl State machine data structure
 * @return true if laser is active, false otherwise
 */
bool Laser_IsActive(const LaserControl_t *ctrl);

#endif /* __LASER_FSM_H__ */
