#include "mavlink_handler.h"
#include "config.h"
#include "text_format.h"
#include "vendor_protocol.h"
#include <math.h>
#include <string.h>

static bool SendMessage(MAVLinkBridge_t *bridge, const mavlink_message_t *message) {
    /* Only unsigned MAVLink2 HEARTBEAT (21 bytes) and ACK (22 bytes) are sent. */
    uint8_t wire[32];
    uint16_t length = mavlink_msg_to_send_buffer(wire, message);
    return bridge->write_flight(bridge->context, wire, length);
}

static bool SendAck(MAVLinkBridge_t *bridge, MountResult_t result) {
    mavlink_command_ack_t ack = {0};
    mavlink_message_t message;
    ack.command = MAVLINK_MOUNT_COMMAND;
    ack.result = (uint8_t)result;
    ack.target_system = (uint8_t)bridge->rx_message.sysid;
    ack.target_component = bridge->rx_message.compid;
    mavlink_msg_command_ack_encode_status(MAVLINK_GIMBAL_SYSTEM_ID,
        MAVLINK_GIMBAL_COMPONENT_ID, &bridge->tx_status, &message, &ack);
    return SendMessage(bridge, &message);
}

static bool ConvertAngles(MountControl_t *control) {
    float yaw = control->yaw_deg * GIMBAL_YAW_SIGN;
    float pitch = control->pitch_deg * GIMBAL_PITCH_SIGN;
    /* Encoding limits from the vendor's 0x0072 table. Reject, do not wrap or
     * silently clamp a target into a different pointing direction. */
    if (!isfinite(yaw) || !isfinite(pitch) || !isfinite(control->roll_deg) ||
        yaw < -179.0f || yaw > 179.0f || pitch < -120.0f || pitch > 90.0f) {
        return false;
    }
    control->vendor_yaw_deg100 = (int16_t)lroundf(yaw * 100.0f);
    control->vendor_pitch_deg100 = (int16_t)lroundf(pitch * 100.0f);
    return true;
}

static void HandleMountControl(MAVLinkBridge_t *bridge) {
    const mavlink_message_t *message = &bridge->rx_message;
    if (message->msgid != MAVLINK_MSG_ID_COMMAND_LONG ||
        message->sysid != MAVLINK_FLIGHT_SYSTEM_ID ||
        message->compid != MAVLINK_FLIGHT_COMPONENT_ID ||
        message->len > MAVLINK_MSG_ID_COMMAND_LONG_LEN ||
        (message->magic == MAVLINK_STX_MAVLINK1 &&
         message->len != MAVLINK_MSG_ID_COMMAND_LONG_LEN) ||
        message->incompat_flags != 0) {
        return;
    }

    /* The upstream decoder zero-fills omitted MAVLink2 payload bytes. */
    mavlink_command_long_t command;
    mavlink_msg_command_long_decode(message, &command);
    if (command.command != MAVLINK_MOUNT_COMMAND ||
        (command.target_system != 0 &&
         command.target_system != MAVLINK_GIMBAL_SYSTEM_ID) ||
        (command.target_component != 0 &&
         command.target_component != MAVLINK_GIMBAL_COMPONENT_ID)) {
        return;
    }

    MountControl_t *control = &bridge->control;
    memset(control, 0, sizeof(*control));
    control->pitch_deg = command.param1;
    control->roll_deg = command.param2;
    control->yaw_deg = command.param3;
    if (command.param7 != MAVLINK_MOUNT_ANGLE_MODE) {
        control->result = MOUNT_UNSUPPORTED;
    } else if (!ConvertAngles(control)) {
        control->result = MOUNT_DENIED;
    } else {
        control->converted = true;
        uint8_t wire[11];
        uint8_t length = Vendor_BuildAnglePointingFrame(wire,
            control->vendor_yaw_deg100, control->vendor_pitch_deg100);
        control->result = bridge->write_gimbal(bridge->context, wire, length)
            ? MOUNT_ACCEPTED : MOUNT_FAILED;
    }
    /* ACCEPTED means the UART write succeeded, not that the gimbal arrived. */
    control->ack_sent = SendAck(bridge, control->result);
    bridge->control_pending = true;
}

void MAVLink_Init(MAVLinkBridge_t *bridge, MAVLinkWrite_t write_flight,
                  MAVLinkWrite_t write_gimbal, void *context) {
    memset(bridge, 0, sizeof(*bridge));
    bridge->write_flight = write_flight;
    bridge->write_gimbal = write_gimbal;
    bridge->context = context;
}

