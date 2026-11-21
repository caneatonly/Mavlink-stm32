#include "mavlink_handler.h"
#include "config.h"
#include "text_format.h"
#include "vendor_protocol.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define POINTING_TASK_ID_MASK 0xFFFFFFU
#define POSITION_ONLY_TYPE_MASK 0x0DF8U

static bool SendMessage(MAVLinkBridge_t *bridge, const mavlink_message_t *message,
                        MAVLinkTxTrace_t *trace, bool transmit) {
    trace->length = mavlink_msg_to_send_buffer(trace->wire, message);
    bool ok = !transmit ||
        bridge->write_flight(bridge->context, trace->wire, trace->length);
    trace->result = !transmit ? "probe" : (ok ? "sent" : "uart1_error");
    return ok;
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
    return SendMessage(bridge, &message, &bridge->ack_trace, true);
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

static void PausePointing(MAVLinkBridge_t *bridge, const char *reason) {
    bridge->task.waiting_angle = false;
    bridge->task.aiming = false;
    bridge->ui_control = true;
    bridge->owner_report_pending = true;
    bridge->point_event = reason;
}

static void AcceptPointingTask(MAVLinkBridge_t *bridge, float value) {
    if (!isfinite(value) || value < 1.0f || value > (float)POINTING_TASK_ID_MASK ||
        value != floorf(value)) {
        return;
    }
    uint32_t id = (uint32_t)value;
    uint32_t advance = (id - bridge->task.id) & POINTING_TASK_ID_MASK;
    if (bridge->task.id != 0 && (advance == 0 || advance > POINTING_TASK_ID_MASK / 2U)) {
        return;
    }
    bridge->task = (PointingTask_t){.id = id, .start_ms = bridge->now_ms, .waiting_angle = true};
    bridge->ui_control = true;
    bridge->first_yaw_deg100 = 0;
    bridge->first_pitch_deg100 = 0;
    bridge->point_event = "new_task";
}

static bool AcceptAngleForTask(MAVLinkBridge_t *bridge, const MountControl_t *control) {
    if (bridge->task.waiting_angle) {
        bridge->first_yaw_deg100 = control->vendor_yaw_deg100;
        bridge->first_pitch_deg100 = control->vendor_pitch_deg100;
        if (!bridge->auto_resume) {
            PausePointing(bridge, "probe_first_205");
            return false;
        }
        bridge->task.waiting_angle = false;
        bridge->task.aiming = true;
        bridge->ui_control = false;
        bridge->owner_report_pending = true;
        bridge->point_event = "forwarding";
    }
    return !bridge->ui_control;
}

static void HandleCommand(MAVLinkBridge_t *bridge) {
    const mavlink_message_t *message = &bridge->rx_message;
    if (message->msgid != MAVLINK_MSG_ID_COMMAND_LONG ||
        message->len > MAVLINK_MSG_ID_COMMAND_LONG_LEN ||
        (message->magic == MAVLINK_STX_MAVLINK1 &&
         message->len != MAVLINK_MSG_ID_COMMAND_LONG_LEN) ||
        message->incompat_flags != 0) {
        return;
    }

    /* The upstream decoder zero-fills omitted MAVLink2 payload bytes. */
    mavlink_command_long_t command;
    mavlink_msg_command_long_decode(message, &command);
    bool from_gcs_to_gimbal = message->sysid == MAVLINK_GCS_SYSTEM_ID &&
        message->compid == MAVLINK_GCS_COMPONENT_ID &&
        command.target_system == MAVLINK_GIMBAL_SYSTEM_ID &&
        command.target_component == MAVLINK_GIMBAL_COMPONENT_ID;
    if (from_gcs_to_gimbal && command.command == MAVLINK_NEW_POINT_COMMAND) {
        AcceptPointingTask(bridge, command.param1);
        return;
    }
    if (from_gcs_to_gimbal && command.command == MAVLINK_CONTROL_OWNER_COMMAND) {
        if (command.param1 == 0.0f || command.param1 == 1.0f) {
            bridge->task.waiting_angle = false;
            bridge->task.aiming = false;
            bridge->ui_control = command.param1 == 1.0f;
            bridge->owner_report_pending = true;
        }
        return;
    }

    if (command.command != MAVLINK_MOUNT_COMMAND ||
        message->sysid != MAVLINK_FLIGHT_SYSTEM_ID ||
        message->compid != MAVLINK_FLIGHT_COMPONENT_ID ||
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
    } else if (bridge->ui_control && !bridge->task.waiting_angle) {
        control->result = MOUNT_UI_CONTROL;
    } else if (!ConvertAngles(control)) {
        control->result = MOUNT_DENIED;
    } else if (!AcceptAngleForTask(bridge, control)) {
        control->converted = true;
        control->result = MOUNT_UI_CONTROL;
    } else {
        control->converted = true;
        uint8_t wire[11];
        uint8_t length = Vendor_BuildAnglePointingFrame(wire,
            control->vendor_yaw_deg100, control->vendor_pitch_deg100);
        control->result = bridge->write_gimbal(bridge->context, wire, length)
            ? MOUNT_ACCEPTED : MOUNT_FAILED;
        if (bridge->task.aiming && control->result == MOUNT_ACCEPTED) {
            bridge->task.target_sent = true;
            bridge->task.yaw_deg100 = control->vendor_yaw_deg100;
            bridge->task.pitch_deg100 = control->vendor_pitch_deg100;
        }
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
    bridge->owner_report_pending = true;
}

void MAVLink_ConfigureAutomation(MAVLinkBridge_t *bridge, bool auto_resume,
                                 bool range_report) {
    bridge->ui_control = true;
    bridge->auto_resume = auto_resume;
    bridge->range_report = range_report;
    bridge->owner_report_pending = true;
}

void MAVLink_UpdateTime(MAVLinkBridge_t *bridge, uint32_t now_ms) {
    bridge->now_ms = now_ms;
    uint32_t elapsed = now_ms - bridge->task.start_ms;
    if (bridge->task.waiting_angle && elapsed >= POINTING_FIRST_ANGLE_MS) {
        PausePointing(bridge, "no_angle");
    } else if (bridge->task.aiming && elapsed >= POINTING_TIMEOUT_MS) {
        PausePointing(bridge, "not_settled");
    }
}

static void ObservePointing(MAVLinkBridge_t *bridge, const GimbalStatus_t *status) {
    PointingTask_t *task = &bridge->task;
    if (!task->aiming || !task->target_sent) {
        return;
    }
    if (!status->valid || !status->gimbal_ready ||
        status->servo_state != VENDOR_SERVO_ANGLE_POINTING ||
        abs((int)status->azimuth_deg100 - task->yaw_deg100) >= POINTING_TOLERANCE_DEG100 ||
        abs((int)status->pitch_deg100 - task->pitch_deg100) >= POINTING_TOLERANCE_DEG100) {
        task->settling = false;
        return;
    }

    uint32_t now_ms = bridge->now_ms;
    if (!task->settling || now_ms - task->last_feedback_ms > POINTING_FEEDBACK_GAP_MS ||
        abs((int)task->yaw_deg100 - task->settle_yaw_deg100) >= POINTING_TOLERANCE_DEG100 ||
        abs((int)task->pitch_deg100 - task->settle_pitch_deg100) >= POINTING_TOLERANCE_DEG100) {
        task->settling = true;
        task->settle_ms = now_ms;
        task->settle_yaw_deg100 = task->yaw_deg100;
        task->settle_pitch_deg100 = task->pitch_deg100;
    }
    task->last_feedback_ms = now_ms;
    if (now_ms - task->settle_ms >= POINTING_SETTLE_MS) {
        PausePointing(bridge, "arrived");
    }
}

static bool SameRangeSample(const GimbalStatus_t *a, const GimbalStatus_t *b) {
    return a->laser_distance_dm == b->laser_distance_dm &&
           a->target_lat_e7 == b->target_lat_e7 && a->target_lon_e7 == b->target_lon_e7 &&
           a->target_alt_m == b->target_alt_m;
}

static bool ValidRangeSample(const GimbalStatus_t *status) {
    return status->valid && status->gimbal_ready && status->tracking_state == TRACKING_STABLE &&
           status->laser_distance_dm != 0 &&
           (status->target_lat_e7 != 0 || status->target_lon_e7 != 0) &&
           status->target_lat_e7 >= -900000000 && status->target_lat_e7 <= 900000000 &&
           status->target_lon_e7 >= -1800000000 && status->target_lon_e7 <= 1800000000 &&
           status->target_alt_m >= -500 && status->target_alt_m <= 10000;
}

static bool SendGlobalPositionTarget(MAVLinkBridge_t *bridge, const GimbalStatus_t *status) {
    mavlink_message_t message = {0};
    message.msgid = MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT;
    char *payload = _MAV_PAYLOAD_NON_CONST(&message);
    _mav_put_uint32_t(payload, 0, bridge->now_ms);
    _mav_put_int32_t(payload, 4, status->target_lat_e7);
    _mav_put_int32_t(payload, 8, status->target_lon_e7);
    _mav_put_float(payload, 12, (float)status->target_alt_m);
    _mav_put_uint16_t(payload, 48, POSITION_ONLY_TYPE_MASK);
    _mav_put_uint8_t(payload, 50, MAVLINK_FLIGHT_SYSTEM_ID);
    _mav_put_uint8_t(payload, 51, MAVLINK_FLIGHT_COMPONENT_ID);
    _mav_put_uint8_t(payload, 52, 0U);  /* MAV_FRAME_GLOBAL (altitude AMSL) */
    /* Preview serialization must not advance the live USART1 sequence. */
    mavlink_status_t preview_status = bridge->tx_status;
    mavlink_status_t *tx_status = bridge->range_report ? &bridge->tx_status : &preview_status;
    mavlink_finalize_message_buffer(&message, MAVLINK_GIMBAL_SYSTEM_ID,
        MAVLINK_GIMBAL_COMPONENT_ID, tx_status, MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_LEN,
        MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_LEN,
        MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT_CRC);
    return SendMessage(bridge, &message, &bridge->range_trace, bridge->range_report);
}

static void ObserveRanging(MAVLinkBridge_t *bridge, const GimbalStatus_t *status) {
    RangeObservation_t *range = &bridge->ranging;
    if (!ValidRangeSample(status)) {
        return;
    }
    /* The status reports the same range fields in single and continuous mode. */
    if (range->last_attempt.valid &&
        (bridge->now_ms - range->last_attempt_ms < RANGE_REPORT_PERIOD_MS ||
         (range->last_attempt_ok && SameRangeSample(&range->last_attempt, status)))) {
        return;
    }

    range->last_attempt = *status;
    range->last_attempt_ms = bridge->now_ms;
    range->last_attempt_ok = SendGlobalPositionTarget(bridge, status);
    if (!range->last_attempt_ok) {
        bridge->range_event = "uart1_error";
    } else {
        bridge->range_event = bridge->range_report ? "sent_86" : "probe_86";
    }
}

void MAVLink_ObserveGimbal(MAVLinkBridge_t *bridge, const GimbalStatus_t *status) {
    ObservePointing(bridge, status);
    ObserveRanging(bridge, status);
}

void MAVLink_FeedByte(MAVLinkBridge_t *bridge, uint8_t byte) {
    uint8_t result = mavlink_frame_char_buffer(&bridge->rx_message,
        &bridge->rx_status, byte, NULL, NULL);
    if (result == MAVLINK_FRAMING_OK) {
        HandleCommand(bridge);
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
    if (!SendMessage(bridge, &message, &bridge->heartbeat_trace, true)) {
        return false;
    }
    bridge->last_heartbeat_ms = now_ms;
    bridge->heartbeat_started = true;
    return true;
}

bool MAVLink_ReportTx(MAVLinkBridge_t *bridge, MAVLinkWrite_t write_text,
                      uint32_t now_ms) {
    MAVLinkTxTrace_t *trace;
    unsigned msgid;
    if (bridge->range_trace.length) {
        trace = &bridge->range_trace;
        msgid = MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT;
    } else if (bridge->heartbeat_trace.length) {
        trace = &bridge->heartbeat_trace;
        msgid = MAVLINK_MSG_ID_HEARTBEAT;
    } else if (bridge->ack_trace.length &&
               (!bridge->ack_trace_started ||
                now_ms - bridge->last_ack_trace_ms >= CONTROL_TEXT_PERIOD_MS)) {
        trace = &bridge->ack_trace;
        msgid = MAVLINK_MSG_ID_COMMAND_ACK;
        bridge->last_ack_trace_ms = now_ms;
        bridge->ack_trace_started = true;
    } else {
        return false;
    }

    static const char hex[] = "0123456789ABCDEF";
    char line[64U + 3U * MAVLINK_TX_WIRE_CAPACITY + 2U];
    int length = snprintf(line, sizeof(line),
        "[UART1_TX] msgid=%u len=%u result=%s hex=",
        msgid, (unsigned)trace->length, trace->result);
    if (length > 0 && (size_t)length + 3U * trace->length + 2U <= sizeof(line)) {
        for (uint16_t i = 0; i < trace->length; i++) {
            uint8_t byte = trace->wire[i];
            if (i != 0) {
                line[length++] = ' ';
            }
            line[length++] = hex[byte >> 4];
            line[length++] = hex[byte & 0x0F];
        }
        line[length++] = '\r';
        line[length++] = '\n';
        /* A debug-port write failure must not retry ahead of live traffic. */
        write_text(bridge->context, (const uint8_t *)line, (uint16_t)length);
    }
    trace->length = 0;
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

bool MAVLink_ReportControlOwner(MAVLinkBridge_t *bridge, MAVLinkWrite_t write_text) {
    char line[140];
    if (bridge->point_event) {
        int n = snprintf(line, sizeof(line),
            "[POINT] task=%lu event=%s first_yaw_cd=%d first_pitch_cd=%d\r\n",
            (unsigned long)bridge->task.id, bridge->point_event,
            bridge->first_yaw_deg100, bridge->first_pitch_deg100);
        if (n > 0 && (size_t)n < sizeof(line) &&
            write_text(bridge->context, (const uint8_t *)line, (uint16_t)n)) {
            bridge->point_event = NULL;
        }
    }
    if (bridge->range_event) {
        const GimbalStatus_t *sample = &bridge->ranging.last_attempt;
        int n = snprintf(line, sizeof(line),
            "[RANGE] event=%s lat_e7=%ld lon_e7=%ld alt_m=%d distance_dm=%u\r\n",
            bridge->range_event, (long)sample->target_lat_e7, (long)sample->target_lon_e7,
            sample->target_alt_m, (unsigned)sample->laser_distance_dm);
        if (n > 0 && (size_t)n < sizeof(line) &&
            write_text(bridge->context, (const uint8_t *)line, (uint16_t)n)) {
            bridge->range_event = NULL;
        }
    }
    if (!bridge->owner_report_pending) {
        return false;
    }
    const char *text = bridge->ui_control ? "[MODE] owner=ui\r\n" : "[MODE] owner=flight\r\n";
    if (!write_text(bridge->context, (const uint8_t *)text, (uint16_t)strlen(text))) {
        return false;
    }
    bridge->owner_report_pending = false;
    return true;
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
                         control->result == MOUNT_UI_CONTROL ? "ui_control" :
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
