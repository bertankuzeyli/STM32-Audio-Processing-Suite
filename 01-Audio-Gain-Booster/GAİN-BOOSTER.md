# Real-Time Audio Gain Booster 🔊

A high-performance real-time audio amplification system for STM32H7 microcontrollers featuring automatic gain control, DC offset removal, and dual-channel I2S audio streaming.

---

## 🎯 Project Description

This project implements a professional-grade audio processing pipeline that captures audio from an I2S MEMS microphone, applies intelligent normalization, and outputs amplified audio to a DAC module in real time.

### What It Does:
- 🎤 Captures 24-bit audio at 48kHz sample rate via I2S MEMS microphone
- ⚡ Processes audio blocks in ~100–150μs (8 samples per block)
- 🔇 Removes DC offset using an exponential moving average high-pass filter
- 📈 Applies automatic gain control (AGC) to maximize loudness without clipping
- 🔁 Streams processed stereo audio to a DAC module via I2S
- 📡 Optionally transmits processed samples to PC via USB CDC

---

## 🏗️ System Architecture

```
┌─────────────┐      I2S       ┌──────────────┐      DMA      ┌─────────────┐
│  I2S MEMS   │─────────────────▶│  STM32H7    │◀──────────────│ Ping-Pong   │
│ Microphone  │   24-bit/48kHz  │   I2S1 RX   │   Circular    │  32 samples │
└─────────────┘                 └──────────────┘               └─────────────┘
                                       │
                                       ▼
                                ┌──────────────┐
                                │  DC Removal  │
                                │  EMA α=0.9995│
                                └──────────────┘
                                       │
                                       ▼
                                ┌──────────────┐
                                │   Automatic  │
                                │  Gain Control│
                                └──────────────┘
                                       │
                          ┌────────────┴────────────┐
                          ▼                         ▼
                  ┌───────────────┐         ┌──────────────┐
                  │  I2S3 TX DMA  │         │  USB CDC TX  │
                  │  DAC Output   │         │  (Optional)  │
                  └───────────────┘         └──────────────┘
                          │
                          ▼
                  ┌───────────────┐
                  │   DAC Module  │
                  │  Stereo Out   │
                  └───────────────┘
```

---

## 🔬 Technical Specifications

### Audio Processing:
| Parameter | Value |
|-----------|-------|
| Sample Rate | 48 kHz |
| Bit Depth | 24-bit |
| Block Size | 8 samples |
| Block Latency | ~166 μs |
| Processing Time | ~100–150 μs/block |
| Throughput | ~6000 blocks/sec |

### Normalization Parameters:
| Parameter | Value | Description |
|-----------|-------|-------------|
| Target Peak | 0.9 | 90% of max headroom |
| Max Gain | 10× (20 dB) | Upper amplification cap |
| Min Signal | 0.00001 | Noise floor threshold |
| DC Alpha | 0.9995 | EMA smoothing factor |

### USB Packet Format:
| Field | Size | Value |
|-------|------|-------|
| SOF Marker | 2 bytes | `0x55AA` |
| Float Samples | 32 bytes | 8 × float32 |
| EOF Marker | 1 byte | `0x0A` |

---

## 🛠️ Hardware Setup

### Microcontroller:
- **Part Number**: STM32H723ZGT6
- **Core**: ARM Cortex-M7 @ 550 MHz
- **FPU**: Double-precision floating-point unit
- **Flash**: 1 MB
- **RAM**: 564 KB

### Pin Configuration:

| Peripheral | Pin | Function |
|------------|-----|----------|
| I2S1_WS | PA4 | Word Select – Mic Input (LRCLK) |
| I2S1_CK | PA5 | Bit Clock – Mic Input (BCLK) |
| I2S1_SD | PA7 | Serial Data In (from Mic) |
| I2S3_WS | PA15 | Word Select – DAC Output |
| I2S3_CK | PB3 | Bit Clock – DAC Output |
| I2S3_SD | PB5 | Serial Data Out (to DAC) |
| USB_DP | PA12 | USB Data+ (optional monitoring) |
| USB_DM | PA11 | USB Data- (optional monitoring) |

### I2S Microphone Connection:
```
STM32H7          I2S MEMS Mic
────────         ────────────
PA4 (WS)   ────▶ LR/SEL
PA5 (CK)   ────▶ CLK
PA7 (SD)   ◀──── DATA
GND        ────▶ GND
3.3V       ────▶ VDD
```

**Recommended Microphones:**
- INMP441 (Invensense)
- ICS-43434 (TDK)
- SPH0645 (Knowles)

### I2S DAC Connection:
```
STM32H7          I2S DAC Module
────────         ──────────────
PA15 (WS)  ────▶ LRCK
PB3  (CK)  ────▶ BCK
PB5  (SD)  ────▶ DIN
GND        ────▶ GND
3.3V       ────▶ VDD
```

**Recommended DAC Modules:**
- PCM5102A (Texas Instruments)
- UDA1334A (NXP)
- MAX98357A (Maxim)

---

## 💻 Software Configuration

