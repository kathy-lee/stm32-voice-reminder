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
#include "dac.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "player.h"

#include <string.h>

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
/* M1 playback test: the USER button starts the compiled-in speech clip,
 * which repeats with a 2 s pause, and stops it again with a short fade. */
#define PLAYBACK_GAIN_Q15 6554       /* 0.20; tone test was clean up to here */
#define PAUSE_SAMPLES (2u * 8000u)   /* 2 s at 8 kHz */
#define DAC_BUFFER_SAMPLES 512       /* two halves of 32 ms each */

extern const uint8_t test_clip[];
extern const size_t test_clip_len;

static uint16_t dac_buffer[DAC_BUFFER_SAMPLES];
static player_t player;
static volatile uint8_t button_pressed;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uart_print(const char *text)
{
  HAL_UART_Transmit(&huart2, (const uint8_t *)text, (uint16_t)strlen(text), 100);
}

/* The DMA plays one half of dac_buffer while the other half is refilled. */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_cb)
{
  (void)hdac_cb;
  player_fill(&player, &dac_buffer[0], DAC_BUFFER_SAMPLES / 2);
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_cb)
{
  (void)hdac_cb;
  player_fill(&player, &dac_buffer[DAC_BUFFER_SAMPLES / 2], DAC_BUFFER_SAMPLES / 2);
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
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Start with the idle player: the DAC rests at mid-scale until the first
   * button press. TIM6 clocks the buffer out at 8 kHz. */
  player_fill(&player, dac_buffer, DAC_BUFFER_SAMPLES);
  if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)dac_buffer, DAC_BUFFER_SAMPLES,
                        DAC_ALIGN_12B_R) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  uart_print("ready: press USER to play\r\n");

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED2);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t last_toggle;
    static uint32_t last_press;

    if (HAL_GetTick() - last_toggle >= 500)
    {
      last_toggle = HAL_GetTick();
      BSP_LED_Toggle(LED2);
    }

    if (button_pressed)
    {
      button_pressed = 0;
      /* Ignore contact bounce: accept at most one press every 200 ms. */
      if (HAL_GetTick() - last_press >= 200)
      {
        last_press = HAL_GetTick();
        if (player_is_idle(&player))
        {
          /* The DMA interrupt also uses the player: keep it out while the
           * player is set up (one block decode, a few tens of microseconds). */
          __disable_irq();
          player_start(&player, test_clip, test_clip_len, PLAYBACK_GAIN_Q15, PAUSE_SAMPLES);
          __enable_irq();
          uart_print(player_is_idle(&player) ? "clip invalid\r\n" : "play\r\n");
        }
        else
        {
          player_stop(&player);
          uart_print("stop\r\n");
        }
      }
    }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == USER_BUTTON_PIN)
  {
    button_pressed = 1;
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
