#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "status_reporter.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

typedef struct {
    char text[4096];
    size_t text_length;
} Capture_t;

static bool capture_text(void *context, const uint8_t *data, uint16_t length) {
    Capture_t *capture = context;
    CHECK(capture->text_length + length < sizeof(capture->text));
    memcpy(capture->text + capture->text_length, data, length);
    capture->text_length += length;
    capture->text[capture->text_length] = '\0';
    return true;
}

static void feed(StatusReporter_t *bridge, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        StatusReporter_FeedStatus(bridge, data[i]);
    }
}

static void make_frame(uint8_t frame[67]) {
    memset(frame, 0, 67);
    const uint8_t header[] = {0xFC, 0x2C, 0x55, 0x3E, 0x14, 0x00};
    memcpy(frame, header, sizeof(header));
    uint8_t *data = frame + 6;
    data[1] = 0xFF;
    data[6] = 0x04;
    Vendor_WriteLE_Int16(data + 7, -8);
    Vendor_WriteLE_Int16(data + 9, 123);
    Vendor_WriteLE_Int16(data + 11, -35);
    Vendor_WriteLE_Int16(data + 25, 1234);
    Vendor_WriteLE_Int32(data + 27, -1144231691);
    Vendor_WriteLE_Int32(data + 31, 304538046);
    Vendor_WriteLE_Int16(data + 35, -42);
    Vendor_WriteLE_Int16(data + 39, -3532);
    Vendor_WriteLE_Int16(data + 41, 125);
    Vendor_WriteLE_Int16(data + 43, -3);
    data[46] = 0x72;
    data[47] = 0x13;
    data[57] = 0x01;
    frame[66] = Vendor_CalcXOR(frame + 3, 63);
}

static void test_signed_fields_and_complete_text(void) {
    Capture_t capture = {0};
    StatusReporter_t bridge;
    uint8_t frame[67];
    make_frame(frame);
    StatusReporter_Init(&bridge, capture_text, &capture);
    feed(&bridge, frame, 20);
    CHECK(!StatusReporter_Report(&bridge, 100));
    feed(&bridge, frame + 20, 47);
    CHECK(StatusReporter_Report(&bridge, 100));
    const char *expected =
        "[SYS] ready=1 selftest=0xFF servo=0x04\r\n"
        "[FRAME] yaw_deg=-0.08 pitch_deg=1.23 roll_deg=-0.35\r\n"
        "[ATT] yaw_deg=-35.32 pitch_deg=1.25 roll_deg=-0.03 cmd_echo=0x72 attitude_feedback=0x13\r\n"
        "[TARGET] tracking=0x01 distance_m=123.4 lon_deg=-114.4231691 lat_deg=30.4538046 alt_m=-42\r\n";
    CHECK(strcmp(capture.text, expected) == 0);
    /* The reporter has only a text callback, so it cannot send laser commands. */
    CHECK(!StatusReporter_Report(&bridge, 300)); /* No repeated old snapshots. */
    feed(&bridge, frame, 67);
    CHECK(!StatusReporter_Report(&bridge, 299));
    CHECK(StatusReporter_Report(&bridge, 300));
}

static void test_bad_xor_short_frames_and_recovery(void) {
    Capture_t capture = {0};
    StatusReporter_t bridge;
    uint8_t frame[67];
    make_frame(frame);
    StatusReporter_Init(&bridge, capture_text, &capture);
    frame[66] ^= 1;
    feed(&bridge, frame, 67);
    CHECK(!StatusReporter_Report(&bridge, 0));
    const uint8_t short_status[] = {0xFC, 0x2C, 0x55, 2, 0x14, 0, 0x16};
    feed(&bridge, short_status, sizeof(short_status));
    CHECK(!StatusReporter_Report(&bridge, 0));
    frame[66] ^= 1;
    const uint8_t noise[] = {0x00, 0xAB, 0xF0};
    feed(&bridge, noise, sizeof(noise));
    feed(&bridge, frame, 67);
    CHECK(StatusReporter_Report(&bridge, 0));
}

static void test_extreme_values_fit_without_float_printf(void) {
    Capture_t capture = {0};
    StatusReporter_t bridge;
    uint8_t frame[67];
    make_frame(frame);
    uint8_t *data = frame + 6;
    const unsigned offsets[] = {7, 9, 11, 39, 41, 43};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        Vendor_WriteLE_Int16(data + offsets[i], INT16_MIN);
    }
    Vendor_WriteLE_Int16(data + 25, -1); /* UINT16_MAX on the wire. */
    Vendor_WriteLE_Int32(data + 27, INT32_MIN);
    Vendor_WriteLE_Int32(data + 31, INT32_MAX);
    Vendor_WriteLE_Int16(data + 35, INT16_MIN);
    frame[66] = Vendor_CalcXOR(frame + 3, 63);
    StatusReporter_Init(&bridge, capture_text, &capture);
    feed(&bridge, frame, 67);
    CHECK(StatusReporter_Report(&bridge, UINT32_MAX - 100U));
    CHECK(strstr(capture.text, "yaw_deg=-327.68") != NULL);
    CHECK(strstr(capture.text, "distance_m=6553.5") != NULL);
    CHECK(strstr(capture.text, "lon_deg=-214.7483648 lat_deg=214.7483647 alt_m=-32768\r\n") != NULL);
    CHECK(capture.text_length < STATUS_TEXT_CAPACITY);
    feed(&bridge, frame, 67);
    CHECK(!StatusReporter_Report(&bridge, 98));
    CHECK(StatusReporter_Report(&bridge, 99)); /* 200ms across tick wrap. */
}

static void test_actual_gimbal_capture(void) {
    /* Unmodified 67-byte frame from the user's TTL capture, including XOR=37. */
    const char *hex =
        "FC 2C 55 3E 14 00 01 FF 40 00 2A 34 0C 08 00 7B 00 23 00 12 00 00 "
        "64 7C 02 60 02 00 00 2B 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "9E 02 36 FB 00 00 00 00 01 00 00 00 00 00 00 40 00 40 00 F0 00 00 00 37";
    uint8_t frame[67];
    for (unsigned i = 0; i < sizeof(frame); i++) {
        char *end;
        frame[i] = (uint8_t)strtoul(hex, &end, 16);
        CHECK(end != hex);
        hex = end;
    }
    CHECK(*hex == '\0');
    Capture_t capture = {0};
    StatusReporter_t bridge;
    StatusReporter_Init(&bridge, capture_text, &capture);
    feed(&bridge, frame, sizeof(frame));
    CHECK(StatusReporter_Report(&bridge, 0));
    CHECK(strstr(capture.text, "[SYS] ready=1 selftest=0xFF servo=0x0C\r\n") != NULL);
    CHECK(strstr(capture.text, "[FRAME] yaw_deg=0.08 pitch_deg=1.23 roll_deg=0.35\r\n") != NULL);
    CHECK(strstr(capture.text, "[ATT] yaw_deg=-12.26 pitch_deg=0.00 roll_deg=0.00") != NULL);
    CHECK(strstr(capture.text, "tracking=0x00 distance_m=0.0 lon_deg=0.0000000 lat_deg=0.0000000 alt_m=0") != NULL);
}

int main(void) {
    test_signed_fields_and_complete_text();
    test_bad_xor_short_frames_and_recovery();
    test_extreme_values_fit_without_float_printf();
    test_actual_gimbal_capture();
    puts("Status reporter: 4 tests PASS");
    return 0;
}
