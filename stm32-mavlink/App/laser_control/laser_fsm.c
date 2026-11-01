/**
 ******************************************************************************
 * @file    laser_fsm.c
 * @brief   Laser ranging state machine implementation
 ******************************************************************************
 */

#include "laser_fsm.h"
#include "config.h"
#include "main.h"

void Laser_Init(LaserControl_t *ctrl)
{
    ctrl->state = LASER_STATE_IDLE;
    ctrl->stable_count = 0;
    ctrl->start_time_ms = 0;
}

LaserAction_t Laser_Update(LaserControl_t *ctrl, const GimbalStatus_t *status)
{
    uint32_t now = HAL_GetTick();

    switch (ctrl->state) {

        case LASER_STATE_IDLE:
            // Check conditions for starting laser
            if (status->gimbal_ready &&
                status->tracking_state == TRACKING_STABLE) {

                ctrl->stable_count++;

                // Debounce: require N consecutive stable frames
                if (ctrl->stable_count >= STABLE_CONFIRM_FRAMES) {
                    ctrl->state = LASER_STATE_STARTING;
                    return LASER_ACTION_START;
                }
            } else {
                // Reset debounce counter if tracking lost
                ctrl->stable_count = 0;
            }
            break;

        case LASER_STATE_STARTING:
            // Waiting for main.c to call Laser_ConfirmStart()
            break;

        case LASER_STATE_ACTIVE:
            // Monitor conditions for stopping laser

            // Condition 1: Tracking lost or not stable
            if (status->tracking_state != TRACKING_STABLE) {
                ctrl->state = LASER_STATE_STOPPING;
                return LASER_ACTION_STOP;
            }

            // Condition 2: Gimbal not ready
            if (!status->gimbal_ready) {
                ctrl->state = LASER_STATE_STOPPING;
                return LASER_ACTION_STOP;
            }

            // Condition 3: Timeout (10 minutes)
            if ((now - ctrl->start_time_ms) > LASER_TIMEOUT_MS) {
                ctrl->state = LASER_STATE_STOPPING;
                return LASER_ACTION_STOP;
            }
            break;

        case LASER_STATE_STOPPING:
            // Waiting for main.c to call Laser_ConfirmStop()
            break;

        default:
            ctrl->state = LASER_STATE_IDLE;
            break;
    }

    return LASER_ACTION_NONE;
}

void Laser_ConfirmStart(LaserControl_t *ctrl)
{
    if (ctrl->state == LASER_STATE_STARTING) {
        ctrl->state = LASER_STATE_ACTIVE;
        ctrl->start_time_ms = HAL_GetTick();
    }
}

void Laser_ConfirmStop(LaserControl_t *ctrl)
{
    if (ctrl->state == LASER_STATE_STOPPING) {
        ctrl->state = LASER_STATE_IDLE;
        ctrl->stable_count = 0;
        ctrl->start_time_ms = 0;
    }
}

bool Laser_IsActive(const LaserControl_t *ctrl)
{
    return (ctrl->state == LASER_STATE_ACTIVE);
}
