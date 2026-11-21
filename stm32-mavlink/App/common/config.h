#ifndef CONFIG_H
#define CONFIG_H

/* Native SToRM32 MAVLink adapter. All three ports use 115200 8N1. */
#define MAVLINK_FLIGHT_SYSTEM_ID       1U
#define MAVLINK_FLIGHT_COMPONENT_ID    1U
#define MAVLINK_GIMBAL_SYSTEM_ID       1U
#define MAVLINK_GIMBAL_COMPONENT_ID    154U
#define MAVLINK_MOUNT_COMMAND          205U
#define MAVLINK_MOUNT_ANGLE_MODE       2.0f
#define MAVLINK_GCS_SYSTEM_ID          255U
#define MAVLINK_GCS_COMPONENT_ID       190U
#define MAVLINK_CONTROL_OWNER_COMMAND  31010U  /* MAV_CMD_USER_1 */
#define MAVLINK_NEW_POINT_COMMAND      31011U  /* MAV_CMD_USER_2 */
#define MAVLINK_HEARTBEAT_PERIOD_MS    1000U
#define CONTROL_TEXT_PERIOD_MS         200U
#define POINTING_TOLERANCE_DEG100      100
#define POINTING_SETTLE_MS             500U
#define POINTING_FEEDBACK_GAP_MS       100U
#define POINTING_FIRST_ANGLE_MS        2000U
#define POINTING_TIMEOUT_MS            10000U
#define RANGE_REPORT_PERIOD_MS         1000U

/* Negate wire param3/param1 for the current bench direction calibration.
 * Reference-frame alignment still requires hardware verification. */
#ifndef GIMBAL_YAW_SIGN
#define GIMBAL_YAW_SIGN                -1
#endif
#ifndef GIMBAL_PITCH_SIGN
#define GIMBAL_PITCH_SIGN              -1
#endif
#if (GIMBAL_YAW_SIGN != 1 && GIMBAL_YAW_SIGN != -1) || \
    (GIMBAL_PITCH_SIGN != 1 && GIMBAL_PITCH_SIGN != -1)
#error "Angle signs must be +1 or -1"
#endif

#define UART1_RX_BUFFER_SIZE           512U
#define UART2_RX_BUFFER_SIZE           256U

#endif
