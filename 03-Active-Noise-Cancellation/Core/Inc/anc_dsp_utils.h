/*
 * anc_dsp_utils.h
 * Description: Yardımcı DSP araçları (Dönüşüm, Normalizasyon vb.)
 */

#ifndef ANC_DSP_UTILS_H_
#define ANC_DSP_UTILS_H_

#include "anc_config.h"

// --- FONKSİYON PROTOTİPLERİ ---
void ANC_DSP_Init(void);
void i2s_to_float(int32_t* i2s_data, float32_t* float_data, uint32_t num_samples);
void float_to_i2s_stereo(float32_t* float_data, int32_t* i2s_data, uint32_t num_samples);
void remove_dc_offset(float32_t* data, uint32_t num_samples);
void normalize_audio(float32_t* input, float32_t* output, uint32_t num_samples);

#endif /* ANC_DSP_UTILS_H_ */
