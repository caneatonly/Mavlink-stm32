#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "mavlink_handler.h"
#include "vendor_protocol.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

typedef struct {
    unsigned flight_calls, gimbal_calls;
    uint8_t flight[32], gimbal[11];
    uint16_t flight_length;
    char text[512];
    bool flight_fails, gimbal_fails, text_fails;
} Capture_t;

static bool write_flight(void *context, const uint8_t *data, uint16_t length) {
    Capture_t *capture = context;
    CHECK(length <= sizeof(capture->flight));
    memcpy(capture->flight, data, length);
    capture->flight_length = length;
    capture->flight_calls++;
    return !capture->flight_fails;
}

static bool write_gimbal(void *context, const uint8_t *data, uint16_t length) {
    Capture_t *capture = context;
    CHECK(length == sizeof(capture->gimbal));
    memcpy(capture->gimbal, data, length);
    capture->gimbal_calls++;
    return !capture->gimbal_fails;
}

static bool write_text(void *context, const uint8_t *data, uint16_t length) {
    Capture_t *capture = context;
    CHECK(length < sizeof(capture->text));
    memcpy(capture->text, data, length);
    capture->text[length] = '\0';
    return !capture->text_fails;
}

static void init(MAVLinkBridge_t *bridge, Capture_t *capture) {
    memset(capture, 0, sizeof(*capture));
    MAVLink_Init(bridge, write_flight, write_gimbal, capture);
}

static void feed(MAVLinkBridge_t *bridge, const uint8_t *wire, size_t length) {
    for (size_t i = 0; i < length; i++) {
        MAVLink_FeedByte(bridge, wire[i]);
    }
}

/* Independent CRC check on the actual output bytes, including CRC_EXTRA. */
static uint16_t wire_crc(const uint8_t *data, size_t length, uint8_t extra) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i <= length; i++) {
        uint8_t temp = (i == length ? extra : data[i]) ^ (uint8_t)crc;
        temp ^= (uint8_t)(temp << 4);
        crc = (crc >> 8) ^ ((uint16_t)temp << 8) ^
              ((uint16_t)temp << 3) ^ (temp >> 4);
    }
    return crc;
}

static void check_ack(const Capture_t *capture, uint8_t result) {
    const uint8_t *wire = capture->flight;
    CHECK(capture->flight_length == 22);
    CHECK(wire[0] == 0xFD && wire[5] == 1 && wire[6] == 154 && wire[7] == 77);
    CHECK(wire[10] == 205 && wire[11] == 0 && wire[12] == result);
    CHECK(wire[18] == 1 && wire[19] == 1);
    CHECK(wire_crc(wire + 1, 19, 143) == ((uint16_t)wire[20] | (uint16_t)wire[21] << 8));
}

static uint16_t command(uint8_t *wire, float pitch, float roll, float yaw,
                        float mode, uint8_t src_sys, uint8_t src_comp,
                        uint8_t dst_sys, uint8_t dst_comp, bool v1, uint8_t confirm) {
    mavlink_message_t message;
    mavlink_status_t status = {0};
    mavlink_command_long_t payload = {0};
    status.flags = v1 ? MAVLINK_STATUS_FLAG_OUT_MAVLINK1 : 0;
    payload.command = 205;
    payload.param1 = pitch;
    payload.param2 = roll;
    payload.param3 = yaw;
    payload.param7 = mode;
    payload.target_system = dst_sys;
    payload.target_component = dst_comp;
    payload.confirmation = confirm;
    mavlink_msg_command_long_encode_status(src_sys, src_comp, &status, &message, &payload);
    return mavlink_msg_to_send_buffer(wire, &message);
}

static uint16_t mount_command(uint8_t *wire, float pitch, float yaw) {
    return command(wire, pitch, 0, yaw, 2, 1, 1, 1, 154, false, 0);
}

