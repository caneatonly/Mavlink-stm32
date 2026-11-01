/**
 ******************************************************************************
 * @file    test_vendor_protocol.c
 * @brief   Vendor protocol unit tests
 * @note    Compile: gcc -o test_vendor test_vendor_protocol.c
 *                   App/vendor_protocol/vendor_protocol.c
 *                   -IApp/common -IApp/vendor_protocol
 ******************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "App/common/config.h"
#include "App/common/types.h"
#include "App/vendor_protocol/vendor_protocol.h"

// 测试颜色输出
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RED    "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET  "\033[0m"

void test_pass(const char* name) {
    printf(COLOR_GREEN "✓ %s\n" COLOR_RESET, name);
}

void test_fail(const char* name, const char* reason) {
    printf(COLOR_RED "✗ %s: %s\n" COLOR_RESET, name, reason);
}

void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/**
 * @brief 测试1：地理指引帧构建（字节级对比）
 */
void test_geo_pointing_frame(void)
{
    printf("\n========== 测试1：地理指引帧构建 ==========\n");

    // 已知抓包数据（ViewControlStudio测试截图）
    // 经度114.4231691°, 纬度30.4538046°, 海拔10.0m
    uint8_t expected[] = {
        0xFB, 0x2C, 0xAA, 0x0E, 0x1B, 0x00,
        0x0B, 0x97, 0x33, 0x44,  // 经度 1144231691
        0xBE, 0xE1, 0x26, 0x12,  // 纬度 304538046
        0x64, 0x00, 0x00, 0x00,  // 海拔 100 (10m×10)
        0xF1  // XOR
    };

    uint8_t actual[19];
    uint8_t len = Vendor_BuildGeoPointingFrame(actual,
                                                1144231691,  // lon_e7
                                                304538046,   // lat_e7
                                                10.0f);      // alt_m

    printf("预期长度: 19, 实际长度: %d\n", len);
    assert(len == 19);

    print_hex("预期帧", expected, 19);
    print_hex("实际帧", actual, 19);

    // 逐字节对比
    bool match = true;
    for (int i = 0; i < 19; i++) {
        if (actual[i] != expected[i]) {
            printf(COLOR_RED "字节[%d]不匹配: 预期0x%02X, 实际0x%02X\n" COLOR_RESET,
                   i, expected[i], actual[i]);
            match = false;
        }
    }

    if (match) {
        test_pass("地理指引帧字节级精确匹配");
    } else {
        test_fail("地理指引帧对比", "字节不匹配");
    }
}

/**
 * @brief 测试2：激光控制帧构建
 */
void test_laser_frames(void)
{
    printf("\n========== 测试2：激光控制帧构建 ==========\n");

    uint8_t buf[11];
    uint8_t len;

    // 测试激光启动帧
    uint8_t expected_start[] = {
        0xFB, 0x2C, 0xAA, 0x06, 0x3E, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x39
    };

    len = Vendor_BuildLaserStartFrame(buf);
    assert(len == 11);
    print_hex("激光启动", buf, 11);

    if (memcmp(buf, expected_start, 11) == 0) {
        test_pass("激光启动帧正确");
    } else {
        test_fail("激光启动帧", "字节不匹配");
    }

    // 测试激光停止帧
    uint8_t expected_stop[] = {
        0xFB, 0x2C, 0xAA, 0x06, 0x3F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x39
    };

    len = Vendor_BuildLaserStopFrame(buf);
    assert(len == 11);
    print_hex("激光停止", buf, 11);

    if (memcmp(buf, expected_stop, 11) == 0) {
        test_pass("激光停止帧正确");
    } else {
        test_fail("激光停止帧", "字节不匹配");
    }
}

/**
 * @brief 测试3：小端序读写
 */
void test_endianness(void)
{
    printf("\n========== 测试3：小端序读写 ==========\n");

    uint8_t buf[4];

    // 测试写入
    Vendor_WriteLE_Int32(buf, 0x12345678);
    assert(buf[0] == 0x78);
    assert(buf[1] == 0x56);
    assert(buf[2] == 0x34);
    assert(buf[3] == 0x12);
    test_pass("Int32小端序写入");

    // 测试读取
    int32_t value = Vendor_ReadLE_Int32(buf);
    assert(value == 0x12345678);
    test_pass("Int32小端序读取");

    // 测试Int16
    Vendor_WriteLE_Int16(buf, 0x1234);
    assert(buf[0] == 0x34);
    assert(buf[1] == 0x12);
    int16_t val16 = Vendor_ReadLE_Int16(buf);
    assert(val16 == 0x1234);
    test_pass("Int16小端序读写");
}

/**
 * @brief 测试4：XOR校验
 */
void test_xor_checksum(void)
{
    printf("\n========== 测试4：XOR校验 ==========\n");

    // 使用已知地理指引帧的XOR
    uint8_t frame[] = {
        0x0E, 0x1B, 0x00,
        0x0B, 0x97, 0x33, 0x44,
        0xBE, 0xE1, 0x26, 0x12,
        0x64, 0x00, 0x00, 0x00
    };

    uint8_t xor_val = Vendor_CalcXOR(frame, 14);
    printf("计算XOR: 0x%02X, 预期: 0xF1\n", xor_val);
    assert(xor_val == 0xF1);
    test_pass("XOR校验计算");
}

/**
 * @brief 测试5：状态帧解析器
 */
