#ifndef CONFIG_H
#define CONFIG_H

/* Native SToRM32 MAVLink adapter. All three ports use 115200 8N1. */
#define MAVLINK_FLIGHT_SYSTEM_ID       1U
#define MAVLINK_FLIGHT_COMPONENT_ID    1U
#define MAVLINK_GIMBAL_SYSTEM_ID       1U
#define MAVLINK_GIMBAL_COMPONENT_ID    154U
#define MAVLINK_MOUNT_COMMAND          205U
#define MAVLINK_MOUNT_ANGLE_MODE       2.0f
#define MAVLINK_HEARTBEAT_PERIOD_MS    1000U
#define CONTROL_TEXT_PERIOD_MS         200U

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
