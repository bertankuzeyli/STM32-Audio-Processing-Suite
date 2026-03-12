# Active Noise Cancellation (FxLMS) 🔇

A real-time adaptive noise cancellation system for STM32H7 microcontrollers using the Filtered-x Least Mean Squares (FxLMS) algorithm, with automatic secondary path calibration and FFT-based dominant frequency locking.

---

## 🎯 Project Description

This project implements a complete active noise cancellation pipeline that captures environmental noise from an I2S MEMS microphone, identifies the dominant noise frequency in real time, and generates an anti-noise signal through a loudspeaker to cancel it at the error microphone.

### What It Does:
- 🎤 Captures 24-bit audio at 48kHz from an I2S MEMS microphone
- 🔍 Detects dominant noise frequency via FFT analysis
- 🧠 Estimates the speaker-to-microphone secondary path using white noise injection
- 🔇 Generates anti-noise with an adaptive FxLMS filter that converges in real time
- 🛡️ Clamps filter coefficients to prevent numerical instability
- 📡 Reports system state and filter performance via SWV/ITM printf

---

## 🏗️ System Architecture

```
┌─────────────┐     I2S      ┌──────────────┐     DMA      ┌──────────────┐
│  I2S MEMS   │──────────────▶│  STM32H7    │◀─────────────│  Ping-Pong   │
│ Microphone  │  24-bit/48kHz │  I2S1 RX    │   Circular   │  32 samples  │
└─────────────┘               └──────────────┘              └──────────────┘
                                      │
                                      ▼
                               ┌─────────────┐
                               │  DC Removal │
                               │  + MIC Gain │
                               └─────────────┘
                                      │
               ┌──────────────────────┼──────────────────────┐
               ▼                      ▼                       ▼
     ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
     │  PHASE 1: IDLE   │   │  PHASE 2: CALIB  │   │  PHASE 3: ANC    │
     │  (Warm-up 1s)    │   │  White Noise LMS │   │  FxLMS Engine    │
     │  Silence output  │   │  SecPath Estim.  │   │  Freq Lock + AGC │
     └──────────────────┘   └──────────────────┘   └──────────────────┘
                                      │                       │
                                      │                       ▼
                                      │              ┌──────────────────┐
                                      │              │  FFT Analysis    │
                                      │              │  Find_Dominant_  │
                                      │              │  Freq()          │
                                      │              └──────────────────┘
                                      │                       │
                                      └───────────┬───────────┘
                                                  ▼
                                         ┌──────────────┐
                                         │  I2S3 TX DMA │
                                         │  DAC Output  │
                                         └──────────────┘
                                                  │
                                                  ▼
                                         ┌──────────────┐
                                         │  Loudspeaker │
                                         │  Anti-Noise  │
                                         └──────────────┘
```

---

## 🔬 Technical Specifications

### Audio Parameters:
| Parameter | Value |
|-----------|-------|
| Sample Rate | 48 kHz |
| Bit Depth | 24-bit |
| Block Size | 8 samples |
| Block Latency | ~166 μs |
| Warm-up Duration | 48,000 samples (~1 sec) |
| Calibration Duration | 128,000 samples (~2.67 sec) |

### FxLMS Filter Parameters:
| Parameter | Define | Description |
|-----------|--------|-------------|
| ANC Filter Length | `ANC_FILTER_LENGTH` | Number of adaptive taps |
| Secondary Path Length | `SEC_PATH_LENGTH` | Estimated echo filter taps |
| ANC Step Size | `ANC_STEP_SIZE` | LMS convergence rate |
| ANC Leakage | `ANC_LEAKAGE` | Coefficient decay factor |
| Calib Noise Volume | `CALIB_NOISE_VOL` | White noise injection level |
| Coefficient Clamp | ±2.0 | Hard safety limit |
| History Buffer Size | `HISTORY_SIZE` = 256 | Ring buffer depth |
| History Mask | `HISTORY_MASK` = 255 | Fast modulo indexing |

### System Timing:
| Parameter | Value |
|-----------|-------|
| System Latency | `SYSTEM_LATENCY` samples | Compensated in LMS update |
| Report Interval | 1000 ms | ITM periodic status |
| FFT Trigger | Every `FFT_LENGTH` samples | Async to ANC processing |

---

## 🧠 Algorithm Details

### FxLMS — Filtered-x Least Mean Squares

The FxLMS algorithm is the standard method for feedforward active noise control. Unlike standard LMS, it accounts for the acoustic path between the loudspeaker and the error microphone (the secondary path).

```
Reference Signal x[n]  ──▶  ANC Filter W  ──▶  y[n] (anti-noise)
                                                     │
                    x'[n] = S_hat * x[n]             ▼  (through loudspeaker)
                                          Error Mic e[n] = d[n] + S*y[n]
                                                     │
                    W[n+1] = W[n] + μ * e[n] * x'[n-Δ]
```

