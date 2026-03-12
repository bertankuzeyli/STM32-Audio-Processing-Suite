/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Real-time FFT with ITM Live Variables
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <math.h>
#include "arm_math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define I2S_BUFFER_SIZE     512
#define FFT_SIZE            256
#define NUM_BINS            (FFT_SIZE / 2)

#define SAMPLE_RATE         32000.0f
#define FREQ_RESOLUTION     (SAMPLE_RATE / FFT_SIZE)

#define INV_MAX_24BIT       1.1920928955078125e-7f
#define HPF_ALPHA           0.996f

// Frequency bands (in bins)
#define BASS_END       8   // 0-1000 Hz
#define MID_END        32  // 1000-4000 Hz
#define HIGH_END       80  // 4000-10000 Hz

// ITM Stimulus Ports for graphing
#define ITM_PORT_BASS       1
#define ITM_PORT_MID        2
#define ITM_PORT_HIGH       3
#define ITM_PORT_PEAK       4
#define ITM_PORT_RMS        5
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

I2S_HandleTypeDef hi2s1;
DMA_HandleTypeDef hdma_spi1_rx;

/* USER CODE BEGIN PV */
// I2S DMA buffers
int32_t i2s_rx_buffer[I2S_BUFFER_SIZE] __attribute__((aligned(32)));
volatile int32_t* volatile active_buffer = NULL;

// FFT working buffers
float32_t fft_input[FFT_SIZE * 2] __attribute__((aligned(4)));
float32_t fft_output[FFT_SIZE] __attribute__((aligned(4)));

// CMSIS DSP FFT instance
arm_rfft_fast_instance_f32 fft_instance;

// HPF state
static float hpf_prev_in = 0.0f;
static float hpf_prev_out = 0.0f;

// *** LIVE VARIABLES FOR ITM GRAPHING ***
// Add these to "Live Expressions" window in STM32CubeIDE
volatile float live_bass_level = 0.0f;      // 0-1000 Hz (0-100 scale)
volatile float live_mid_level = 0.0f;       // 1000-4000 Hz (0-100 scale)
volatile float live_high_level = 0.0f;      // 4000-10000 Hz (0-100 scale)
volatile float live_peak_level = 0.0f;      // Peak amplitude (0-100 scale)
volatile float live_rms_level = 0.0f;       // RMS level (0-100 scale)
volatile float live_total_energy = 0.0f;    // Total spectrum energy

// DEBUG: Raw signal monitoring
volatile float live_raw_sample = 0.0f;      // Raw audio sample for debugging
volatile uint32_t live_fft_counter = 0;     // FFT execution counter

// Smoothing buffers for stable display
static float bass_smooth = 0.0f;
static float mid_smooth = 0.0f;
static float high_smooth = 0.0f;
static float peak_smooth = 0.0f;
static float rms_smooth = 0.0f;

// Update counter for reduced ITM bandwidth
static uint32_t update_counter = 0;

// Individual frequency bins for detailed analysis
volatile float live_bin_100hz = 0.0f;
volatile float live_bin_500hz = 0.0f;
volatile float live_bin_1khz = 0.0f;
volatile float live_bin_2khz = 0.0f;
volatile float live_bin_4khz = 0.0f;
volatile float live_bin_8khz = 0.0f;