### STM32CubeMX Settings:

#### I2S1 (Microphone Input):
```
Mode: Master Receive
Standard: Philips I2S
Data Format: 24-bit
Sample Rate: 48 kHz
MCLK Output: Disabled
DMA: Enabled (Circular Mode)
```

#### I2S3 (DAC Output):
```
Mode: Master Transmit
Standard: Philips I2S
Data Format: 24-bit
Sample Rate: 48 kHz
MCLK Output: Enabled
DMA: Enabled (Circular Mode)
```

#### DMA Settings:
```
DMA1 Stream 0 – I2S1 RX
Direction: Peripheral to Memory
Mode: Circular
Priority: Very High
Data Width: Word (32-bit)

DMA1 Stream 1 – I2S3 TX
Direction: Memory to Peripheral
Mode: Circular
Priority: Very High
Data Width: Word (32-bit)
```

#### Clock Configuration:
```
PLL1: 550 MHz (main system clock)
HSE: External crystal
HSI48: Enabled for USB
AHB Divider: /2 → 275 MHz
APB1/2/3/4: /2 → 137.5 MHz
```

---

## 📊 DSP Pipeline Details

### Stage 1: I2S to Float Conversion
```c
// Extract left channel from stereo I2S buffer
// I2S format: 32-bit word with 24-bit data, MSB-aligned
int32_t raw = i2s_data[i * 2] >> 8;          // Align 24-bit data
float normalized = raw * (1.0f / 8388608.0f); // Scale to [-1.0, 1.0]
```

### Stage 2: DC Offset Removal
```c
// Exponential Moving Average (EMA) high-pass filter
// Tracks slow drift and subtracts it from signal
block_mean = mean(data[0:N]);
dc_offset = 0.9995 * dc_offset + 0.0005 * block_mean;
data[i] -= dc_offset;  // In-place removal
```

### Stage 3: Automatic Gain Control
```c
// Find peak amplitude in current block
max_abs = max(|data[i]|);

// Compute gain capped at MAX_GAIN
gain = min(TARGET_PEAK / max_abs, MAX_GAIN);  // 0.9 / peak, max 10×

// Apply gain to all samples
output[i] = input[i] * gain;
```

### Stage 4: Float to I2S Stereo Output
```c
// Scale back to 24-bit integer range
int32_t val = (int32_t)(output[i] * 8388608.0f);

// Left-align for Philips I2S standard
int32_t sample = val << 8;

// Duplicate mono to stereo
i2s_tx[i*2]   = sample;  // Left
i2s_tx[i*2+1] = sample;  // Right
```

---

## ⚡ Performance & Optimization

### CPU Usage Breakdown:
```
I2S DMA Transfer:         0% (hardware)
I2S to Float Conversion:  5% (~10μs)
DC Offset Removal:        8% (~15μs)
AGC Normalization:       10% (~20μs)
Float to I2S Conversion:  5% (~10μs)
USB Packet (optional):   12% (~25μs)
────────────────────────────────────
Total (no USB):          28% (~55μs)
Total (with USB):        40% (~80μs)
Headroom:               60–72% free
```

### Optimization Techniques Used:
1. **Hardware DMA** – Ping-pong buffering with zero CPU intervention during transfer
2. **ARM CMSIS-DSP** – `arm_mean_f32`, `arm_abs_f32`, `arm_max_f32`, `arm_scale_f32` using SIMD
3. **Compiler O3** – `__attribute__((optimize("O3")))` on all audio functions
4. **Cache Alignment** – 32-byte aligned buffers (`__attribute__((aligned(32)))`)
5. **Ping-Pong Buffering** – Process block N while DMA fills block N+1

---

## 🎚️ Tuning Parameters

### AGC Target Level (Line 86):
```c
const float32_t TARGET_PEAK = 0.9f;  // Adjust output loudness
```
- `0.5` → quieter output with more headroom
- `0.9` → near-maximum loudness (recommended)
- `1.0` → risk of occasional clipping

### Maximum Gain Limit (Line 87):
```c
const float32_t MAX_GAIN = 10.0f;  // Caps amplification at 20 dB
```
- **Too low** → very quiet signals remain quiet
- **Too high** → noise floor gets boosted during silence
- **Ideal** → `10.0f` for speech; `5.0f` for music

### DC Offset Tracking Speed (Line 82):
```c
const float32_t DC_ALPHA = 0.9995f;  // ~10 Hz cutoff
```
- `0.999` → faster DC tracking (~50 Hz cutoff)
- `0.9999` → slower tracking (~1 Hz cutoff, preserves more bass)

---

## 🐛 Troubleshooting

### Problem: No audio output from DAC
**Solution:**
- Verify I2S3 pin connections (WS, CK, SD)
- Confirm DAC module power supply (3.3V)
- Check `MX_I2S3_Init()` — MCLK must be enabled for most DACs
- Inspect DMA1 Stream 1 interrupt priority

