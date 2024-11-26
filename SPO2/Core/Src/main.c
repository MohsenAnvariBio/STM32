/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../lvgl/lvgl.h"
#include "../../lv_conf.h"
#include "tft.h"
#include "touchpad.h"
#include "pulse_oximeter.h"
#include "system.h"
#include "string.h"
#include <stdio.h>
#include "chart.h"
#include "filter.h"
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
I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
volatile uint8_t pulseOximiterIntFlag = 0;
#define FILTER_LENGTH 5       // Length of the high-pass filter buffer
#define MOVING_AVG_LENGTH 2    // Length of the moving average buffer
#define ALPHA 0.95f // High-pass filter coefficient
#define M 10 // Size of the buffer
#define DATA_LENGTH  1000  // Length of dataBuffer

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
float highPassFilter(float input, float *prevInput, float *prevOutput, float alpha);
float mean(const float *buffer, uint16_t size);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

//  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  lv_init();
  tft_init();
  lv_disp_set_rotation(lv_disp_get_default(), LV_DISP_ROT_270);
  touchpad_init();

  setup_ui();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  FIFO_LED_DATA fifoLedData;
  pulseOximeter_resetRegisters();
  pulseOximeter_initFifo();
  pulseOximeter_setSampleRate(_800SPS);
  pulseOximeter_setPulseWidth(_411_US);
  pulseOximeter_setLedCurrent(RED_LED, 50);
  pulseOximeter_setLedCurrent(IR_LED, 5);
  pulseOximeter_resetFifo();
  pulseOximeter_setMeasurementMode(SPO2);

  // High-pass filter state variables
  float prevInput = 0.0f, prevOutput = 0.0f;
  float buffer[M] = {0};
  int i = 0, j = 0;
  int filled = 0, filled2 = 0;
  uint32_t R[DATA_LENGTH] = {0};
  uint32_t R_count = 0;
  float bufferDetectPeak[DATA_LENGTH] = {0};


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (PUSLE_OXIMETER_INTERRUPT == 1) {
		  if (pulseOximiterIntFlag) {
			  pulseOximiterIntFlag = 0;
			  fifoLedData = pulseOximeter_readFifo();
			  float ppg_signal = (float)fifoLedData.irLedRaw;

			  // High-pass filter
			  float ppg_signal_rdc = highPassFilter(ppg_signal, &prevInput, &prevOutput, 0.95f);

			  if (is_moving_average_enabled()) {
				  // Moving average enabled
				  if (i < M) {
					  buffer[i] = ppg_signal_rdc;
					  i++;
					  if (i == M) {
						  filled = 1;
					  }
				  } else if (filled) {
					  float output = mean(buffer, M);
					  update_chart_with_gain(output);

					  if (j < DATA_LENGTH) {
						  bufferDetectPeak[j] = -output/40;
//						  printf('%.2f', bufferDetectPeak[j]);
					  	  j++;
					  	  if (j == DATA_LENGTH) {
					  		  filled2 = 1;
					  	  }
					  } else if (filled2){
						  //If the following problems happen during compiling by STM32CubeIDE, go to "Project > Properties > C/C++ Build > Settings > Tool Settings > MCU Settings" and then check the box "Use float with printf from newlib-nano (-u _printf_float)."
//						  printf('%.2f', bufferDetectPeak);


						  findPeaks(bufferDetectPeak, DATA_LENGTH, R, &R_count);
						  update_SPO2(calculateMean(bufferDetectPeak, DATA_LENGTH));
						  update_HR(heartRate(R, R_count));


						  filled2 = 0;
						  j = 0;
						  R_count= 0;

						  memset(bufferDetectPeak, 0, sizeof(bufferDetectPeak));
						  memset(R, 0, sizeof(R));
					  }

					  // Shift buffer elements
					  for (int j = 0; j < M - 1; j++) {
						  buffer[j] = buffer[j + 1];
					  }
					  buffer[M - 1] = ppg_signal_rdc;
				  }
			  } else {
				  // Only high-pass filter
				  update_chart_with_gain(ppg_signal_rdc);
			  }

			  pulseOximeter_clearInterrupt();
		  }
	  }

	  HAL_Delay(1);
	  lv_timer_handler();
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == GPIO_PIN_9)
	{
		pulseOximiterIntFlag = 1;
	}
}

// High-pass filter function
float highPassFilter(float input, float *prevInput, float *prevOutput, float alpha) {
    float inputF = input; // No need to cast, input is already float
    float output = alpha * (*prevOutput + inputF - *prevInput);
    *prevInput = inputF;
    *prevOutput = output;
    return output;
}

// Function to calculate the mean of an array
float mean(const float *buffer, uint16_t size) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}


void highPassFilterWithBuffer(float input, float *inputBuffer, float *outputBuffer, float *filteredSample, float alpha) {
    static int index = 0; // Circular buffer index

    // Add new input to the buffer (circular behavior)
    inputBuffer[index] = input;

    // Compute the high-pass filter output
    int prevIndex = (index - 1 + FILTER_LENGTH) % FILTER_LENGTH; // Previous index in circular buffer
    outputBuffer[index] = alpha * (outputBuffer[prevIndex] + inputBuffer[index] - inputBuffer[prevIndex]);

    // Save the current filtered sample for visualization
    *filteredSample = outputBuffer[index];

    // Advance the circular buffer index
    index = (index + 1) % FILTER_LENGTH;
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

#ifdef  USE_FULL_ASSERT
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