// Spectrum bands array
float spectrum_bands[NUM_BINS];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S1_Init(void);
/* USER CODE BEGIN PFP */
static inline float remove_dc_offset(float input);
static void process_fft_and_update(void);
static void send_itm_data(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  High-pass filter for DC offset removal
  */
static inline float remove_dc_offset(float input)
{
    hpf_prev_out = HPF_ALPHA * (hpf_prev_out + input - hpf_prev_in);
    hpf_prev_in = input;
    return hpf_prev_out;
}

/**
  * @brief  Apply Hamming window
  */
static inline float hamming_window(int n, int N)
{
    return 0.54f - 0.46f * arm_cos_f32(2.0f * PI * n / (N - 1));
}

/**
  * @brief  Send data to ITM ports for Time Graph
  */
static void send_itm_data(void)
{
    // Only send every 8th frame to reduce bandwidth
    if (++update_counter < 8) return;
    update_counter = 0;

    // ITM Data Trace - Write 32-bit values to stimulus ports
    // Scale to 0-100 range for better visualization

    uint32_t bass_out = (uint32_t)(live_bass_level);
    uint32_t mid_out = (uint32_t)(live_mid_level);
    uint32_t high_out = (uint32_t)(live_high_level);
    uint32_t peak_out = (uint32_t)(live_peak_level);
    uint32_t rms_out = (uint32_t)(live_rms_level);

    // Write to ITM stimulus ports (32-bit write)
    if (ITM->PORT[ITM_PORT_BASS].u32 == 0)
        ITM->PORT[ITM_PORT_BASS].u32 = bass_out;

    if (ITM->PORT[ITM_PORT_MID].u32 == 0)
        ITM->PORT[ITM_PORT_MID].u32 = mid_out;

    if (ITM->PORT[ITM_PORT_HIGH].u32 == 0)
        ITM->PORT[ITM_PORT_HIGH].u32 = high_out;

    if (ITM->PORT[ITM_PORT_PEAK].u32 == 0)
        ITM->PORT[ITM_PORT_PEAK].u32 = peak_out;

    if (ITM->PORT[ITM_PORT_RMS].u32 == 0)
        ITM->PORT[ITM_PORT_RMS].u32 = rms_out;
}

/**
  * @brief  Process FFT and update live variables
  */
__attribute__((optimize("O3")))
static void process_fft_and_update(void)
{
    int32_t* src = (int32_t*)active_buffer;

    live_fft_counter++;  // Track FFT executions

    // Fill FFT input buffer with windowed samples
    float peak_sample = 0.0f;
    float rms_sum = 0.0f;

    for (int i = 0; i < FFT_SIZE; i++)
    {
        int32_t sample_raw = src[i * 2] >> 8;
        float normalized = (float)sample_raw * INV_MAX_24BIT;

        // Store raw sample for debugging (first sample only)
        if (i == 0) live_raw_sample = normalized;

        float filtered = remove_dc_offset(normalized);

        // Track peak and RMS
        float abs_val = fabsf(filtered);
        if (abs_val > peak_sample) peak_sample = abs_val;
        rms_sum += filtered * filtered;

        // Apply window and store
        fft_input[i] = filtered * hamming_window(i, FFT_SIZE);
    }

    // Calculate RMS and Peak
    float rms_raw = sqrtf(rms_sum / FFT_SIZE);

    // More aggressive scaling for better visualization
    // Original values are very small (0.0-0.01 range typically)
    // Multiply by large factor to get 0-100 range
    float scale_factor = 5000.0f;  // Adjust based on your signal level

    rms_smooth = 0.7f * rms_smooth + 0.3f * (rms_raw * scale_factor);
    peak_smooth = 0.8f * peak_smooth + 0.2f * (peak_sample * scale_factor);

    // Perform Real FFT
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);

    // Calculate magnitude spectrum
    arm_cmplx_mag_f32(fft_output, spectrum_bands, NUM_BINS);

    // Calculate frequency band levels
    float bass_sum = 0.0f;
    float mid_sum = 0.0f;
    float high_sum = 0.0f;

    // Bass (0-1000 Hz) - bins 1-8
    for (int i = 1; i < BASS_END; i++) {
        bass_sum += spectrum_bands[i];
    }
    float bass_avg = bass_sum / (BASS_END - 1);

    // Mid (1000-4000 Hz) - bins 8-32
    for (int i = BASS_END; i < MID_END; i++) {
        mid_sum += spectrum_bands[i];
    }
    float mid_avg = mid_sum / (MID_END - BASS_END);

    // High (4000-10000 Hz) - bins 32-80
    for (int i = MID_END; i < HIGH_END; i++) {
        high_sum += spectrum_bands[i];
    }
    float high_avg = high_sum / (HIGH_END - MID_END);

    // Scale FFT magnitudes (FFT output is also very small)
    bass_avg *= scale_factor;
    mid_avg *= scale_factor;
    high_avg *= scale_factor;

    // Apply exponential smoothing
    const float alpha = 0.5f;  // More responsive
    bass_smooth = alpha * bass_avg + (1.0f - alpha) * bass_smooth;
    mid_smooth = alpha * mid_avg + (1.0f - alpha) * mid_smooth;
    high_smooth = alpha * high_avg + (1.0f - alpha) * high_smooth;

    // Direct assignment (already scaled)
    live_bass_level = bass_smooth;
    live_mid_level = mid_smooth;
    live_high_level = high_smooth;
    live_rms_level = rms_smooth;
    live_peak_level = peak_smooth;

    // Clamp to 0-100 range
    if (live_bass_level < 0.0f) live_bass_level = 0.0f;
    if (live_bass_level > 100.0f) live_bass_level = 100.0f;
    if (live_mid_level < 0.0f) live_mid_level = 0.0f;
    if (live_mid_level > 100.0f) live_mid_level = 100.0f;
    if (live_high_level < 0.0f) live_high_level = 0.0f;
    if (live_high_level > 100.0f) live_high_level = 100.0f;
    if (live_rms_level < 0.0f) live_rms_level = 0.0f;
    if (live_rms_level > 100.0f) live_rms_level = 100.0f;
    if (live_peak_level < 0.0f) live_peak_level = 0.0f;
    if (live_peak_level > 100.0f) live_peak_level = 100.0f;

    // Total energy
    live_total_energy = bass_smooth + mid_smooth + high_smooth;

    // Extract specific frequency bins
    live_bin_100hz = spectrum_bands[1] * scale_factor;
    live_bin_500hz = spectrum_bands[4] * scale_factor;
    live_bin_1khz = spectrum_bands[8] * scale_factor;
    live_bin_2khz = spectrum_bands[16] * scale_factor;
    live_bin_4khz = spectrum_bands[32] * scale_factor;
    live_bin_8khz = spectrum_bands[64] * scale_factor;

    // Send data to ITM for Time Graph
    send_itm_data();

    // Clear flag
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
  /* USER CODE BEGIN 2 */

  // Initialize FFT
  arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

  // Start I2S DMA
  if(HAL_I2S_Receive_DMA(&hi2s1, (uint16_t*)i2s_rx_buffer, I2S_BUFFER_SIZE) != HAL_OK)
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
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (active_buffer != NULL)
    {
        process_fft_and_update();
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
  hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_32K;
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

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (active_buffer == NULL)
  {
    active_buffer = i2s_rx_buffer;
  }
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (active_buffer == NULL)
  {
    active_buffer = &i2s_rx_buffer[I2S_BUFFER_SIZE >> 1];
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
