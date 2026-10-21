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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "sdcard.h"
#include "sd_spi.h"
#include "wav_writer.h"

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
SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/*
 * variables for recording on computer
uint32_t micBuffer[512];
int16_t pcm16[512];
volatile uint32_t lastButtonTime = 0;
*/
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SAI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* Debug values */
static volatile uint32_t debugSamples = 0U;
static volatile uint32_t debugOverruns = 0U;
static volatile uint32_t debugRecordingError = 0U;
static volatile FRESULT debugResult = FR_OK;
static volatile uint32_t debugLastWriteMs = 0U;
static volatile uint32_t debugMaxWriteMs = 0U;

/* Audio configuration */
#define AUDIO_SAMPLE_RATE        35714U
#define AUDIO_BITS_PER_SAMPLE    16U
#define AUDIO_CHANNELS           1U

/*
 * TEMPORARY TEST SETTINGS:
 * Each WAV = 60 seconds
 * Total recording session = maximum 30 minutes
 *
 * After testing successfully:
 * FILE_SECONDS       -> 3600U
 * MAX_RECORD_SECONDS -> 86400U
 */
#define FILE_SECONDS             1800U   /* 30-minute WAV segments */

/* DMA audio buffers */
#define AUDIO_BUFFER_SAMPLES     8192U
#define HALF_BUFFER_SAMPLES      (AUDIO_BUFFER_SAMPLES / 2U)
#define PCM_QUEUE_BLOCKS         6U

#define WAV_HEADER_SIZE          44U

/* Recording functions */
static FRESULT Start_Recording(void);
static FRESULT Stop_Recording(void);
static FRESULT Open_Next_File(void);

static FRESULT Write_Queued_Block(void);
static void Queue_DMA_Half(uint32_t startIndex);

static void Find_Next_File_Index(void);
static void SD_SPI_SetFast(void);


/* DMA / button state */
static volatile uint32_t dmaOverrunCount = 0U;

static volatile uint8_t startRequest = 0U;
static volatile uint8_t stopRequest = 0U;

static volatile uint32_t lastButtonTime = 0U;


/* Audio buffers */
static uint32_t micBuffer[AUDIO_BUFFER_SAMPLES];
static int16_t pcmQueue[PCM_QUEUE_BLOCKS][HALF_BUFFER_SAMPLES];
static volatile uint8_t queueHead = 0U;
static volatile uint8_t queueTail = 0U;
static volatile uint8_t queueCount = 0U;


/* Current WAV file */
static FIL wav_file;


/* Recording state */
static volatile uint8_t recording = 0U;
static volatile uint8_t recordingCompleteRequest = 0U;
static volatile uint8_t recordingError = 0U;


/*
 * Samples written to the CURRENT WAV file.
 * Resets whenever a new 1-minute test file is opened.
 */
static volatile uint32_t fileSamplesWritten = 0U;

/*
 * Samples written during the WHOLE recording session.
 * Resets only when the user starts a new session.
 */
static volatile uint64_t totalSamplesWritten = 0U;


/* Number of samples allowed in each WAV file */
static const uint32_t samplesPerFile =
    AUDIO_SAMPLE_RATE * FILE_SECONDS;


static uint32_t fileIndex = 0U;
static char currentFilename[40];

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
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SAI1_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  //HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t*)micBuffer, 512);

  FATFS fs;
  FRESULT res = FR_OK;
  uint8_t sdReady = 0U;

  HAL_Delay(500);

  res = f_mount(&fs, "", 1);

  if (res == FR_OK)
  {
      /* Switch SD SPI to faster speed after successful initialization */
      SD_SPI_SetFast();

      /* Create recording directory */
      /* SD mounted successfully - record directly in root directory. */
      Find_Next_File_Index();
      sdReady = 1U;
      startRequest = 1U;
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (sdReady && startRequest && !recording)
    {
        startRequest = 0U;
        res = Start_Recording();
        debugResult = res;
        if (res == FR_OK)
        {
            if (HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)micBuffer, AUDIO_BUFFER_SAMPLES) != HAL_OK)
            {
                recordingError = 1U;
                recordingCompleteRequest = 1U;
            }
            else
            {
                /* Recording is active: green LED ON. */
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            }
        }
    }

    if (recording && queueCount > 0U)
    {
        res = Write_Queued_Block();
        debugResult = res;
    }

    if (recording && (stopRequest || recordingCompleteRequest))
    {
        stopRequest = 0U;
        HAL_SAI_DMAStop(&hsai_BlockA1);

        /* Drain every complete block captured before DMA was stopped. */
        while (queueCount > 0U && !recordingError)
        {
            res = Write_Queued_Block();
            if (res != FR_OK) break;
        }

        res = Stop_Recording();
        debugResult = res;
        recordingCompleteRequest = 0U;
        /* Recording is fully finalized: green LED OFF. */
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_RX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_32K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_MONOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_32BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);

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
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* Continuous SD-card recording helpers. */
static FRESULT Start_Recording(void)
{
    FRESULT result;

    fileSamplesWritten = 0U;
    totalSamplesWritten = 0U;
    queueHead = 0U;
    queueTail = 0U;
    queueCount = 0U;
    recordingCompleteRequest = 0U;
    recordingError = 0U;
    dmaOverrunCount = 0U;
    debugLastWriteMs = 0U;
    debugMaxWriteMs = 0U;

    snprintf(currentFilename, sizeof(currentFilename),
             "R%06lu.WAV", (unsigned long)fileIndex);

    result = WAV_Start(&wav_file, currentFilename,
                       AUDIO_SAMPLE_RATE, AUDIO_BITS_PER_SAMPLE, AUDIO_CHANNELS);
    if (result != FR_OK)
    {
        recording = 0U;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        return result;
    }

    recording = 1U;
    return FR_OK;
}