void MAVLink_FeedByte(MAVLinkBridge_t *bridge, uint8_t byte) {
    uint8_t result = mavlink_frame_char_buffer(&bridge->rx_message,
        &bridge->rx_status, byte, NULL, NULL);
    if (result == MAVLINK_FRAMING_OK) {
        HandleMountControl(bridge);
    } else if (result == MAVLINK_FRAMING_BAD_CRC ||
               result == MAVLINK_FRAMING_BAD_SIGNATURE) {
        /* Match the upstream parse_char recovery without global channel state. */
        bridge->rx_status.parse_state = MAVLINK_PARSE_STATE_IDLE;
        bridge->rx_status.msg_received = MAVLINK_FRAMING_INCOMPLETE;
        if (byte == MAVLINK_STX || byte == MAVLINK_STX_MAVLINK1) {
            mavlink_frame_char_buffer(&bridge->rx_message, &bridge->rx_status,
                                      byte, NULL, NULL);
        }
    }
}

bool MAVLink_SendHeartbeatIfDue(MAVLinkBridge_t *bridge, uint32_t now_ms) {
    if (bridge->heartbeat_started &&
        (uint32_t)(now_ms - bridge->last_heartbeat_ms) < MAVLINK_HEARTBEAT_PERIOD_MS) {
        return false;
    }
    mavlink_heartbeat_t heartbeat = {0};
    mavlink_message_t message;
    heartbeat.type = MAV_TYPE_GIMBAL;
    heartbeat.autopilot = MAV_AUTOPILOT_INVALID;
    heartbeat.system_status = MAV_STATE_ACTIVE;
    mavlink_msg_heartbeat_encode_status(MAVLINK_GIMBAL_SYSTEM_ID,
        MAVLINK_GIMBAL_COMPONENT_ID, &bridge->tx_status, &message, &heartbeat);
    if (!SendMessage(bridge, &message)) {
        return false;
    }
    bridge->last_heartbeat_ms = now_ms;
    bridge->heartbeat_started = true;
    return true;
}

static void FormatRawAngle(char *out, size_t capacity, float angle) {
    /* Invalid command values must never reach a float-to-integer conversion. */
    if (!isfinite(angle) || fabsf(angle) > 1000.0f) {
        snprintf(out, capacity, "invalid");
        return;
    }
    Text_FormatFixed(out, capacity, (int32_t)lroundf(angle * 1000.0f), 1000U, 3);
}

bool MAVLink_Report(MAVLinkBridge_t *bridge, MAVLinkWrite_t write_text,
                    uint32_t now_ms) {
    if (!bridge->control_pending ||
        (bridge->report_started &&
         (uint32_t)(now_ms - bridge->last_report_ms) < CONTROL_TEXT_PERIOD_MS)) {
        return false;
    }
    const MountControl_t *control = &bridge->control;
    char pitch[20], roll[20], yaw[20], vendor_yaw[16] = "NA", vendor_pitch[16] = "NA";
    FormatRawAngle(pitch, sizeof(pitch), control->pitch_deg);
    FormatRawAngle(roll, sizeof(roll), control->roll_deg);
    FormatRawAngle(yaw, sizeof(yaw), control->yaw_deg);
    if (control->converted) {
        Text_FormatFixed(vendor_yaw, sizeof(vendor_yaw), control->vendor_yaw_deg100, 100U, 2);
        Text_FormatFixed(vendor_pitch, sizeof(vendor_pitch), control->vendor_pitch_deg100, 100U, 2);
    }
    const char *result = control->result == MOUNT_ACCEPTED ? "sent" :
                         control->result == MOUNT_DENIED ? "rejected" :
                         control->result == MOUNT_UNSUPPORTED ? "mode_unsupported" : "uart2_error";
    char text[320];
    int length = snprintf(text, sizeof(text),
        "[CTRL] p1_pitch_deg=%s p2_roll_deg=%s p3_yaw_deg=%s "
        "vendor_yaw_deg=%s vendor_pitch_deg=%s result=%s ack=%s\r\n",
        pitch, roll, yaw, vendor_yaw, vendor_pitch, result,
        control->ack_sent ? "sent" : "uart1_error");
    if (length <= 0 || (size_t)length >= sizeof(text) ||
        !write_text(bridge->context, (const uint8_t *)text, (uint16_t)length)) {
        return false;
    }
    bridge->last_report_ms = now_ms;
    bridge->report_started = true;
    bridge->control_pending = false;
    return true;
}
