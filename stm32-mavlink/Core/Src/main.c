/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "config.h"
#include "types.h"
#include "ring_buffer.h"
#include "vendor_protocol.h"
#include "mavlink_handler.h"
#include "laser_fsm.h"
#include "target_validator.h"
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ========== 协议解析器 ========== */
static VendorParser_t vendor_parser;
static GimbalStatus_t gimbal_status;

/* ========== 激光控制状态机 ========== */
static LaserControl_t laser_ctrl;

/* ========== UART单字节接收缓冲 ========== */
static uint8_t uart1_rx_byte;
static uint8_t uart2_rx_byte;

/* ========== Ring Buffer ========== */
static uint8_t uart1_rx_buf[UART1_RX_BUFFER_SIZE];
static uint8_t uart2_rx_buf[UART2_RX_BUFFER_SIZE];
static RingBuffer_t rb_uart1;
static RingBuffer_t rb_uart2;

/* ========== UART发送缓冲 ========== */
static uint8_t uart1_tx_buf[280];  // MAVLink最大帧长
static uint8_t uart2_tx_buf[32];   // 厂家协议最大帧19字节

/* ========== UART发送状态标志 ========== */
static volatile bool uart1_tx_busy = false;
static volatile bool uart2_tx_busy = false;

/* ========== 统计信息 ========== */
#if DEBUG_STATS_ENABLE
SystemStats_t g_stats = {0};
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ProcessGimbalStatus(void);
static bool SendToPixhawk(uint8_t *data, uint16_t len);
static bool SendToGimbal(uint8_t *data, uint8_t len);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief 非阻塞发送数据到Pixhawk (UART1)
 * @param data 发送缓冲区指针
 * @param len  数据长度
 * @return true=成功启动发送, false=UART忙
 * @note  发送期间data缓冲区不能被修改
 */
static bool SendToPixhawk(uint8_t *data, uint16_t len)
{
    if (!uart1_tx_busy) {
        uart1_tx_busy = true;
        if (HAL_UART_Transmit_IT(&huart1, data, len) == HAL_OK) {
            return true;
        } else {
            uart1_tx_busy = false;  // 发送失败，清除标志
            return false;
        }
    }
    return false;  // UART忙，丢弃本次发送
}

/**
 * @brief 非阻塞发送数据到吊舱 (UART2)
 * @param data 发送缓冲区指针
 * @param len  数据长度
 * @return true=成功启动发送, false=UART忙
 * @note  发送期间data缓冲区不能被修改
 */
static bool SendToGimbal(uint8_t *data, uint8_t len)
{
    if (!uart2_tx_busy) {
        uart2_tx_busy = true;
        if (HAL_UART_Transmit_IT(&huart2, data, len) == HAL_OK) {
            return true;
        } else {
            uart2_tx_busy = false;  // 发送失败，清除标志
            return false;
        }
    }
    return false;  // UART忙，丢弃本次发送
}

/**
 * @brief 处理吊舱状态（核心控制逻辑）
 * @note  在主循环中调用，每收到完整状态帧执行一次（50Hz）
 */
