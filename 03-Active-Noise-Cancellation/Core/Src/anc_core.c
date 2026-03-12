/*
 * anc_core.c - FxLMS FINAL ENGINE
 * Revizyon: Latency Compensation, Fast RNG, Debug Report & SAFETY CLAMPING
 */

#include "anc_core.h"
#include "main.h"   // HAL_GetTick() için gereklidir
#include <stdio.h>

// --- BUFFERLAR ---
int32_t i2s_rx_buffer[I2S_BUFFER_SIZE] __attribute__((aligned(32)));
int32_t i2s_tx_buffer[I2S_BUFFER_SIZE] __attribute__((aligned(32)));
volatile int32_t* volatile active_buffer __attribute__((section(".dtcmram"), aligned(4))) = NULL;

static float32_t input_buffer[BLOCK_SIZE]  __attribute__((aligned(4)));
static float32_t output_buffer[BLOCK_SIZE] __attribute__((aligned(4)));

// --- ZAMANLAMA ---
static uint64_t global_sample_counter = 0;
#define WARMUP_SAMPLES      48000
#define CALIB_DURATION      128000

// --- DURUM & RAPORLAMA ---
static uint8_t anc_state = ANC_STATE_IDLE;
volatile uint8_t print_report_req = 0;

// Raporlama hafızası
static uint8_t prev_anc_state = 255;
static uint32_t last_report_time = 0;

// --- GÜNCELLENMİŞ BUFFERLAR (HISTORY_SIZE = 256) ---
float32_t sec_path_coeffs[SEC_PATH_LENGTH];
float32_t white_noise_history[HISTORY_SIZE];
uint32_t wn_index = 0;

float32_t anc_coeffs[ANC_FILTER_LENGTH];
float32_t x_history[HISTORY_SIZE];
float32_t x_prime_history[HISTORY_SIZE];
uint32_t hist_index = 0;

// --- SENTEZLEYİCİ ---
static float32_t synth_angle = 0.0f;
static float32_t current_freq = 0.0f;
static float32_t current_volume = 0.0f;

// --- XORSHIFT RNG ---
static uint32_t rng_state = 2463534242;

static inline uint32_t xorshift32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

// --- YARDIMCI MAKRO ---
#define GET_HIST(buff, current_idx, delay)  (buff[(current_idx - delay) & HISTORY_MASK])


// --- BAŞLATMA ---
void ANC_Core_Init(void)
{
    active_buffer = NULL;
    anc_state = ANC_STATE_IDLE;
    global_sample_counter = 0;
    print_report_req = 0;

    prev_anc_state = 255;
    last_report_time = 0;

    // Temizlik
    for(int i=0; i<SEC_PATH_LENGTH; i++) sec_path_coeffs[i] = 0.0f;
    for(int i=0; i<ANC_FILTER_LENGTH; i++) anc_coeffs[i] = 0.0f;

    for(int i=0; i<HISTORY_SIZE; i++) {
        white_noise_history[i] = 0.0f;
        x_history[i] = 0.0f;
        x_prime_history[i] = 0.0f;
    }

    wn_index = 0;
    hist_index = 0;

    rng_state = 2463534242;
}

// --- CYCLE HANDLER & RAPORLAMA ---
void ANC_Cycle_Handler(void)
{
    if (active_buffer != NULL) process_audio_block();

    if (anc_state == ANC_STATE_RUNNING && is_fft_ready == 1) {
        Find_Dominant_Freq();
        is_fft_ready = 0;
    }

    // A) Durum Değişikliği Tespiti
    if (anc_state != prev_anc_state)
    {
        switch (anc_state)
        {
            case ANC_STATE_IDLE:
                printf("\n>>> DURUM: IDLE (Isinma/Bekleme) <<<\n");
                break;
            case ANC_STATE_CALIBRATION:
                printf("\n>>> DURUM: CALIBRATION (Kalibrasyon Basladi) <<<\n");
                printf("--- Hoparlorden hisirti gelmeli ---\n");
                break;
            case ANC_STATE_RUNNING:
                printf("\n>>> DURUM: RUNNING (ANC AKTIF!) <<<\n");
                printf("--- Gurultu Avi Basladi ---\n");
                break;
        }
        prev_anc_state = anc_state;
    }

    // B) Periyodik Canlılık Raporu
    if (HAL_GetTick() - last_report_time > 1000)
    {
        if (anc_state == ANC_STATE_RUNNING)
        {
            // Burası filtre durumunu gösterir.
            // Gain[0] artık asla milyar trilyon olmayacak.
            printf("[ANC] Freq: %4.0f Hz | Vol: %.2f | Gain[0]: %.5f\n",
                   current_freq, current_volume, anc_coeffs[0]);
        }
        else if (anc_state == ANC_STATE_CALIBRATION)
        {
            printf("[CALIB] Ogreniyor... Gecen Sample: %lu\n", (uint32_t)global_sample_counter);
        }

        last_report_time = HAL_GetTick();
    }
}

