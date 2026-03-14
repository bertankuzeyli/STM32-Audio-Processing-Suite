# STM32 Audio Processing Suite 🎵

[![STM32](https://img.shields.io/badge/STM32-H723ZG-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-STM32H7-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)

A collection of audio processing projects for STM32H7 microcontrollers, featuring real-time DSP algorithms, I2S audio interfacing, and signal analysis.

---

## 🎯 Project Overview

This repository contains three audio processing projects demonstrating:
- **Real-time signal processing** using ARM CMSIS-DSP
- **High-performance I2S audio acquisition** at 48kHz/24-bit
- **DMA-based zero-copy buffer management** for continuous streaming
- **FFT-based frequency analysis** with live visualization
- **Adaptive noise cancellation** using the FxLMS algorithm
- **Modular embedded software architecture**

---

## 📂 Repository Structure

```
STM32-Audio-Processing-Suite/
│
├── 01-Audio-Gain-Booster/         ✅ Complete
│   └── Real-time digital amplification with automatic gain control
│
├── 02-Real-Time-FFT-Analyzer/     ✅ Complete
│   └── Real-time spectrum analysis with ITM telemetry
│
└── 03-Active-Noise-Cancellation/  ⚠️ Work in Progress
    └── Adaptive FxLMS noise cancellation with FFT frequency locking
```

---

## 🚀 Project 01 — Audio Gain Booster

A real-time audio amplifier that captures microphone input, removes DC offset, and applies automatic gain control before outputting to a DAC.

### Key Features:
- ⚡ **Ultra-low latency**: 8 samples per block (~166μs at 48kHz)
- 🔇 **DC offset removal** via exponential moving average filter
- 📈 **Automatic Gain Control (AGC)**: up to 10× gain with peak limiting
- 🔁 **Ping-pong DMA buffering** for zero-gap audio streaming
- 📡 **Optional USB CDC streaming** for PC-side monitoring

[➡️ View Full Documentation](01-Audio-Gain-Booster/README.md)

---

## 🚀 Project 02 — Real-Time FFT Analyzer

A real-time audio spectrum analyzer that processes audio and provides frequency band analysis via ITM telemetry.

### Key Features:
- ⚡ **256-point FFT** using ARM CMSIS-DSP (hardware-optimized)
- 🎚️ **Frequency band analysis** based on define-configurable bin boundaries:
  - Bass: bins 0–8 → **0–1000 Hz**
  - Mid: bins 8–32 → **1000–4000 Hz**
  - High: bins 32–80 → **4000–10000 Hz**
- 📊 **Frequency resolution**: 125 Hz/bin (32kHz sample rate ÷ 256 points)
- 📡 **Live monitoring** via ITM (Instrumentation Trace Macrocell) — ports 1–5
- 🔊 **Peak and RMS level detection**
- 🎛️ **DC offset removal** via high-pass filter (`HPF_ALPHA = 0.996`)
- 📈 **Hamming windowing** for spectral leakage reduction

> **Note**: All frequency band boundaries (`BASS_END`, `MID_END`, `HIGH_END`) and `FFT_SIZE` are defined as compile-time constants and can be freely adjusted for different use cases.

[➡️ View Full Documentation](02-Real-Time-FFT-Analyzer/README.md)

---

## 🚀 Project 03 — Active Noise Cancellation (ANC)

A narrowband virtual feedback ANC system using the FxLMS algorithm. The system uses a **single microphone pointed at the speaker** (feedback topology — no separate reference microphone) and targets only the dominant noise frequency detected via FFT, rather than attempting broadband cancellation.

### System Architecture

#### Topology: Narrowband Virtual Feedback
The design uses a **virtual feedback** approach: there is only one microphone, facing the speaker. The error signal is not measured by a separate error mic but is derived from the same feedback microphone. This makes the system a single-sensor, single-channel (1×1) narrowband ANC. The FxLMS filter adapts only around the dominant frequency bin identified by the FFT stage, keeping computational load low and the adaptive filter focused.

#### State Machine Operation
The system runs as a cyclic state machine. On every power-on or reset, it starts from the beginning:

| Step | State | Description |
|------|-------|-------------|
| 1 | **CALIBRATION** | Runs once on every power-on. White noise is injected into the speaker; the microphone captures the acoustic response and FxLMS estimates the secondary path S(z). Takes ~2.67s (128k samples). |
| 2 | **FFT LOCK** | FFT analysis of incoming audio. Dominant frequency bin is identified and the narrowband filter is tuned to that target. |
| 3 | **ACTIVE ANC** | FxLMS filter runs in real time. Anti-noise is generated and output via DAC. |
| ↺ | **Loop** | After ACTIVE ANC, the system returns to FFT LOCK and re-analyzes the dominant frequency. Steps 2 and 3 repeat continuously until power-off. |
| ⚡ | **Power-off / Reset** | The entire sequence restarts from CALIBRATION. The secondary path model is not stored persistently. |

On power-off and restart, the full sequence begins again from **CALIBRATION**, as the secondary path model is not stored persistently.

### Key Features:
- 🧠 **FxLMS adaptive filter** with online secondary path modeling
- 🎯 **Narrowband targeting** — adapts only around the FFT-detected dominant frequency
- 🔁 **Virtual feedback topology** — single microphone, no reference mic required
- 🔄 **Cyclic state machine**: CALIBRATION → FFT_LOCK → ACTIVE_ANC → FFT_LOCK → ...
- 🛡️ **Safety clamping** to prevent filter coefficient divergence
- 📡 **SWV/ITM live state reporting** — current state visible in real time via ITM ports
- 🔧 **Modular architecture**: separate DSP, analysis, and core modules

### ⚠️ Current Status — Work in Progress

Hardware testing was performed using an **STM32H723ZG NUCLEO board**, an I2S MEMS microphone, an I2S DAC module, and a **Logitech Z323 speaker** as the noise source. All state machine phases run correctly at the firmware level — secondary path calibration completes, FFT frequency locking works, and the active ANC stage executes — however, **effective noise suppression has not yet been achieved** in the physical setup.

The working hypothesis is that the limitations stem from the acoustic constraints of the virtual feedback topology: with a single microphone also picking up the anti-noise output, the feedback path creates conditions that are difficult to stabilize in a real room environment without careful acoustic isolation. Secondary path estimation accuracy under these conditions may also be insufficient.

Contributions, suggestions, or insights from anyone with single-microphone or virtual feedback ANC experience are very welcome.

[➡️ View Full Documentation](03-Active-Noise-Cancellation/README.md)

---

## 🛠️ Hardware Requirements

### Microcontroller:
- **STM32H723ZG** (Cortex-M7 @ 550MHz)
- 1MB Flash, 564KB RAM
- Hardware FPU for DSP operations
- DTCM RAM for latency-critical variables

### Peripherals Used:
| Peripheral | Role |
|-----------|------|
| I2S1 | Audio input – Microphone RX |
| I2S3 | Audio output – DAC TX |
| DMA1 | Circular buffer management |
| USB FS/HS | Virtual COM port (optional) |
| ITM/SWV | Real-time telemetry to debugger |
| USART3 | COM1 serial output |

### External Components:
- I2S MEMS Microphone (24-bit): INMP441, ICS-43434, SPH0645
- I2S DAC Module: PCM5102A, UDA1334A, MAX98357A
- ST-LINK/V3 debugger (required for ITM/SWV)

---

## 💻 Software Requirements

### Development Tools:
- **STM32CubeIDE** v1.12.0 or later
- **STM32CubeMX** (for hardware configuration)
- **ARM CMSIS-DSP** library (included in HAL)

### Optional Tools:
- **Serial Wire Viewer (SWV)** for ITM data visualization
- **STM Studio** for real-time variable plotting
- **Python / MATLAB** for offline audio analysis

---

## 🔧 Getting Started

### 1. Clone the Repository:
```bash
git clone https://github.com/bertankuzeyli/STM32-Audio-Processing-Suite.git
cd STM32-Audio-Processing-Suite
```

### 2. Open a Project in STM32CubeIDE:
```
File → Open Projects from File System
Select the desired project folder (e.g. 03-Active-Noise-Cancellation)
```

### 3. Build and Flash:
```
Project → Build All  (Ctrl+B)
Run → Debug          (F11)
```

### 4. Enable SWV Monitoring (for Projects 02 and 03):
```
Debug Configuration → Debugger → Enable Serial Wire Viewer (SWV)
Core Clock: 550 MHz
Window → Show View → SWV → SWV ITM Data Console
Enable ITM Stimulus Ports 1-5
```

---

## 🎓 Educational Value

### Concepts Demonstrated:
1. **Real-time DSP** – Audio processing within hard real-time constraints
2. **Adaptive Filtering** – FxLMS algorithm and secondary path estimation
3. **Efficient Memory Management** – Ping-pong DMA buffering, DTCM placement
4. **Signal Processing Theory** – Windowing, DC removal, FFT, spectral analysis
5. **Embedded Optimization** – ARM SIMD instructions, O3 compiler flags, cache alignment
6. **Professional Debugging** – ITM telemetry, live variable monitoring, state reporting
7. **Modular Firmware Design** – Separated config, DSP utils, analysis, and core layers

---

## 🙏 Acknowledgments

- **STMicroelectronics** for excellent HAL and BSP libraries
- **ARM** for CMSIS-DSP optimized functions
- **Open-source embedded audio community** for algorithm references and inspiration

---

## 📧 Contact

**Author**: Bertan Kuzeyli  
**Email**: bertankuzeyli@gmail.com  
**GitHub**: [@bertankuzeyli](https://github.com/bertankuzeyli)

---

<div align="center">

**Built with ❤️ for the embedded audio community**

[Report Bug](https://github.com/bertankuzeyli/STM32-Audio-Processing-Suite/issues) ·
[Documentation](https://github.com/bertankuzeyli/STM32-Audio-Processing-Suite/wiki)

</div>

---

**Last Updated**: March 2026  
**Tested On**: STM32H723ZG-NUCLEO Board  
**IDE Version**: STM32CubeIDE 1.19.0
