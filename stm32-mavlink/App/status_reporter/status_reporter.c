#include "status_reporter.h"
#include "text_format.h"
#include <stdio.h>
#include <string.h>

void StatusReporter_Init(StatusReporter_t *bridge, StatusWrite_t write_text, void *context) {
    memset(bridge, 0, sizeof(*bridge));
    Vendor_ParserInit(&bridge->parser);
    bridge->write_text = write_text;
    bridge->context = context;
}

bool StatusReporter_FeedStatus(StatusReporter_t *bridge, uint8_t byte) {
    VendorParseResult_t result = Vendor_ParseByte(&bridge->parser, byte);
    /* Only a complete 60-byte 0x0014 data block may be decoded as status. */
    if (result == VENDOR_PARSE_RESULT_STATUS_OK && bridge->parser.length == 62 &&
        Vendor_ExtractStatus(bridge->parser.payload, &bridge->status)) {
        bridge->status_pending = true;
        return true;
    }
    return false;
}

bool StatusReporter_Report(StatusReporter_t *bridge, uint32_t now_ms) {
    if (!bridge->status_pending ||
        (bridge->report_started &&
         (uint32_t)(now_ms - bridge->last_report_ms) < STATUS_TEXT_PERIOD_MS)) {
        return false;
    }

    const GimbalStatus_t *status = &bridge->status;
    const int16_t angles[] = {
        status->azimuth_deg100, status->pitch_deg100, status->roll_deg100,
        status->attitude_azimuth_deg100, status->attitude_pitch_deg100,
        status->attitude_roll_deg100
    };
    char angle_text[6][16];
    for (unsigned i = 0; i < 6; i++) {
        Text_FormatFixed(angle_text[i], sizeof(angle_text[i]), angles[i], 100U, 2);
    }
    char distance[16], longitude[20], latitude[20];
    Text_FormatFixed(distance, sizeof(distance), status->laser_distance_dm, 10U, 1);
    Text_FormatFixed(longitude, sizeof(longitude), status->target_lon_e7, 10000000U, 7);
    Text_FormatFixed(latitude, sizeof(latitude), status->target_lat_e7, 10000000U, 7);

    char text[STATUS_TEXT_CAPACITY];
    int length = snprintf(text, sizeof(text),
        "[SYS] ready=%u selftest=0x%02X servo=0x%02X\r\n"
        "[FRAME] yaw_deg=%s pitch_deg=%s roll_deg=%s\r\n"
        "[ATT] yaw_deg=%s pitch_deg=%s roll_deg=%s cmd_echo=0x%02X attitude_feedback=0x%02X\r\n"
        "[TARGET] tracking=0x%02X distance_m=%s lon_deg=%s lat_deg=%s alt_m=%d\r\n",
        (unsigned)status->gimbal_ready, (unsigned)status->self_test,
        (unsigned)status->servo_state,
        angle_text[0], angle_text[1], angle_text[2],
        angle_text[3], angle_text[4], angle_text[5],
        (unsigned)status->cmd_echo, (unsigned)status->attitude_complete,
        (unsigned)status->tracking_state, distance, longitude, latitude,
        (int)status->target_alt_m);
    if (length <= 0 || (size_t)length >= sizeof(text) ||
        !bridge->write_text(bridge->context, (const uint8_t *)text, (uint16_t)length)) {
        return false;
    }

    bridge->last_report_ms = now_ms;
    bridge->report_started = true;
    bridge->status_pending = false;
    return true;
}