// --- ANA MOTOR (ZIRHLI VERSİYON) ---
__attribute__((optimize("O3")))
void process_audio_block(void)
{
    int32_t* i2s_source = (int32_t*)active_buffer;
    int32_t* i2s_dest = (active_buffer == i2s_rx_buffer) ? i2s_tx_buffer : &i2s_tx_buffer[I2S_BUFFER_SIZE / 2];

    i2s_to_float(i2s_source, input_buffer, NUM_SAMPLES);
    remove_dc_offset(input_buffer, NUM_SAMPLES);

    // Mikrofon kazancı
    arm_scale_f32(input_buffer, MIC_SOFTWARE_GAIN, input_buffer, NUM_SAMPLES);

    global_sample_counter += NUM_SAMPLES;

    // --- AŞAMA 1 & 2: KALİBRASYON ---
    if (global_sample_counter < (WARMUP_SAMPLES + CALIB_DURATION))
    {
        if(global_sample_counter < WARMUP_SAMPLES) {
             anc_state = ANC_STATE_IDLE;
             for(int i=0; i<NUM_SAMPLES; i++) output_buffer[i] = 0.0f;
        }
        else {
             anc_state = ANC_STATE_CALIBRATION;

             for(int i=0; i < NUM_SAMPLES; i++) {
                // 1. Gürültü Üret
                float32_t wn = ((float32_t)(xorshift32() >> 1) / 1073741824.0f) - 1.0f;
                wn *= CALIB_NOISE_VOL;

                wn_index = (wn_index + 1) & HISTORY_MASK;
                white_noise_history[wn_index] = wn;

                // 2. Tahmini Çıkış
                float32_t y_hat = 0.0f;
                for(int k=0; k < SEC_PATH_LENGTH; k++) {
                    y_hat += sec_path_coeffs[k] * GET_HIST(white_noise_history, wn_index, k);
                }

                float32_t error = input_buffer[i] - y_hat;

                // 3. LMS (Kalibrasyon) - Gecikmeli
                for(int k=0; k < SEC_PATH_LENGTH; k++) {
                    float32_t delayed_wn = GET_HIST(white_noise_history, wn_index, (SYSTEM_LATENCY + k));
                    sec_path_coeffs[k] += SEC_PATH_STEP * error * delayed_wn;
                }

                output_buffer[i] = wn;
             }
        }
    }
    // --- AŞAMA 3: GERÇEK ANC (ZIRHLI MOD) ---
    else
    {
        if(anc_state == ANC_STATE_CALIBRATION) print_report_req = 1;
        anc_state = ANC_STATE_RUNNING;

        // Hedef frekansa kilitlen
        if (target_freq > 20.0f) current_freq = (0.98f * current_freq) + (0.02f * target_freq);
        else current_freq *= 0.98f;

        float32_t angle_step = (2.0f * PI * current_freq) / (float32_t)SAMPLING_RATE;
        float32_t target_vol = (target_freq > 20.0f) ? 1.0f : 0.0f;
        current_volume = (0.9f * current_volume) + (0.1f * target_vol);

        for(int i=0; i < NUM_SAMPLES; i++)
        {
            // 1. Referans (x)
            float32_t x_curr = 0.0f;
            if(current_volume > 0.01f) {
                x_curr = arm_sin_f32(synth_angle) * current_volume;
                synth_angle += angle_step;
                if(synth_angle > 2.0f*PI) synth_angle -= 2.0f*PI;
            }

            hist_index = (hist_index + 1) & HISTORY_MASK;
            x_history[hist_index] = x_curr;

            // 2. Filtrelenmiş Referans (x')
            float32_t x_prime = 0.0f;
            for(int k=0; k < SEC_PATH_LENGTH; k++) {
                x_prime += sec_path_coeffs[k] * GET_HIST(x_history, hist_index, k);
            }
            x_prime_history[hist_index] = x_prime;

            // 3. Anti-Noise (y)
            float32_t y_out = 0.0f;
            for(int k=0; k < ANC_FILTER_LENGTH; k++) {
                y_out += anc_coeffs[k] * GET_HIST(x_history, hist_index, k);
            }

            // 4. Hata (e)
            float32_t error = input_buffer[i];

            // 5. [GÜVENLİ] LMS GÜNCELLEMESİ
            if(current_volume > 0.01f) {
                for(int k=0; k < ANC_FILTER_LENGTH; k++) {
                    float32_t delayed_x_prime = GET_HIST(x_prime_history, hist_index, (SYSTEM_LATENCY + k));

                    // Güncelleme
                    anc_coeffs[k] += ANC_STEP_SIZE * error * delayed_x_prime;

                    // Sızıntı
                    anc_coeffs[k] *= ANC_LEAKAGE;

                    // [YENİ] EMNİYET KEMERİ (CLAMPING)
                    // Filtre patlamasını kesin olarak engeller
                    if (anc_coeffs[k] > 2.0f) anc_coeffs[k] = 2.0f;
                    else if (anc_coeffs[k] < -2.0f) anc_coeffs[k] = -2.0f;
                }
            }

            // Saturation
            if (y_out > 1.0f) y_out = 1.0f;
            if (y_out < -1.0f) y_out = -1.0f;

            //BU ALTTAKI ÇARPANLA OYNAYABİLİRSİN HOPARLORÜN SES SEVİYESİ İLE İLGİLİ
            output_buffer[i] = y_out * 2.00003153521f;
        }
    }

    float_to_i2s_stereo(output_buffer, i2s_dest, NUM_SAMPLES);

    // FFT BESLEME
    if (is_fft_ready == 0) {
        for(uint32_t i=0; i<NUM_SAMPLES; i++) {
            if (fft_buffer_index < FFT_LENGTH) {
                fft_input_buffer[fft_buffer_index] = input_buffer[i];
                fft_buffer_index++;
            }
        }
        if (fft_buffer_index >= FFT_LENGTH) {
            is_fft_ready = 1;
            fft_buffer_index = 0;
        }
    }
    active_buffer = NULL;
}