static FRESULT Stop_Recording(void)
{
    FRESULT result;

    /* DMA is stopped and the queue is drained by main() before this call. */
    recording = 0U;
    result = WAV_Finish(&wav_file);

    debugSamples = fileSamplesWritten;
    debugOverruns = dmaOverrunCount;
    debugRecordingError = recordingError;
    debugResult = result;

    if (result == FR_OK)
    {
        fileIndex++;
    }

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    return result;
}

static FRESULT Open_Next_File(void)
{
    FRESULT result;

    fileIndex++;
    fileSamplesWritten = 0U;

    snprintf(currentFilename, sizeof(currentFilename),
             "R%06lu.WAV", (unsigned long)fileIndex);

    result = WAV_Start(&wav_file, currentFilename,
                       AUDIO_SAMPLE_RATE, AUDIO_BITS_PER_SAMPLE, AUDIO_CHANNELS);
    return result;
}

static FRESULT Write_Queued_Block(void)
{
    FRESULT result;
    uint8_t tail;
    uint32_t writeStartTime;

    __disable_irq();
    if (queueCount == 0U)
    {
        __enable_irq();
        return FR_OK;
    }
    tail = queueTail;
    __enable_irq();

    writeStartTime = HAL_GetTick();
    result = WAV_WriteSamples(&wav_file, pcmQueue[tail], HALF_BUFFER_SAMPLES);
    debugLastWriteMs = HAL_GetTick() - writeStartTime;
    if (debugLastWriteMs > debugMaxWriteMs)
    {
        debugMaxWriteMs = debugLastWriteMs;
    }

    if (result != FR_OK)
    {
        recordingError = 1U;
        recordingCompleteRequest = 1U;
        return result;
    }

    fileSamplesWritten += HALF_BUFFER_SAMPLES;
    totalSamplesWritten += HALF_BUFFER_SAMPLES;

    __disable_irq();
    queueTail = (uint8_t)((queueTail + 1U) % PCM_QUEUE_BLOCKS);
    queueCount--;
    __enable_irq();

    if (fileSamplesWritten >= samplesPerFile)
    {
        result = WAV_Finish(&wav_file);
        if (result == FR_OK)
        {
            result = Open_Next_File();
        }
        if (result != FR_OK)
        {
            recordingError = 1U;
            recordingCompleteRequest = 1U;
        }
    }

    return result;
}

static void Queue_DMA_Half(uint32_t startIndex)
{
    uint8_t head;

    if (!recording)
    {
        return;
    }

    if (queueCount >= PCM_QUEUE_BLOCKS)
    {
        dmaOverrunCount++;
        recordingError = 1U;
        recordingCompleteRequest = 1U;
        return;
    }

    head = queueHead;
    for (uint32_t i = 0U; i < HALF_BUFFER_SAMPLES; i++)
    {
        pcmQueue[head][i] =
            (int16_t)((int32_t)micBuffer[startIndex + i] >> 14);
    }

    queueHead = (uint8_t)((queueHead + 1U) % PCM_QUEUE_BLOCKS);
    queueCount++;
}

static void Find_Next_File_Index(void)
{
    FILINFO fileInfo;

    fileIndex = 0U;
    while (1)
    {
        snprintf(currentFilename, sizeof(currentFilename),
                 "R%06lu.WAV", (unsigned long)fileIndex);
        if (f_stat(currentFilename, &fileInfo) != FR_OK)
        {
            break;
        }
        fileIndex++;
    }
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        Queue_DMA_Half(0U);
    }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1)
    {
        Queue_DMA_Half(HALF_BUFFER_SAMPLES);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        uint32_t now = HAL_GetTick();

        if ((now - lastButtonTime) < 300U)
        {
            return;
        }
        lastButtonTime = now;

        /* Only request the operation here; FatFs work stays in main(). */
        if (recording)
        {
            stopRequest = 1U;
            startRequest = 0U;
        }
        else
        {
            startRequest = 1U;
            stopRequest = 0U;
        }
    }
}

static void SD_SPI_SetFast(void)
{
    HAL_SPI_DeInit(&hspi2);
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
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
