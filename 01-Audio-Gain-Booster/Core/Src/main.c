/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Real-Time Audio Gain Booster with Digital Normalization
  * @author         : Bertan Kuzeyli
  * @date           : 2026
  ******************************************************************************
  * @attention
  *
  * This project implements a real-time audio processing system that:
  * - Captures audio from an I2S MEMS microphone
  * - Removes DC offset for clean signal processing
  * - Applies intelligent normalization (automatic gain control)
  * - Outputs amplified audio to a DAC module via I2S
  * - Streams processed data to PC via USB CDC
  *
  * Key Features:
  * - Ultra-low latency: 8 samples per block (~166μs at 48kHz)
  * - ARM CMSIS-DSP optimized processing
  * - Ping-pong DMA buffering for continuous operation
  * - Automatic peak limiting to prevent distortion
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "usbd_cdc_if.h"  // USB CDC (Virtual COM Port) interface
#include "arm_math.h"     // ARM CMSIS-DSP library for optimized math
#include <math.h>         // Standard math functions
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ============================================================================
// SYSTEM PARAMETERS
// ============================================================================

#define NUM_SAMPLES             8   // Samples per processing block (ultra-low latency: ~166μs)
#define I2S_BUFFER_SIZE         (NUM_SAMPLES * 2 * 2)  // Total buffer: 8 samples × 2 channels × 2 halves = 32

// USB packet markers for data integrity
#define SOF_WORD                0x55AA  // Start of Frame marker
#define EOF_BYTE                0x0A    // End of Frame marker (newline)

// Audio processing constants
#define INV_MAX_24BIT           1.1920928955078125e-7f  // 1/(2^23) for 24-bit normalization
#define BLOCK_SIZE              NUM_SAMPLES

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

I2S_HandleTypeDef hi2s1;
I2S_HandleTypeDef hi2s3;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi3_tx;

/* USER CODE BEGIN PV */

// ============================================================================
// DMA BUFFERS (Ping-Pong Architecture)
// ============================================================================
// While one half is being processed, DMA fills the other half - zero data loss

int32_t i2s_rx_buffer[I2S_BUFFER_SIZE] __attribute__((aligned(32)));  // Mic input buffer (32-byte aligned for DMA)
int32_t i2s_tx_buffer[I2S_BUFFER_SIZE] __attribute__((aligned(32)));  // DAC output buffer

// Pointer to active buffer half being processed
volatile int32_t* volatile active_buffer = NULL;

// ============================================================================
// USB PACKET STRUCTURE
// ============================================================================
// Sends processed audio samples to PC for visualization/analysis

typedef struct __attribute__((packed)) {
    uint16_t sof;                   // Start marker (0x55AA)
    float samples[NUM_SAMPLES];     // Float audio samples
    uint8_t eof;                    // End marker (0x0A)
} usb_packet_t;

usb_packet_t usb_packet __attribute__((aligned(4)));

// ============================================================================
// DSP PROCESSING BUFFERS
// ============================================================================

float32_t input_buffer[BLOCK_SIZE] __attribute__((aligned(4)));   // Input: converted from I2S
float32_t output_buffer[BLOCK_SIZE] __attribute__((aligned(4)));  // Output: after normalization
float32_t temp_buffer[BLOCK_SIZE] __attribute__((aligned(4)));    // Temporary: for calculations

// ============================================================================
// DC OFFSET REMOVAL PARAMETERS
// ============================================================================
// DC offset = unwanted constant voltage that shifts signal away from zero
// High-pass filtering removes this to prevent downstream issues

float32_t dc_offset = 0.0f;           // Current DC estimate
const float32_t DC_ALPHA = 0.9995f;   // Smoothing factor (closer to 1.0 = slower adaptation)

// ============================================================================
// NORMALIZATION PARAMETERS (Automatic Gain Control)
// ============================================================================
// These values control how aggressively the system amplifies quiet signals

const float32_t TARGET_PEAK = 0.9f;    // Target peak level (0.9 = 90% of maximum, leaves headroom)
const float32_t MAX_GAIN = 10.0f;      // Maximum amplification (10x = 20dB boost)
const float32_t MIN_SIGNAL = 0.00001f; // Minimum signal threshold (ignore noise floor)

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S1_Init(void);
static void MX_I2S3_Init(void);
/* USER CODE BEGIN PFP */

