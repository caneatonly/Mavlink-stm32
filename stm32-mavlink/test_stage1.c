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
void test_config_types(void) {
    GimbalStatus_t status = {0};
    assert(!status.valid);
    assert(MAVLINK_GIMBAL_COMPONENT_ID == 154);
    assert(MAVLINK_MOUNT_COMMAND == 205);
    assert(MAVLINK_HEARTBEAT_PERIOD_MS == 1000);
    assert(VENDOR_CMD_ANGLE_POINTING == 0x0072);
    assert(VENDOR_CMD_STATUS_FEEDBACK == 0x0014);
    test_pass("Native adapter configuration");
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
