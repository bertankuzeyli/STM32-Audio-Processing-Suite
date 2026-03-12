/*
 * anc_core.h
 * Description: Ana ANC Motoru (FxLMS ve State Machine)
 */

#ifndef ANC_CORE_H_
#define ANC_CORE_H_

#include "anc_config.h"     // Ayarları gör
#include "anc_dsp_utils.h"  // Araçları gör
#include "anc_analysis.h"   // FFT sonuçlarını gör (target_freq vb.)

// --- PAYLAŞILAN BUFFERLAR (Main.c DMA için bunları ister) ---
extern int32_t i2s_rx_buffer[I2S_BUFFER_SIZE];
extern int32_t i2s_tx_buffer[I2S_BUFFER_SIZE];
extern volatile int32_t* volatile active_buffer;

// --- FONKSİYON PROTOTİPLERİ ---
void ANC_Core_Init(void);
void ANC_Cycle_Handler(void);
void process_audio_block(void);

#endif /* ANC_CORE_H_ */
