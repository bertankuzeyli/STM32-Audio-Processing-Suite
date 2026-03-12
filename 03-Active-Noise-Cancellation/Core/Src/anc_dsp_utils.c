/*
 * anc_dsp_utils.c
 */

#include "anc_dsp_utils.h"

// --- Özel Değişkenler (Sadece bu dosya kullanır) ---
static float32_t dc_offset = 0.0f;
static float32_t temp_buffer[BLOCK_SIZE] __attribute__((aligned(4))); // Normalizasyon için geçici alan

// --- Başlatma Fonksiyonu ---
void ANC_DSP_Init(void)
{
    dc_offset = 0.0f;
    // Temp buffer'ı temizlemeye gerek yok, her tur üzerine yazılıyor.
}

// --- 1. I2S (Int32) -> Float Dönüşümü ---
__attribute__((optimize("O3")))
void i2s_to_float(int32_t* i2s_data, float32_t* float_data, uint32_t num_samples)
{
    for (uint32_t i = 0; i < num_samples; i++)
    {
        int32_t raw = i2s_data[i * 2]; // Sol kanal
        raw = raw >> 8;                // 24-bit hizalama
        float_data[i] = (float32_t)raw * INV_MAX_24BIT;
    }
}

// --- 2. Float -> I2S (Stereo) Dönüşümü ---
__attribute__((optimize("O3")))
void float_to_i2s_stereo(float32_t* float_data, int32_t* i2s_data, uint32_t num_samples)
{
    const float32_t SCALE_FACTOR = 8388608.0f;

    for (uint32_t i = 0; i < num_samples; i++)
    {
        int32_t val = (int32_t)(float_data[i] * SCALE_FACTOR);
        int32_t formatted_sample = val << 8;

        // Mono sesi iki kanala kopyala
        i2s_data[i * 2]     = formatted_sample;
        i2s_data[i * 2 + 1] = formatted_sample;
    }
}

// --- 3. DC Offset Temizleme ---
__attribute__((optimize("O3")))
void remove_dc_offset(float32_t* data, uint32_t num_samples)
{
    float32_t block_mean;
    arm_mean_f32(data, num_samples, &block_mean);
    dc_offset = DC_ALPHA * dc_offset + (1.0f - DC_ALPHA) * block_mean;
    arm_offset_f32(data, -dc_offset, data, num_samples);
}

// --- 4. Normalizasyon (Ses Yükseltme) ---
__attribute__((optimize("O3")))
void normalize_audio(float32_t* input, float32_t* output, uint32_t num_samples)
{
    float32_t max_abs;
    uint32_t max_index;

    arm_abs_f32(input, temp_buffer, num_samples);
    arm_max_f32(temp_buffer, num_samples, &max_abs, &max_index);
    float32_t gain = 1.0f;

    if (max_abs > MIN_SIGNAL)
    {
        gain = fminf(TARGET_PEAK / max_abs, MAX_GAIN);
    }
    arm_scale_f32(input, gain, output, num_samples);
}
