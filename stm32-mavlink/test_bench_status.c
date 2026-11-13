#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vendor_protocol.h"

/* Avoid the Windows CRT assertion dialog in unattended host checks. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static void test_selected_status_fields(void) {
    uint8_t payload[62] = {0x14, 0x00};
    uint8_t *data = payload + 2;
    data[1] = 0xD7;
    data[6] = 0x0C;
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
    data[46] = 0x71;
    data[47] = 0x13;
    data[57] = 0x01;
    GimbalStatus_t status;
    memset(&status, 0xA5, sizeof(status));
    assert(Vendor_ExtractStatus(payload, &status));
    assert(status.self_test == 0xD7);
    assert(status.gimbal_ready == 1 && status.servo_state == 0x0C);
    assert(status.azimuth_deg100 == -8 && status.pitch_deg100 == 123);
    assert(status.roll_deg100 == -35);
    assert(status.attitude_azimuth_deg100 == -3532);
    assert(status.attitude_pitch_deg100 == 125);
    assert(status.attitude_roll_deg100 == -3);
    assert(status.laser_distance_dm == 1234);
    assert(status.target_lon_e7 == -1144231691);
    assert(status.target_lat_e7 == 304538046 && status.target_alt_m == -42);
    assert(status.cmd_echo == 0x71 && status.attitude_complete == 0x13);
    assert(status.tracking_state == 1 && status.valid);
}

int main(void) {
    test_selected_status_fields();
    puts("Selected status fields: PASS");
    return 0;
}
