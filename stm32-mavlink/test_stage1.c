/**
 ******************************************************************************
 * @file    test_stage1.c
 * @brief   阶段1测试：在PC上验证基础模块
 * @note    使用gcc编译：gcc -o test_stage1 test_stage1.c App/common/ring_buffer.c
 ******************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "App/common/config.h"
#include "App/common/types.h"
#include "App/common/ring_buffer.h"

// 测试颜色输出
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RED    "\033[0;31m"
#define COLOR_RESET  "\033[0m"

void test_pass(const char* name) {
    printf(COLOR_GREEN "✓ %s PASSED\n" COLOR_RESET, name);
}

void test_fail(const char* name, const char* reason) {
    printf(COLOR_RED "✗ %s FAILED: %s\n" COLOR_RESET, name, reason);
}

/**
 * @brief 测试环形缓冲区
 */
void test_ring_buffer(void)
{
    printf("\n========== 测试环形缓冲区 ==========\n");

    uint8_t buf[16];
    RingBuffer_t rb;

    // 测试1：初始化
    RingBuffer_Init(&rb, buf, 16);
    assert(RingBuffer_Available(&rb) == 0);
    test_pass("RingBuffer_Init");

    // 测试2：写入和读取
    assert(RingBuffer_Write(&rb, 0x55) == true);
    assert(RingBuffer_Write(&rb, 0xAA) == true);
    assert(RingBuffer_Available(&rb) == 2);

    uint8_t byte;
    assert(RingBuffer_Read(&rb, &byte) == true);
    assert(byte == 0x55);
    assert(RingBuffer_Read(&rb, &byte) == true);
    assert(byte == 0xAA);
    assert(RingBuffer_Available(&rb) == 0);
    test_pass("RingBuffer_Write/Read");

    // 测试3：环形回绕
    for (int i = 0; i < 20; i++) {
        RingBuffer_Write(&rb, i);
        RingBuffer_Read(&rb, &byte);
        assert(byte == i);
    }
    test_pass("RingBuffer回绕");

    // 测试4：缓冲区满
    RingBuffer_Clear(&rb);
    for (int i = 0; i < 15; i++) {  // 最多15个（size-1）
        assert(RingBuffer_Write(&rb, i) == true);
    }
    assert(RingBuffer_Write(&rb, 0xFF) == false);  // 应该失败
    test_pass("RingBuffer满检测");

    // 测试5：Peek功能
    RingBuffer_Clear(&rb);
    RingBuffer_Write(&rb, 0x11);
    RingBuffer_Write(&rb, 0x22);
    RingBuffer_Write(&rb, 0x33);

    assert(RingBuffer_Peek(&rb, 0, &byte) == true && byte == 0x11);
    assert(RingBuffer_Peek(&rb, 1, &byte) == true && byte == 0x22);
    assert(RingBuffer_Peek(&rb, 2, &byte) == true && byte == 0x33);
    assert(RingBuffer_Available(&rb) == 3);  // Peek不移动指针
    test_pass("RingBuffer_Peek");

    printf(COLOR_GREEN "✓ 环形缓冲区所有测试通过！\n" COLOR_RESET);
}

/**
 * @brief 测试配置和类型定义
 */
void test_config_types(void)
{
    printf("\n========== 测试配置和类型定义 ==========\n");

    // 测试1：配置宏
    assert(UART1_RX_BUFFER_SIZE == 512);
    assert(UART2_RX_BUFFER_SIZE == 256);
    assert(STABLE_TRACKING_CONFIRM_FRAMES == 3);
    assert(LASER_MAX_DURATION_MS == 600000);
    test_pass("配置宏定义");

    // 测试2：数据结构大小
    GimbalStatus_t status;
    RoiCommand_t roi;
    TargetPosition_t target;
    LaserControl_t laser;

    memset(&status, 0, sizeof(status));
    memset(&roi, 0, sizeof(roi));
    memset(&target, 0, sizeof(target));
    memset(&laser, 0, sizeof(laser));

    printf("  GimbalStatus_t: %lu bytes\n", sizeof(GimbalStatus_t));
    printf("  RoiCommand_t: %lu bytes\n", sizeof(RoiCommand_t));
    printf("  TargetPosition_t: %lu bytes\n", sizeof(TargetPosition_t));
    printf("  LaserControl_t: %lu bytes\n", sizeof(LaserControl_t));
    test_pass("数据结构定义");

    // 测试3：枚举值
    assert(TRACKING_IDLE == 0x00);
    assert(TRACKING_STABLE == 0x01);
    assert(TRACKING_MEMORY == 0x02);
    assert(TRACKING_LOST == 0x03);
    assert(TRACKING_INTERCEPT == 0x05);
    test_pass("跟踪状态枚举");

    // 测试4：厂家命令字
    assert(VENDOR_CMD_GEO_POINTING == 0x001B);
    assert(VENDOR_CMD_LASER_START == 0x003E);
    assert(VENDOR_CMD_LASER_STOP == 0x003F);
    assert(VENDOR_CMD_STATUS_FEEDBACK == 0x0014);
    test_pass("厂家命令字定义");

    // 测试5：坐标范围
    assert(LAT_MIN_E7 == -900000000);
    assert(LAT_MAX_E7 == 900000000);
    assert(LON_MIN_E7 == -1800000000);
    assert(LON_MAX_E7 == 1800000000);
    test_pass("坐标范围定义");

    printf(COLOR_GREEN "✓ 配置和类型定义所有测试通过！\n" COLOR_RESET);
}

/**
 * @brief 主测试函数
 */
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  阶段1测试：基础设施层                    ║\n");
    printf("║  模块1: config.h + types.h                ║\n");
    printf("║  模块2: ring_buffer.c                     ║\n");
    printf("╚════════════════════════════════════════════╝\n");

    test_config_types();
    test_ring_buffer();

    printf("\n");
    printf(COLOR_GREEN "════════════════════════════════════════════\n");
    printf("  阶段1所有测试通过！✓\n");
    printf("════════════════════════════════════════════\n" COLOR_RESET);

    return 0;
}
