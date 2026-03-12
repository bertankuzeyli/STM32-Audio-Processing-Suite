/*
 * anc_analysis.h
 * Description: FFT ve Frekans Tespiti İşlemleri
 */

#ifndef ANC_ANALYSIS_H_
#define ANC_ANALYSIS_H_

#include "anc_config.h"

// --- PAYLAŞILAN DEĞİŞKENLER (Extern) ---
// Bu değişkenler anc_analysis.c içinde tanımlı ama Core (Motor) da bunları görüp kullanmalı.

extern float32_t fft_input_buffer[FFT_LENGTH];  // Motor burayı dolduracak
extern uint32_t fft_buffer_index;               // Doldurma sayacı
extern volatile uint8_t is_fft_ready;           // Bayrak

extern float32_t target_freq;                   // Sonuç (Motor bunu okuyup ses üretecek)
extern volatile float32_t debug_detected_freq;  // Debug için

// --- FONKSİYON PROTOTİPLERİ ---
void ANC_Analysis_Init(void);      // FFT motorunu hazırlar
void Find_Dominant_Freq(void);     // Hesaplamayı yapar

#endif /* ANC_ANALYSIS_H_ */
