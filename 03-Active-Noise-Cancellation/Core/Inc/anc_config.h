/*
 * anc_config.h
 * Description: Temel sistem ve algoritma ayarları
 */

#ifndef ANC_CONFIG_H_
#define ANC_CONFIG_H_

// --- KRİTİK KÜTÜPHANELER ---
#include <stdint.h>
#include "arm_math.h"
#include <math.h>
#include <stdlib.h>

// ==========================================
// 1. TEMEL SİSTEM AYARLARI
// ==========================================
#define SAMPLING_RATE           48000
// [KRİTİK DÜZELTME] 64 Sample gecikmeyi artırır.
// ANC için hız hayattır. 32'ye geri çekiyoruz.
#define NUM_SAMPLES             32
#define BLOCK_SIZE              NUM_SAMPLES
#define I2S_BUFFER_SIZE         (NUM_SAMPLES * 2 * 2)

// ==========================================
// 2. MATEMATİKSEL SABİTLER
// ==========================================
#define INV_MAX_24BIT           1.1920928955078125e-7f

// ==========================================
// 3. DSP VE SES İŞLEME AYARLARI
// ==========================================
#define DC_ALPHA                0.9995f
#define TARGET_PEAK             0.9f
#define MAX_GAIN                20.0f
#define MIN_SIGNAL              0.00001f

// ==========================================
// 4. İSTİHBARAT (FFT) AYARLARI
// ==========================================
#define FFT_LENGTH              4096

// Eşik: Çok düşük olursa dip gürültüyü gürültü sanar.
// 0.001f ideal bir "fısıltı" seviyesidir.
#define SILENCE_THRESH          0.001f

// ==========================================
// 5. SENTEZLEYİCİ AYARLARI
// ==========================================
#define NUM_HARMONICS           3
#define TEST_VOLUME             0.1f

// ==========================================
// 6. İKİNCİL YOL (SECONDARY PATH) AYARLARI
// ==========================================
#define SEC_PATH_LENGTH         64
// Kalibrasyon adımı: Çok küçük olursa öğrenemez.
#define SEC_PATH_STEP           0.02f
// Kalibrasyon sesi: Duyulabilir olmalı ki mikrofon algılasın.
#define CALIB_NOISE_VOL         0.25f

// [KRİTİK] GECİKME YÖNETİMİ
// 32 Sample buffer ile yaklaşık 2 blok (64) gecikme olur.
#define SYSTEM_LATENCY          64
#define HISTORY_SIZE            256
#define HISTORY_MASK            (HISTORY_SIZE - 1)

// Durumlar
#define ANC_STATE_IDLE          0
#define ANC_STATE_CALIBRATION   1
#define ANC_STATE_RUNNING       2

// [MANTIKLI KAZANÇ]
// 0.1 çok azdı (uyudu), 3.0 çok fazlaydı (patladı).
// 2.0f tam kararında bir başlangıçtır.
#define MIC_SOFTWARE_GAIN       4.0f

// ==========================================
// 8. ANC (FxLMS) MOTOR AYARLARI
// ==========================================
#define ANC_FILTER_LENGTH       32

// [MANTIKLI HIZ]
// 0.000001 çok yavaştı.
// Gain 2.0 iken 0.00001 (1e-5) standarttır.
#define ANC_STEP_SIZE           0.00001f

// Sızıntı (Leakage): 0.999 biraz gevşek olabilir,
// 0.995 filtreyi daha iyi dizginler.
#define ANC_LEAKAGE             0.995f

#endif /* ANC_CONFIG_H_ */