static void test_heartbeat_sequence_and_wrap(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    CHECK(MAVLink_SendHeartbeatIfDue(&bridge, 0));
    const uint8_t expected[] = {0xFD, 9, 0, 0, 0, 1, 154, 0, 0, 0,
                                0, 0, 0, 0, 26, 8, 0, 4, 3, 0x1A, 0xCF};
    CHECK(capture.flight_length == sizeof(expected));
    CHECK(memcmp(capture.flight, expected, sizeof(expected)) == 0);
    CHECK(!MAVLink_SendHeartbeatIfDue(&bridge, 999));
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mount_command(wire, 0, 10);
    feed(&bridge, wire, length);
    check_ack(&capture, 0);
    CHECK(capture.flight[4] == 1);
    CHECK(MAVLink_SendHeartbeatIfDue(&bridge, 1000) && capture.flight[4] == 2);
    init(&bridge, &capture);
    capture.flight_fails = true;
    CHECK(!MAVLink_SendHeartbeatIfDue(&bridge, UINT32_MAX - 500U));
    capture.flight_fails = false;
    CHECK(MAVLink_SendHeartbeatIfDue(&bridge, UINT32_MAX - 500U));
    CHECK(!MAVLink_SendHeartbeatIfDue(&bridge, 498));
    CHECK(MAVLink_SendHeartbeatIfDue(&bridge, 499));
}

static void test_partial_truncated_signed_angles_and_padding(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mount_command(wire, 0, 10);
    CHECK(wire[1] == 32 && length == 44);
    feed(&bridge, wire, length - 1);
    CHECK(capture.gimbal_calls == 0 && capture.flight_calls == 0);
    feed(&bridge, wire + length - 1, 1);
    const uint8_t expected[] = {0xFB, 0x2C, 0xAA, 6, 0x72, 0, 0xE8, 3, 0, 0, 0x9F};
    if (GIMBAL_YAW_SIGN == 1) {
        CHECK(memcmp(capture.gimbal, expected, sizeof(expected)) == 0);
    }
    check_ack(&capture, 0);
    length = command(wire, -5, 12, -10, 2, 1, 1, 1, 154, false, 7);
    CHECK(wire[1] == 33);
    feed(&bridge, wire, length);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 6) == -1000 * GIMBAL_YAW_SIGN);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == -500 * GIMBAL_PITCH_SIGN);
    CHECK(bridge.control.roll_deg == 12);
    length = mount_command(wire, -0.126f, 1.236f);
    feed(&bridge, wire, length);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 6) == 124 * GIMBAL_YAW_SIGN);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == -13 * GIMBAL_PITCH_SIGN);
    length = mount_command(wire, 0, 0);
    feed(&bridge, wire, length);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 6) == 0);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == 0);
}

static void test_crc_concatenation_and_v1(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mount_command(wire, 1, 2);
    wire[length - 1] ^= 1;
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 0 && capture.flight_calls == 0);
    wire[length - 1] ^= 1;
    feed(&bridge, wire, length);
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 2);
    length = command(wire, -5, 0, 5, 2, 1, 1, 1, 154, true, 0);
    CHECK(wire[0] == 0xFE && wire[1] == 33);
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 3);
    check_ack(&capture, 0);
    CHECK(MAVLink_SendHeartbeatIfDue(&bridge, 0) && capture.flight[0] == 0xFD);
}

static void test_address_filters_broadcast_and_other_command(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    const uint8_t ids[][4] = {{2, 1, 1, 154}, {1, 2, 1, 154},
                              {1, 1, 2, 154}, {1, 1, 1, 153}};
    for (unsigned i = 0; i < 4; i++) {
        uint16_t length = command(wire, 0, 0, 0, 2, ids[i][0], ids[i][1],
                                  ids[i][2], ids[i][3], false, 0);
        feed(&bridge, wire, length);
    }
    CHECK(capture.gimbal_calls == 0 && capture.flight_calls == 0);
    uint16_t length = command(wire, 0, 0, 0, 2, 1, 1, 0, 0, false, 0);
    CHECK(wire[1] == 29);
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 1);
    check_ack(&capture, 0);
    length = mount_command(wire, 0, 0);
    wire[38] = 204;
    uint16_t crc = wire_crc(wire + 1, length - 3, 152);
    wire[length - 2] = (uint8_t)crc;
    wire[length - 1] = (uint8_t)(crc >> 8);
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 1 && capture.flight_calls == 1);
}

