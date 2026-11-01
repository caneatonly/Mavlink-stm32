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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "config.h"
#include "types.h"
#include "ring_buffer.h"
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
// UART接收缓冲
static uint8_t uart1_rx_byte;
static uint8_t uart2_rx_byte;

static uint8_t uart1_rx_buf[UART1_RX_BUFFER_SIZE];
static uint8_t uart2_rx_buf[UART2_RX_BUFFER_SIZE];

static RingBuffer_t rb_uart1;
static RingBuffer_t rb_uart2;

// 系统统计
#if DEBUG_STATS_ENABLE
SystemStats_t g_stats = {0};
#endif

// 超时检测
static uint32_t last_gimbal_status_time = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// LED调试函数
static void Debug_LED_On(void);
static void Debug_LED_Off(void);
static void Debug_LED_Toggle(void);
static void Debug_LED_Blink(uint8_t count);
static void Debug_LED_FastBlink(uint8_t count);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief LED控制函数 - 点亮
 */
static void Debug_LED_On(void)
{
#if DEBUG_LED_ENABLE
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  // PC13低电平点亮
#endif
}

/**
 * @brief LED控制函数 - 熄灭
 */
static void Debug_LED_Off(void)
{
#if DEBUG_LED_ENABLE
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    // PC13高电平熄灭
#endif
}

/**
 * @brief LED控制函数 - 翻转
 */
static void Debug_LED_Toggle(void)
{
#if DEBUG_LED_ENABLE
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
#endif
}

/**
 * @brief LED闪烁指示（慢速500ms间隔）
 * @param count 闪烁次数
 */
static void Debug_LED_Blink(uint8_t count)
{
#if DEBUG_LED_ENABLE
    for (uint8_t i = 0; i < count; i++) {
        Debug_LED_On();
        HAL_Delay(500);
        Debug_LED_Off();
        HAL_Delay(500);
    }
#endif
}

/**
 * @brief LED快闪指示（快速100ms间隔）
 * @param count 闪烁次数
 */
static void Debug_LED_FastBlink(uint8_t count)
{
#if DEBUG_LED_ENABLE
    for (uint8_t i = 0; i < count; i++) {
        Debug_LED_On();
        HAL_Delay(100);
        Debug_LED_Off();
        HAL_Delay(100);
    }
#endif
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
  // 初始化环形缓冲区
  RingBuffer_Init(&rb_uart1, uart1_rx_buf, UART1_RX_BUFFER_SIZE);
  RingBuffer_Init(&rb_uart2, uart2_rx_buf, UART2_RX_BUFFER_SIZE);

  // 启动UART中断接收
  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
  HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);

  // 启动指示：LED闪3次
  Debug_LED_Blink(3);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 测试：LED心跳闪烁（1秒1次）
    Debug_LED_Toggle();
    HAL_Delay(1000);

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
 * @note  在中断中被调用，将字节放入环形缓冲区
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        RingBuffer_Write(&rb_uart1, uart1_rx_byte);
#if DEBUG_STATS_ENABLE
        g_stats.uart1_rx_bytes++;
#endif
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);  // 重启接收
    }

    if (huart == &huart2) {
        RingBuffer_Write(&rb_uart2, uart2_rx_byte);
#if DEBUG_STATS_ENABLE
        g_stats.uart2_rx_bytes++;
#endif
        HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);  // 重启接收
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