Where `S_hat` is the estimated secondary path and `Δ` is the system latency compensation.

### 3-Phase Operation

**Phase 1 — Idle (Warm-up)**
The system runs silently for the first 48,000 samples (~1 second) while the DC offset filter stabilizes and the microphone settles.

**Phase 2 — Calibration**
White noise is injected into the loudspeaker. The LMS algorithm uses the microphone signal as error and converges the secondary path filter `sec_path_coeffs[]` to model the acoustic transfer function between speaker and microphone.

```c
// Secondary path LMS update (with latency compensation)
for (k = 0; k < SEC_PATH_LENGTH; k++) {
    float delayed_wn = GET_HIST(white_noise_history, wn_index, SYSTEM_LATENCY + k);
    sec_path_coeffs[k] += SEC_PATH_STEP * error * delayed_wn;
}
```

**Phase 3 — Active ANC**
The FFT analysis module provides the dominant noise frequency. A sine wave synthesizer generates a reference signal locked to that frequency. The FxLMS filter produces the anti-noise output, updated every sample with leakage and safety clamping.

```c
// FxLMS coefficient update
for (k = 0; k < ANC_FILTER_LENGTH; k++) {
    float x_prime_delayed = GET_HIST(x_prime_history, hist_index, SYSTEM_LATENCY + k);
    anc_coeffs[k] += ANC_STEP_SIZE * error * x_prime_delayed;
    anc_coeffs[k] *= ANC_LEAKAGE;                              // leakage
    if (anc_coeffs[k] >  2.0f) anc_coeffs[k] =  2.0f;        // clamp
    if (anc_coeffs[k] < -2.0f) anc_coeffs[k] = -2.0f;
}
```

### XORSHIFT Random Number Generator
Fast 32-bit RNG used for calibration white noise — avoids the overhead of standard `rand()`:
```c
x ^= x << 13;
x ^= x >> 17;
x ^= x << 5;
```

### History Buffer Ring Indexing
All delay lines use a power-of-2 ring buffer with bitmask indexing for zero-branch access:
```c
#define GET_HIST(buff, current_idx, delay)  (buff[(current_idx - delay) & HISTORY_MASK])
```

---

## 🛠️ Hardware Setup

### Microcontroller:
- **Part Number**: STM32H723ZGT6
- **Core**: ARM Cortex-M7 @ 550MHz
- **FPU**: Double-precision floating-point unit
- **Flash**: 1MB
- **RAM**: 564KB (DTCM used for `active_buffer`)

### Pin Configuration:

| Peripheral | Pin | Function |
|------------|-----|----------|
| I2S1_WS | PA4 | Word Select – Mic Input |
| I2S1_CK | PA5 | Bit Clock – Mic Input |
| I2S1_SD | PA7 | Serial Data In (from Mic) |
| I2S3_WS | PA15 | Word Select – DAC Output |
| I2S3_CK | PB3 | Bit Clock – DAC Output |
| I2S3_SD | PB5 | Serial Data Out (to DAC) |
| SWO | PB3 | ITM Serial Wire Output |

### Physical Setup:
```
Noise Source
     │  (acoustic)
     ▼
┌─────────────┐   I2S    ┌─────────────┐   I2S    ┌──────────┐
│  Reference  │──────────▶│  STM32H7   │──────────▶│  Speaker │
│  Microphone │          │  FxLMS ANC  │          │(Anti-    │
└─────────────┘          └─────────────┘          │ Noise)   │
                                │                  └──────────┘
                          ┌─────▼─────┐                │ (acoustic)
                          │   Error   │◀───────────────-┘
                          │ Microphone│  (residual noise)
                          └───────────┘
```

**Recommended Components:**
- Reference Mic: INMP441, ICS-43434
- DAC / Amplifier: PCM5102A + Class-D amp, MAX98357A (integrated)

---

## 💻 Software Configuration

### Project Structure:
```
03-Active-Noise-Cancellation/
│
├── Core/Src/
│   ├── main.c              ← Hardware init, DMA start, main loop
│   └── anc_core.c          ← FxLMS engine, calibration, phase control
│
├── Core/Inc/
│   ├── anc_config.h        ← All tuning constants (step size, lengths, etc.)
│   ├── anc_core.h          ← Engine API + extern buffer declarations
│   ├── anc_dsp_utils.h     ← i2s_to_float, float_to_i2s, remove_dc_offset
│   └── anc_analysis.h      ← FFT module API (ANC_Analysis_Init, Find_Dominant_Freq)
```

### STM32CubeMX Settings:

#### I2S1 (Microphone):
```
Mode: Master Receive
Standard: Philips I2S
Data Format: 24-bit
Sample Rate: 48 kHz
MCLK Output: Disabled
DMA: Circular, Word width
```