### Problem: Output is distorted or clipping
**Solution:**
- Reduce `TARGET_PEAK` to `0.7f`
- Reduce `MAX_GAIN` to `5.0f`
- Verify microphone is not overloading (check with oscilloscope)

### Problem: Loud hum or DC drift in output
**Solution:**
- Decrease `DC_ALPHA` to `0.999` for faster DC removal
- Ensure microphone ground is connected to MCU GND
- Add decoupling capacitors near microphone VDD pin

### Problem: USB CDC not receiving data
**Solution:**
- Uncomment `send_usb_packet()` call in `process_audio_block()`
- Install virtual COM port driver (STM32 VCP)
- Ensure USB cable supports data (not charge-only)
- Check `CDC_Transmit_HS()` return value is `USBD_OK`

### Problem: Audio processing not triggering
**Solution:**
- Confirm DMA interrupts are enabled in `MX_DMA_Init()`
- Verify `active_buffer != NULL` check in main loop
- Set breakpoint in `HAL_I2S_RxHalfCpltCallback()` to confirm DMA fires

---

## 📚 Code Structure

```
Core/Src/main.c
│
├── Configuration Constants (Lines 30–52)
│   ├── NUM_SAMPLES = 8 (block size)
│   ├── I2S_BUFFER_SIZE = 32 (total circular buffer)
│   ├── USB packet markers (SOF / EOF)
│   └── Normalization constants
│
├── Global Variables (Lines 55–115)
│   ├── DMA buffers (i2s_rx_buffer, i2s_tx_buffer)
│   ├── active_buffer pointer (volatile, set by ISR)
│   ├── USB packet structure
│   └── DSP working buffers (input, output, temp)
│
├── Signal Processing Functions
│   ├── audio_init()            – Reset DC estimator
│   ├── i2s_to_float()          – 24-bit I2S → float [-1, 1]
│   ├── float_to_i2s_stereo()   – Float → stereo I2S output
│   ├── remove_dc_offset()      – EMA-based DC removal
│   ├── normalize_audio()       – AGC with peak limiting
│   ├── send_usb_packet()       – USB CDC transmission
│   └── process_audio_block()   – Full pipeline orchestrator
│
├── Hardware Initialization
│   ├── SystemClock_Config()    – 550 MHz PLL setup
│   ├── MX_I2S1_Init()          – Microphone RX configuration
│   ├── MX_I2S3_Init()          – DAC TX configuration
│   └── MX_DMA_Init()           – DMA controller & IRQ setup
│
├── DMA Interrupt Callbacks
│   ├── HAL_I2S_RxHalfCpltCallback()  – First half buffer ready
│   └── HAL_I2S_RxCpltCallback()      – Second half buffer ready
│
└── Main Loop
    └── Polls active_buffer flag and calls process_audio_block()
```

---

## 🔬 Further Experiments

### Easy Modifications:
1. **Change block size** → Modify `NUM_SAMPLES` (powers of 2: 4, 8, 16, 32)
2. **Adjust sample rate** → Change `AudioFreq` in `.ioc` and I2S init
3. **Enable USB streaming** → Uncomment `send_usb_packet()` in `process_audio_block()`
4. **Add LED VU meter** → Threshold on `max_abs` to drive GPIO pins

### Advanced Projects:
1. **Noise Gate** → Mute output when signal is below RMS threshold
2. **Soft Limiter / Compressor** → Smooth AGC with attack/release times
3. **Real-Time Equalizer** → Combine with FFT project for frequency-band gain
4. **Voice Activity Detection (VAD)** → Detect speech presence using RMS + ZCR
5. **Stereo Widening** → Apply decorrelation between left/right I2S channels
6. **USB Audio Class** → Replace CDC with UAC2 for lossless PC playback

---

## 📖 References

### Documentation:
- [STM32H7 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [CMSIS-DSP Library Guide](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- [I2S Protocol Specification](https://www.sparkfun.com/datasheets/BreakoutBoards/I2SBUS.pdf)
- [USB CDC Class Specification](https://www.usb.org/document-library/class-definitions-communication-devices-12)

### Application Notes:
- [AN4990: Getting started with STM32H7 audio](https://www.st.com/resource/en/application_note/an4990-getting-started-with-audio-features-for-stm32h7-series-mcus-stmicroelectronics.pdf)
- [AN5027: Using the CMSIS-DSP library](https://www.st.com/resource/en/application_note/an5027-using-the-arm-cortexm-dsp-libraries-on-stm32h7-microcontrollers-stmicroelectronics.pdf)

### Learning Resources:
- [Understanding Digital Audio](https://www.soundonsound.com/techniques/digital-audio-fundamentals)
- [Automatic Gain Control Theory](https://www.dspguide.com/ch9.htm)
- [MEMS Microphone Application Guide – Invensense](https://invensense.tdk.com/wp-content/uploads/2015/02/AN-1124.pdf)

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

---

## 🙏 Acknowledgments

- **ARM CMSIS-DSP Team** for SIMD-optimized math primitives
- **STMicroelectronics** for comprehensive HAL and BSP libraries
- **Open-source audio processing community** for algorithm references

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