// Audio processing pipeline functions
void audio_init(void);
void i2s_to_float(int32_t* i2s_data, float32_t* float_data, uint32_t num_samples);
void float_to_i2s_stereo(float32_t* float_data, int32_t* i2s_data, uint32_t num_samples);
void remove_dc_offset(float32_t* data, uint32_t num_samples);
void normalize_audio(float32_t* input, float32_t* output, uint32_t num_samples);
void send_usb_packet(float32_t* data, uint32_t num_samples);
void process_audio_block(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  ******************************************************************************
  * @brief  Initialize audio processing parameters
  * @retval None
  ******************************************************************************
  */
void audio_init(void)
{
    dc_offset = 0.0f;  // Reset DC offset estimate
}

/**
  ******************************************************************************
  * @brief  Convert I2S samples to normalized floating-point values
  * @param  i2s_data: Raw 24-bit I2S data (stereo, left-aligned)
  * @param  float_data: Output floating-point array
  * @param  num_samples: Number of mono samples to convert
  * @retval None
  *
  * @note   I2S format: 32-bit word containing 24-bit data (MSB-aligned)
  *         We extract only the left channel and normalize to [-1.0, 1.0]
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void i2s_to_float(int32_t* i2s_data, float32_t* float_data, uint32_t num_samples)
{
    for (uint32_t i = 0; i < num_samples; i++)
    {
        // Extract left channel sample (every other word in stereo buffer)
        int32_t raw = i2s_data[i * 2];

        // Shift right by 8 bits to align 24-bit data properly
        raw = raw >> 8;

        // Normalize to floating-point range [-1.0, 1.0]
        float_data[i] = (float32_t)raw * INV_MAX_24BIT;
    }
}

/**
  ******************************************************************************
  * @brief  Convert normalized float samples to I2S format for DAC output
  * @param  float_data: Processed floating-point samples
  * @param  i2s_data: Output I2S buffer (stereo, 24-bit)
  * @param  num_samples: Number of mono samples to convert
  * @retval None
  *
  * @note   Converts mono processed audio to stereo by duplicating to both channels
  *         Scales float [-1.0, 1.0] back to 24-bit integer format
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void float_to_i2s_stereo(float32_t* float_data, int32_t* i2s_data, uint32_t num_samples)
{
    const float32_t SCALE_FACTOR = 8388608.0f;  // 2^23 (max value for 24-bit signed)

    for (uint32_t i = 0; i < num_samples; i++)
    {
        // Convert float back to 24-bit integer
        int32_t val = (int32_t)(float_data[i] * SCALE_FACTOR);

        // Left-align for I2S Philips standard (shift left by 8 bits)
        int32_t formatted_sample = val << 8;

        // Duplicate mono signal to both stereo channels
        i2s_data[i * 2]     = formatted_sample;  // Left channel
        i2s_data[i * 2 + 1] = formatted_sample;  // Right channel
    }
}

/**
  ******************************************************************************
  * @brief  Remove DC offset from audio signal
  * @param  data: Audio buffer (in-place processing)
  * @param  num_samples: Number of samples
  * @retval None
  *
  * @note   DC offset = unwanted constant voltage in signal
  *         Uses exponential moving average for smooth tracking
  *         CMSIS-DSP functions used for optimization
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void remove_dc_offset(float32_t* data, uint32_t num_samples)
{
    // Calculate mean value of current block
    float32_t block_mean;
    arm_mean_f32(data, num_samples, &block_mean);

    // Update DC offset estimate using exponential moving average
    // dc_offset(n) = α * dc_offset(n-1) + (1-α) * block_mean
    dc_offset = DC_ALPHA * dc_offset + (1.0f - DC_ALPHA) * block_mean;

    // Subtract DC offset from all samples (in-place)
    arm_offset_f32(data, -dc_offset, data, num_samples);
}

/**
  ******************************************************************************
  * @brief  Normalize audio to target peak level (Automatic Gain Control)
  * @param  input: Input audio buffer
  * @param  output: Normalized audio output
  * @param  num_samples: Number of samples
  * @retval None
  *
  * @note   Algorithm:
  *         1. Find maximum absolute value in block
  *         2. Calculate gain = TARGET_PEAK / max_value
  *         3. Limit gain to MAX_GAIN to prevent over-amplification
  *         4. Apply gain to all samples
  *
  * @note   This ensures consistent output volume regardless of input level
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void normalize_audio(float32_t* input, float32_t* output, uint32_t num_samples)
{
    float32_t max_abs;
    uint32_t max_index;

    // Step 1: Compute absolute values
    arm_abs_f32(input, temp_buffer, num_samples);

    // Step 2: Find maximum value
    arm_max_f32(temp_buffer, num_samples, &max_abs, &max_index);

    // Step 3: Calculate gain factor
    float32_t gain = 1.0f;  // Default: no change

    if (max_abs > MIN_SIGNAL)  // Only process if signal is above noise floor
    {
        // Target gain = TARGET_PEAK / current_peak
        // Clamp to MAX_GAIN to prevent excessive amplification of very quiet signals
        gain = fminf(TARGET_PEAK / max_abs, MAX_GAIN);
    }

    // Step 4: Apply gain to entire block
    arm_scale_f32(input, gain, output, num_samples);
}

/**
  ******************************************************************************
  * @brief  Send processed audio data to PC via USB CDC
  * @param  data: Float audio samples to transmit
  * @param  num_samples: Number of samples
  * @retval None
  *
  * @note   Packet format: [SOF (2 bytes)] [Samples (NUM_SAMPLES × 4 bytes)] [EOF (1 byte)]
  *         This allows PC software to visualize/analyze the processed audio
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void send_usb_packet(float32_t* data, uint32_t num_samples)
{
    // Construct packet
    usb_packet.sof = SOF_WORD;

    for (uint32_t i = 0; i < num_samples; i++)
    {
        usb_packet.samples[i] = data[i];
    }

    usb_packet.eof = EOF_BYTE;

    // Transmit via USB CDC (non-blocking)
    CDC_Transmit_HS((uint8_t*)&usb_packet, sizeof(usb_packet));
}

/**
  ******************************************************************************
  * @brief  Main audio processing pipeline
  * @retval None
  *
  * @note   Execution flow:
  *         1. Convert I2S samples to float
  *         2. Remove DC offset
  *         3. Apply normalization (AGC)
  *         4. Send to USB (optional monitoring)
  *         5. Convert back to I2S and send to DAC
  *
  * @note   Processing time: ~100-150μs on STM32H7 @ 550MHz
  ******************************************************************************
  */
__attribute__((optimize("O3")))
void process_audio_block(void)
{
    // Determine source buffer (which half-buffer was just filled)
    int32_t* i2s_source = (int32_t*)active_buffer;

    // Determine destination buffer (synchronized with source)
    // If processing first half of RX buffer → write to first half of TX buffer
    // If processing second half of RX buffer → write to second half of TX buffer
    int32_t* i2s_dest = (active_buffer == i2s_rx_buffer) ?
                         i2s_tx_buffer :
                         &i2s_tx_buffer[I2S_BUFFER_SIZE / 2];

    // ========================================================================
    // STAGE 1: INPUT CONVERSION AND DC REMOVAL
    // ========================================================================

    // Convert from 24-bit I2S format to float
    i2s_to_float(i2s_source, input_buffer, NUM_SAMPLES);

    // Remove DC offset for clean signal
    remove_dc_offset(input_buffer, NUM_SAMPLES);

    // ========================================================================
    // STAGE 2: NORMALIZATION (Automatic Gain Control)
    // ========================================================================

    // Apply intelligent gain to maximize signal level without clipping
    normalize_audio(input_buffer, output_buffer, NUM_SAMPLES);

    // ========================================================================
    // STAGE 3: USB TRANSMISSION (Optional - for monitoring)
    // ========================================================================

    // Uncomment to stream processed audio to PC
    // send_usb_packet(output_buffer, NUM_SAMPLES);

    // ========================================================================
    // STAGE 4: DAC OUTPUT
    // ========================================================================

    // Convert processed audio back to I2S format and write to TX buffer
    // DMA will automatically transmit this data to the DAC module
    float_to_i2s_stereo(output_buffer, i2s_dest, NUM_SAMPLES);

    // Release buffer for next DMA interrupt
    active_buffer = NULL;
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_I2S1_Init();
  MX_USB_DEVICE_Init();
  MX_I2S3_Init();
  /* USER CODE BEGIN 2 */

  // Initialize audio processing subsystem
  audio_init();

  // =========================================================================
  // START MICROPHONE INPUT (RX DMA)
  // =========================================================================
  // Configure I2S1 to continuously receive audio from microphone via DMA
  // Using circular buffer mode with half-complete and full-complete interrupts

  if (HAL_I2S_Receive_DMA(&hi2s1, (uint16_t*)i2s_rx_buffer, I2S_BUFFER_SIZE) != HAL_OK)
  {
      Error_Handler();
  }

  // =========================================================================
  // START DAC OUTPUT (TX DMA)
  // =========================================================================
  // Configure I2S3 to continuously transmit audio to DAC module via DMA
  // Start with silent buffer; process_audio_block() will fill it with processed audio

  memset(i2s_tx_buffer, 0, sizeof(i2s_tx_buffer));  // Initialize with silence

  if (HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t*)i2s_tx_buffer, I2S_BUFFER_SIZE) != HAL_OK)
  {
      Error_Handler();
  }

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_YELLOW);
  BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // =========================================================================
  // MAIN PROCESSING LOOP
  // =========================================================================
  // Simple polling loop - all audio processing triggered by DMA interrupts

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Check if DMA has flagged a buffer half as ready for processing
    if (active_buffer != NULL)
    {
        // Process audio block and output to DAC
        process_audio_block();
    }

    // Note: In production, could add LED indicators, USB status, etc. here
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 68;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 6144;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2S1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S1_Init(void)
{

  /* USER CODE BEGIN I2S1_Init 0 */

  /* USER CODE END I2S1_Init 0 */

  /* USER CODE BEGIN I2S1_Init 1 */

  /* USER CODE END I2S1_Init 1 */
  hi2s1.Instance = SPI1;
  hi2s1.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s1.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s1.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s1.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s1.Init.CPOL = I2S_CPOL_LOW;
  hi2s1.Init.FirstBit = I2S_FIRSTBIT_MSB;
  hi2s1.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
  hi2s1.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_LEFT;
  hi2s1.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
  if (HAL_I2S_Init(&hi2s1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S1_Init 2 */

  /* USER CODE END I2S1_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.FirstBit = I2S_FIRSTBIT_MSB;
  hi2s3.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
  hi2s3.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_LEFT;
  hi2s3.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_14, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PD8 PD9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PE1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// ============================================================================
// DMA INTERRUPT CALLBACKS
// ============================================================================
// These are called automatically by HAL when DMA completes half or full buffer

/**
  ******************************************************************************
  * @brief  DMA half-transfer complete callback
  * @param  hi2s: I2S handle
  * @retval None
  *
  * @note   Called when first half of RX circular buffer is filled
  *         Signals main loop to process first half while DMA fills second half
  ******************************************************************************
  */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // Only respond to microphone (RX) interrupts
    // DAC (TX) runs automatically, no intervention needed
    if (hi2s->Instance == SPI1)
    {
        if (active_buffer == NULL)
        {
            active_buffer = i2s_rx_buffer;  // Point to first half of buffer
        }
    }
}

/**
  ******************************************************************************
  * @brief  DMA full-transfer complete callback
  * @param  hi2s: I2S handle
  * @retval None
  *
  * @note   Called when second half of RX circular buffer is filled
  *         Signals main loop to process second half while DMA wraps to first half
  ******************************************************************************
  */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI1)
    {
        if (active_buffer == NULL)
        {
            // Point to second half of buffer
            active_buffer = &i2s_rx_buffer[I2S_BUFFER_SIZE / 2];
        }
    }
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
    // Halt execution - investigate with debugger
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