#### I2S3 (DAC / Speaker):
```
Mode: Master Transmit
Standard: Philips I2S
Data Format: 24-bit
Sample Rate: 48 kHz
MCLK Output: Enabled
DMA: Circular, Word width
```

---

## 📡 Live Monitoring via SWV

### Setup:
```
Run → Debug Configurations → Debugger tab
✅ Enable Serial Wire Viewer (SWV)
Core Clock: 550 MHz

Window → Show View → SWV → SWV ITM Data Console
Start Trace
```

### Console Output Examples:

**State transitions printed automatically:**
```
>>> DURUM: IDLE (Isinma/Bekleme) <<<

>>> DURUM: CALIBRATION (Kalibrasyon Basladi) <<<
--- Hoparlorden hisirti gelmeli ---
[CALIB] Ogreniyor... Gecen Sample: 64000
[CALIB] Ogreniyor... Gecen Sample: 128000

>>> DURUM: RUNNING (ANC AKTIF!) <<<
--- Gurultu Avi Basladi ---
[ANC] Freq:  440 Hz | Vol: 0.98 | Gain[0]:  0.01823
[ANC] Freq:  440 Hz | Vol: 0.97 | Gain[0]: -0.02104
```

### Key Variables to Watch in Live Expressions:
| Variable | Meaning | Healthy Range |
|----------|---------|---------------|
| `anc_coeffs[0]` | First filter tap | ±0.001 – ±0.5 |
| `current_freq` | Tracked noise frequency (Hz) | 20–20000 |
| `current_volume` | Signal volume envelope | 0.0–1.0 |
| `global_sample_counter` | Total samples processed | Increasing |
| `anc_state` | 0=IDLE, 1=CALIB, 2=RUNNING | 0→1→2 |

---

## ⚡ Performance

### CPU Usage Breakdown:
```
I2S DMA Transfer:          0% (hardware)
DC Removal + Mic Gain:     5%
Secondary Path Filter:    15% (SEC_PATH_LENGTH taps)
ANC FxLMS Filter:         20% (ANC_FILTER_LENGTH taps)
Sine Synthesizer:          5%
FFT Analysis (async):     10% (triggered every FFT_LENGTH samples)
ITM Reporting:             1%
────────────────────────────────────────────────
Total:                   ~56% @ 550MHz
Headroom:                ~44%
```

### Optimization Techniques:
1. **DTCM Placement** – `active_buffer` in DTCM RAM for zero-wait-state ISR access
2. **XORSHIFT RNG** – Single-cycle random number generation for calibration noise
3. **Bitmask Ring Buffer** – `& HISTORY_MASK` eliminates modulo division in inner loops
4. **ARM CMSIS-DSP** – `arm_scale_f32`, `arm_mean_f32` use hardware SIMD
5. **O3 Optimization** – `__attribute__((optimize("O3")))` on `process_audio_block()`
6. **Safety Clamping** – Branch prediction-friendly min/max instead of `fclamp()`

---

## 🎚️ Tuning Parameters (`anc_config.h`)

### Convergence Speed — `ANC_STEP_SIZE`:
```c
#define ANC_STEP_SIZE  0.001f  // Start here
```
- **Too large** → filter diverges (coefficients blow up despite clamping)
- **Too small** → slow convergence, poor noise cancellation
- **Typical range**: `0.0001` – `0.005` depending on signal level

### Leakage Factor — `ANC_LEAKAGE`:
```c
#define ANC_LEAKAGE  0.9999f
```
- Prevents coefficient drift during silence or transients
- Values closer to `1.0` preserve learned state longer
- Values closer to `0.99` cause faster forgetting

### Calibration Noise Volume — `CALIB_NOISE_VOL`:
```c
#define CALIB_NOISE_VOL  0.1f  // 10% of full scale
```
- Must be audible from the speaker but not too loud
- If calibration fails (coefficients stay near zero), increase this value

### Secondary Path Step — `SEC_PATH_STEP`:
```c
#define SEC_PATH_STEP  0.01f
```
- Controls how fast the secondary path estimate is learned
- Reduce if calibration output sounds unstable

---

## 🐛 Troubleshooting

### Problem: System stays in CALIBRATION forever
**Solution:**
- Increase `CALIB_NOISE_VOL` — speaker may not be loud enough for the mic to hear
- Check I2S3 DAC connections and MCLK enable
- Verify `SEC_PATH_LENGTH` is not larger than the actual acoustic echo delay

### Problem: ANC is active but noise gets louder
**Solution:**
- Reduce `ANC_STEP_SIZE` by 10× — filter is diverging
- Verify `SYSTEM_LATENCY` matches real hardware delay (measure with scope)
- Check that calibration succeeded — print `sec_path_coeffs[0]` to verify non-zero