static void ProcessGimbalStatus(void)
{
    /* ===== 步骤1: 更新激光状态机 ===== */
    LaserAction_t action = Laser_Update(&laser_ctrl, &gimbal_status);

    /* ===== 步骤2: 执行激光控制命令 ===== */
    if (action == LASER_ACTION_START) {
        // 启动激光测距
        uint8_t len = Vendor_BuildLaserStartFrame(uart2_tx_buf);
        if (SendToGimbal(uart2_tx_buf, len)) {
            // 确认发送成功
            Laser_ConfirmStart(&laser_ctrl);

            #if DEBUG_STATS_ENABLE
            g_stats.laser_start_count++;
            #endif
        }
    }
    else if (action == LASER_ACTION_STOP) {
        // 停止激光测距
        uint8_t len = Vendor_BuildLaserStopFrame(uart2_tx_buf);
        if (SendToGimbal(uart2_tx_buf, len)) {
            // 确认发送成功
            Laser_ConfirmStop(&laser_ctrl);

            #if DEBUG_STATS_ENABLE
            g_stats.laser_stop_count++;
            #endif
        }
    }

    /* ===== 步骤3: 目标位置验证与上报 ===== */
    if (Laser_IsActive(&laser_ctrl)) {
        TargetPosition_t target;

        // 验证坐标有效性
        if (Target_Validate(&gimbal_status, &target)) {
            // 构建MAVLink目标位置消息
            uint16_t tx_len;
            if (MAVLink_SendTargetPosition(&target, uart1_tx_buf, &tx_len)) {
                // 发送给Pixhawk（非阻塞）
                SendToPixhawk(uart1_tx_buf, tx_len);
            }
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // 初始化协议解析器
  Vendor_ParserInit(&vendor_parser);
  MAVLink_Init();
  Laser_Init(&laser_ctrl);

  // 初始化Ring Buffer
  RingBuffer_Init(&rb_uart1, uart1_rx_buf, UART1_RX_BUFFER_SIZE);
  RingBuffer_Init(&rb_uart2, uart2_rx_buf, UART2_RX_BUFFER_SIZE);

  // 清零状态
  memset(&gimbal_status, 0, sizeof(gimbal_status));

  // 启动UART中断接收
  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
  HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* ===== 处理UART1数据（MAVLink） ===== */
    while (RingBuffer_Available(&rb_uart1) > 0) {
      uint8_t byte;
      RingBuffer_Read(&rb_uart1, &byte);

      RoiCommand_t roi;
      MAVLinkParseResult_t result = MAVLink_ParseByte(byte, &roi);

      if (result == MAVLINK_PARSE_RESULT_ROI_CMD) {
        // 转换为吊舱地理指向命令
        uint8_t len = Vendor_BuildGeoPointingFrame(
          uart2_tx_buf,
          roi.lon_e7,  // 厂家协议：经度在前
          roi.lat_e7,  // 纬度在后
          roi.alt_m
        );

        // 发送给吊舱（非阻塞）
        if (SendToGimbal(uart2_tx_buf, len)) {
          #if DEBUG_STATS_ENABLE
          g_stats.geo_frames_sent++;
          #endif
        }
      }
    }

    /* ===== 处理UART2数据（吊舱协议） ===== */
    while (RingBuffer_Available(&rb_uart2) > 0) {
      uint8_t byte;
      RingBuffer_Read(&rb_uart2, &byte);

      VendorParseResult_t result = Vendor_ParseByte(&vendor_parser, byte);

      if (result == VENDOR_PARSE_RESULT_STATUS_OK) {
        // 成功解析状态帧
        if (Vendor_ExtractStatus(vendor_parser.payload, &gimbal_status)) {
          gimbal_status.timestamp_ms = HAL_GetTick();

          #if DEBUG_STATS_ENABLE
          g_stats.vendor_parse_ok++;
          #endif

          // 处理吊舱状态（核心控制逻辑）
          ProcessGimbalStatus();
        }
      }
      else if (result == VENDOR_PARSE_RESULT_ERROR) {
        #if DEBUG_STATS_ENABLE
        g_stats.vendor_parse_error++;
        #endif
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief UART接收完成回调函数
 * @note  仅负责将字节快速搬运到Ring Buffer，立即返回
 *        中断耗时：<1μs
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        /* ===== UART1: Pixhawk MAVLink ===== */

        // 写入Ring Buffer
        RingBuffer_Write(&rb_uart1, uart1_rx_byte);

        #if DEBUG_STATS_ENABLE
        g_stats.uart1_rx_bytes++;
        #endif

        // 重启接收
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
    }

    else if (huart == &huart2) {
        /* ===== UART2: 吊舱厂家协议 ===== */

        // 写入Ring Buffer
        RingBuffer_Write(&rb_uart2, uart2_rx_byte);

        #if DEBUG_STATS_ENABLE
        g_stats.uart2_rx_bytes++;
        #endif

        // 重启接收
        HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
    }
}

/**
 * @brief UART发送完成回调函数
 * @note  清除发送忙标志，允许下次发送
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        uart1_tx_busy = false;
    }
    else if (huart == &huart2) {
        uart2_tx_busy = false;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
