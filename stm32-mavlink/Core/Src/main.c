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
#include "ring_buffer.h"
#include "mavlink_handler.h"
#include "status_reporter.h"
#include <stdio.h>
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
static MAVLinkBridge_t mavlink_bridge;
static StatusReporter_t status_reporter;
static uint8_t uart1_rx_byte;
static uint8_t uart2_rx_byte;
static uint8_t uart1_rx_buf[UART1_RX_BUFFER_SIZE];
static uint8_t uart2_rx_buf[UART2_RX_BUFFER_SIZE];
static RingBuffer_t rb_uart1;
static RingBuffer_t rb_uart2;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static bool WriteToGimbal(void *context, const uint8_t *data, uint16_t length);
static bool WriteToFlight(void *context, const uint8_t *data, uint16_t length);
static bool WriteToComputer(void *context, const uint8_t *data, uint16_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Foreground blocking writes: RX interrupts remain enabled on both UARTs.
 * These HAL I/O limits are not a gimbal-link timeout policy. */
static bool WriteToGimbal(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return HAL_UART_Transmit(&huart2, (uint8_t *)data, length, 100) == HAL_OK;
}

static bool WriteToComputer(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return HAL_UART_Transmit(&huart3, (uint8_t *)data, length, 100) == HAL_OK;
}

static bool WriteToFlight(void *context, const uint8_t *data, uint16_t length) {
    (void)context;
    return HAL_UART_Transmit(&huart1, (uint8_t *)data, length, 100) == HAL_OK;
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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
    RingBuffer_Init(&rb_uart1, uart1_rx_buf, UART1_RX_BUFFER_SIZE);
    RingBuffer_Init(&rb_uart2, uart2_rx_buf, UART2_RX_BUFFER_SIZE);
    MAVLink_Init(&mavlink_bridge, WriteToFlight, WriteToGimbal, NULL);
    StatusReporter_Init(&status_reporter, WriteToComputer, NULL);
    HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
    HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);

    MAVLink_SendHeartbeatIfDue(&mavlink_bridge, HAL_GetTick());
    char boot[192];
    int boot_length = snprintf(boot, sizeof(boot),
        "\r\n[BOOT] TZ130TM NATIVE | UART1=FC UART2=GIMBAL UART3=LOG | "
        "115200 8N1 | yaw_sign=%d pitch_sign=%d | HB=1Hz TEXT=5Hz\r\n",
        GIMBAL_YAW_SIGN, GIMBAL_PITCH_SIGN);
    if (boot_length > 0 && (size_t)boot_length < sizeof(boot)) {
        WriteToComputer(NULL, (const uint8_t *)boot, (uint16_t)boot_length);
    }
/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint8_t byte;
    while (RingBuffer_Read(&rb_uart1, &byte)) {
        MAVLink_FeedByte(&mavlink_bridge, byte);
    }
    while (RingBuffer_Read(&rb_uart2, &byte)) {
        StatusReporter_FeedStatus(&status_reporter, byte);
    }
    uint32_t now_ms = HAL_GetTick();
    MAVLink_SendHeartbeatIfDue(&mavlink_bridge, now_ms);
    MAVLink_Report(&mavlink_bridge, WriteToComputer, now_ms);
    StatusReporter_Report(&status_reporter, now_ms);

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
/* Interrupts only capture bytes. Forwarding and formatting run in main(). */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        RingBuffer_Write(&rb_uart1, uart1_rx_byte);
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
    } else if (huart == &huart2) {
        RingBuffer_Write(&rb_uart2, uart2_rx_byte);
        HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->RxState == HAL_UART_STATE_READY) {
        if (huart == &huart1) {
            HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
        } else if (huart == &huart2) {
            HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
        }
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
