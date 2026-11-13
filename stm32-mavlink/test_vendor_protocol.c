#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vendor_protocol.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

int main(void) {
    uint8_t wire[11];
    const uint8_t positive[] = {0xFB, 0x2C, 0xAA, 6, 0x72, 0, 0xE8, 3, 0, 0, 0x9F};
    const uint8_t negative[] = {0xFB, 0x2C, 0xAA, 6, 0x72, 0, 0x18, 0xFC, 0, 0, 0x90};
    CHECK(Vendor_BuildAnglePointingFrame(wire, 1000, 0) == 11);
    CHECK(memcmp(wire, positive, sizeof(wire)) == 0);
    Vendor_BuildAnglePointingFrame(wire, -1000, 0);
    CHECK(memcmp(wire, negative, sizeof(wire)) == 0);
    Vendor_BuildAnglePointingFrame(wire, 500, -500);
    CHECK(wire[6] == 0xF4 && wire[7] == 1 && wire[8] == 0x0C && wire[9] == 0xFE);
    uint8_t checksum = 0;
    for (unsigned i = 3; i <= 9; i++) {
        checksum ^= wire[i];
    }
    CHECK(wire[10] == checksum);
    CHECK(Vendor_ReadLE_Int16(wire + 8) == -500);
    uint8_t data[4];
    Vendor_WriteLE_Int32(data, INT32_MIN);
    CHECK(Vendor_ReadLE_Int32(data) == INT32_MIN);
    Vendor_WriteLE_Int32(data, INT32_MAX);
    CHECK(Vendor_ReadLE_Int32(data) == INT32_MAX);
    Vendor_WriteLE_Int16(data, INT16_MIN);
    CHECK(Vendor_ReadLE_Int16(data) == INT16_MIN);
    puts("Vendor protocol: exact +10/-10 commands, negative pitch XOR, LE limits PASS");
    return 0;
}