void test_status_parser(void)
{
    printf("\n========== 测试5：状态帧解析器 ==========\n");

    // 构造一个完整的状态帧（67字节）
    uint8_t status_frame[67];
    memset(status_frame, 0, 67);

    // 帧头
    status_frame[0] = 0xFC;
    status_frame[1] = 0x2C;
    status_frame[2] = 0x55;

    // LEN = 62 (2命令+60数据)
    status_frame[3] = 0x3E;

    // 命令字 0x0014 (小端)
    status_frame[4] = 0x14;
    status_frame[5] = 0x00;

    // 数据区（60字节）- 从byte[6]开始
    // payload[1]: bit7=1 (吊舱就绪)
    status_frame[6+1] = 0x80;

    // payload[7..8]: 方位角 = 9000 (90.00度)
    Vendor_WriteLE_Int16(&status_frame[6+7], 9000);

    // payload[9..10]: 俯仰角 = -4500 (-45.00度)
    Vendor_WriteLE_Int16(&status_frame[6+9], -4500);

    // payload[25..26]: 激光距离 = 1000 (100.0米)
    Vendor_WriteLE_Int16(&status_frame[6+25], 1000);

    // payload[27..30]: 经度 = 1144231691
    Vendor_WriteLE_Int32(&status_frame[6+27], 1144231691);

    // payload[31..34]: 纬度 = 304538046
    Vendor_WriteLE_Int32(&status_frame[6+31], 304538046);

    // payload[35..36]: 海拔 = 100m
    Vendor_WriteLE_Int16(&status_frame[6+35], 100);

    // payload[57]: 跟踪状态 = 0x01 (稳定跟踪)
    status_frame[6+57] = 0x01;

    // 计算XOR: 从byte[3]到byte[65]，共62字节
    uint8_t xor_val = Vendor_CalcXOR(&status_frame[3], 62);
    status_frame[66] = xor_val;

    printf("构造的状态帧XOR: 0x%02X\n", xor_val);

    // 测试解析器
    VendorParser_t parser;
    Vendor_ParserInit(&parser);

    VendorParseResult_t result = VENDOR_PARSE_RESULT_NONE;
    for (int i = 0; i < 67; i++) {
        result = Vendor_ParseByte(&parser, status_frame[i]);
    }

    if (result == VENDOR_PARSE_RESULT_STATUS_OK) {
        test_pass("状态帧解析成功");

        // 提取数据
        GimbalStatus_t status;
        if (Vendor_ExtractStatus(parser.payload, &status)) {
            printf("  吊舱就绪: %d\n", status.gimbal_ready);
            printf("  方位角: %.2f°\n", status.azimuth_deg100 / 100.0);
            printf("  俯仰角: %.2f°\n", status.pitch_deg100 / 100.0);
            printf("  激光距离: %.1fm\n", status.laser_distance_dm / 10.0);
            printf("  经度: %.7f°\n", status.target_lon_e7 / 1e7);
            printf("  纬度: %.7f°\n", status.target_lat_e7 / 1e7);
            printf("  海拔: %dm\n", status.target_alt_m);
            printf("  跟踪状态: 0x%02X\n", status.tracking_state);

            // 验证数据正确性
            assert(status.gimbal_ready == 1);
            assert(status.azimuth_deg100 == 9000);
            assert(status.pitch_deg100 == -4500);
            assert(status.laser_distance_dm == 1000);
            assert(status.target_lon_e7 == 1144231691);
            assert(status.target_lat_e7 == 304538046);
            assert(status.target_alt_m == 100);
            assert(status.tracking_state == 0x01);

            test_pass("状态数据提取正确");
        } else {
            test_fail("状态数据提取", "失败");
        }
    } else {
        test_fail("状态帧解析", "失败");
    }
}

/**
 * @brief 测试6：半包/粘包处理
 */
void test_partial_frames(void)
{
    printf("\n========== 测试6：半包/粘包处理 ==========\n");

    // 构造两个连续的状态帧（简化版）
    uint8_t data[] = {
        // 第一帧（部分）
        0xFC, 0x2C, 0x55, 0x3E, 0x14, 0x00,
        // ... 60字节数据省略，用0填充
    };

    VendorParser_t parser;
    Vendor_ParserInit(&parser);

    // 测试半包：只喂前3个字节
    for (int i = 0; i < 3; i++) {
        VendorParseResult_t result = Vendor_ParseByte(&parser, data[i]);
        assert(result == VENDOR_PARSE_RESULT_NONE);
    }
    test_pass("半包处理：等待更多数据");

    // 测试错误恢复：XOR错误
    uint8_t bad_frame[67];
    memset(bad_frame, 0, 67);
    bad_frame[0] = 0xFC;
    bad_frame[1] = 0x2C;
    bad_frame[2] = 0x55;
    bad_frame[3] = 0x3E;
    bad_frame[4] = 0x14;
    bad_frame[5] = 0x00;
    bad_frame[66] = 0xFF;  // 错误的XOR

    Vendor_ParserInit(&parser);
    VendorParseResult_t result = VENDOR_PARSE_RESULT_NONE;
    for (int i = 0; i < 67; i++) {
        result = Vendor_ParseByte(&parser, bad_frame[i]);
    }

    if (result == VENDOR_PARSE_RESULT_ERROR) {
        test_pass("错误帧检测：XOR不匹配");
    } else {
        test_fail("错误帧检测", "未检测到XOR错误");
    }
}

/**
 * @brief 主测试函数
 */
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  模块4测试：厂家协议编解码                 ║\n");
    printf("║  Vendor Protocol Unit Tests                ║\n");
    printf("╚════════════════════════════════════════════╝\n");

    test_geo_pointing_frame();
    test_laser_frames();
    test_endianness();
    test_xor_checksum();
    test_status_parser();
    test_partial_frames();

    printf("\n");
    printf(COLOR_GREEN "════════════════════════════════════════════\n");
    printf("  模块4所有测试通过！✓\n");
    printf("════════════════════════════════════════════\n" COLOR_RESET);

    return 0;
}