### Problem: `Gain[0]` exploding before clamp kicks in
**Solution:**
- Reduce `ANC_STEP_SIZE`
- Lower `MIC_SOFTWARE_GAIN` if input signal is too hot
- Clamp range (±2.0) can be reduced for extra safety

### Problem: No output from speaker during calibration
**Solution:**
- Confirm `I2S_MCLKOUTPUT_ENABLE` in `MX_I2S3_Init()`
- Check DAC module power and I2S3 pin connections
- Add a breakpoint inside the calibration branch to verify `output_buffer[i] = wn` is reached

### Problem: ITM console shows nothing
**Solution:**
- Enable SWV in debug configuration with correct core clock (550 MHz)
- Check SWO pin connected on ST-LINK
- Verify `_write()` redirects to `ITM_SendChar()` in `main.c`

---

## 📚 Code Structure

```
main.c
│
├── _write()                    ← Redirects printf to ITM/SWV
├── main()
│   ├── ANC_DSP_Init()          ← Reset i2s↔float utils, DC filter
│   ├── ANC_Analysis_Init()     ← Initialize FFT instance (arm_rfft_fast_init_f32)
│   ├── ANC_Core_Init()         ← Zero all buffers, reset state machine
│   ├── HAL_I2S_Receive_DMA()   ← Start mic circular DMA
│   ├── HAL_I2S_Transmit_DMA()  ← Start DAC circular DMA
│   └── while(1): ANC_Cycle_Handler()
│
anc_core.c
│
├── ANC_Core_Init()             ← Reset all adaptive filter state
├── ANC_Cycle_Handler()         ← State machine: dispatch + ITM reporting
│   ├── process_audio_block()   ← Called when active_buffer != NULL
│   └── Find_Dominant_Freq()    ← Called when is_fft_ready == 1
│
└── process_audio_block()       ← Main audio engine (O3 optimized)
    ├── i2s_to_float()
    ├── remove_dc_offset()
    ├── arm_scale_f32() (mic gain)
    ├── PHASE 1: silence output
    ├── PHASE 2: white noise injection + secondary path LMS
    ├── PHASE 3: FxLMS anti-noise generation
    │   ├── Freq tracking + sine synthesis
    │   ├── Filtered reference x'[n] computation
    │   ├── Anti-noise y[n] from ANC filter
    │   └── Coefficient update with leakage + clamping
    ├── float_to_i2s_stereo()
    └── FFT buffer feeding
```

---

## 🔬 Further Experiments

### Easy Modifications:
1. **Tune step size** → Adjust `ANC_STEP_SIZE` for faster/safer convergence
2. **Multi-frequency ANC** → Run parallel FxLMS instances for harmonic noise
3. **LED indicators** → Drive GPIO based on `anc_state` for visual feedback
4. **Longer filter** → Increase `ANC_FILTER_LENGTH` for lower-frequency noise

### Advanced Projects:
1. **Feedback ANC** → Use error mic as both reference and feedback for broadband noise
2. **NLMS** → Normalize step size by input power for automatic gain-independent convergence
3. **Kalman Filter Secondary Path** → Replace LMS calibration with online Kalman estimation
4. **USB Audio Class** → Replace CDC with UAC2 for lossless PC playback of processed audio
5. **RTOS Migration** → Separate calibration, ANC, and FFT into FreeRTOS tasks

---

## 📖 References

### Documentation:
- [STM32H7 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [CMSIS-DSP Library Guide](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- [I2S Protocol Specification](https://www.sparkfun.com/datasheets/BreakoutBoards/I2SBUS.pdf)

### Algorithm References:
- [AN4990: STM32H7 Audio Getting Started](https://www.st.com/resource/en/application_note/an4990-getting-started-with-audio-features-for-stm32h7-series-mcus-stmicroelectronics.pdf)
- S. M. Kuo and D. R. Morgan, *Active Noise Control Systems*, Wiley, 1996
- [Understanding the FxLMS Algorithm](https://www.sciencedirect.com/topics/engineering/filtered-x-least-mean-square-algorithm)

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

---

## 🙏 Acknowledgments

- **ARM CMSIS-DSP Team** for SIMD-optimized math primitives
- **STMicroelectronics** for comprehensive HAL and BSP libraries
- **Kuo & Morgan** for foundational FxLMS algorithm research

---

## 📧 Support

For questions or issues:
- Open an issue in the [main repository](https://github.com/YourUsername/STM32-Audio-Processing-Suite/issues)
- Contact: [Your Email]

---

**Author**: Bertan Kuzeyli  
**Last Updated**: March 2026  
**Tested On**: STM32H723ZG-NUCLEO Board  
**IDE Version**: STM32CubeIDE 1.12.0