static void test_mode_finite_range_and_send_failure(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = command(wire, 0, 0, 0, 4, 1, 1, 1, 154, false, 0);
    feed(&bridge, wire, length);
    check_ack(&capture, 3);
    const float bad[][2] = {{NAN, 0}, {0, INFINITY}, {0, 179.01f}, {0, -179.01f},
                            {120.01f, 0}, {-120.01f, 0}};
    for (unsigned i = 0; i < 6; i++) {
        length = mount_command(wire, bad[i][0], bad[i][1]);
        feed(&bridge, wire, length);
        check_ack(&capture, 2);
    }
    CHECK(capture.gimbal_calls == 0);
    length = mount_command(wire, 90 * GIMBAL_PITCH_SIGN, 179 * GIMBAL_YAW_SIGN);
    feed(&bridge, wire, length);
    check_ack(&capture, 0);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 6) == 17900);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == 9000);
    length = mount_command(wire, -120 * GIMBAL_PITCH_SIGN, -179 * GIMBAL_YAW_SIGN);
    feed(&bridge, wire, length);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == -12000);
    capture.gimbal_fails = true;
    length = mount_command(wire, 0, 0);
    feed(&bridge, wire, length);
    check_ack(&capture, 4);
}

static void test_report_coalesces_and_retries(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mount_command(wire, 1, 2);
    feed(&bridge, wire, length);
    capture.text_fails = true;
    CHECK(!MAVLink_Report(&bridge, write_text, 0));
    capture.text_fails = false;
    CHECK(MAVLink_Report(&bridge, write_text, 0));
    CHECK(strstr(capture.text, "[CTRL]") && strstr(capture.text, "result=sent"));
    length = mount_command(wire, 3, 4);
    feed(&bridge, wire, length);
    CHECK(!MAVLink_Report(&bridge, write_text, 199));
    CHECK(MAVLink_Report(&bridge, write_text, 200));
    CHECK(strstr(capture.text, "p1_pitch_deg=3.000") != NULL);
    CHECK(strstr(capture.text, "p3_yaw_deg=4.000") != NULL);
    CHECK(capture.gimbal_calls == 2 && !MAVLink_Report(&bridge, write_text, 400));
}

static void test_signed_packets_unknown_frames_and_noise(void) {
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t length = mount_command(wire, 1, 2);
    wire[2] = MAVLINK_IFLAG_SIGNED;
    uint16_t crc = wire_crc(wire + 1, length - 3, 152);
    wire[length - 2] = (uint8_t)crc;
    wire[length - 1] = (uint8_t)(crc >> 8);
    memset(wire + length, 0, MAVLINK_SIGNATURE_BLOCK_LEN);
    feed(&bridge, wire, length + MAVLINK_SIGNATURE_BLOCK_LEN);
    CHECK(capture.gimbal_calls == 0 && capture.flight_calls == 0);
    length = mount_command(wire, 1, 2);
    wire[7] = 200; /* Unknown message: consume it, then recover at the next frame. */
    feed(&bridge, wire, length);
    const uint8_t noise[] = {0, 0xFB, 0x2C, 0xAA, 1, 2, 3};
    feed(&bridge, noise, sizeof(noise));
    length = mount_command(wire, 1, 2);
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 1);
    check_ack(&capture, 0);
    capture.flight_fails = true;
    feed(&bridge, wire, length);
    CHECK(capture.gimbal_calls == 2 && !bridge.control.ack_sent);
    CHECK(MAVLink_Report(&bridge, write_text, 0));
    CHECK(strstr(capture.text, "ack=uart1_error") != NULL);
}

static void test_actual_capture(const char *path) {
    FILE *file = fopen(path, "r");
    CHECK(file != NULL);
    Capture_t capture;
    MAVLinkBridge_t bridge;
    init(&bridge, &capture);
    unsigned byte;
    while (fscanf(file, "%x", &byte) == 1) {
        CHECK(byte <= 255);
        MAVLink_FeedByte(&bridge, (uint8_t)byte);
    }
    CHECK(feof(file));
    fclose(file);
    CHECK(capture.gimbal_calls == 70 && capture.flight_calls == 70);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 6) == 7036 * GIMBAL_YAW_SIGN);
    CHECK(Vendor_ReadLE_Int16(capture.gimbal + 8) == 19 * GIMBAL_PITCH_SIGN);
    check_ack(&capture, 0);
}

int main(int argc, char **argv) {
    test_heartbeat_sequence_and_wrap();
    test_partial_truncated_signed_angles_and_padding();
    test_crc_concatenation_and_v1();
    test_address_filters_broadcast_and_other_command();
    test_mode_finite_range_and_send_failure();
    test_report_coalesces_and_retries();
    test_signed_packets_unknown_frames_and_noise();
    if (argc == 2) {
        test_actual_capture(argv[1]);
    }
    puts("MAVLink bridge: 7 groups + optional 70-frame capture PASS");
    return 0;
}
