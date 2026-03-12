# STM32 Audio Processing Suite 🎵

[![STM32](https://img.shields.io/badge/STM32-H723ZG-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-STM32H7-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)

A collection of professional-grade audio processing projects for STM32H7 microcontrollers, featuring real-time DSP algorithms, I2S audio interfacing, and advanced signal analysis.

---

## 🎯 Project Overview

This repository contains three complete audio processing projects demonstrating:
- **Real-time signal processing** using ARM CMSIS-DSP
- **High-performance I2S audio acquisition** at 48kHz/24-bit
- **DMA-based zero-copy buffer management** for continuous streaming
- **FFT-based frequency analysis** with live visualization
- **Adaptive noise cancellation** using the FxLMS algorithm
- **Professional modular embedded software architecture**

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
└── 03-Active-Noise-Cancellation/  ✅ Complete
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

### Performance Metrics:
- **Processing Time**: ~100–150μs per block
- **CPU Usage**: ~28% at 550MHz
- **Sample Rate**: 48kHz
- **Block Latency**: ~166μs

[➡️ View Full Documentation](01-Audio-Gain-Booster/README.md)

---

## 🚀 Project 02 — Real-Time FFT Analyzer

A high-performance audio spectrum analyzer that processes audio in real-time and provides frequency band analysis via ITM telemetry.

### Key Features:
- ⚡ **256-point FFT** using ARM CMSIS-DSP (hardware-optimized)
- 🎚️ **Frequency band analysis**: Bass (0–1kHz), Mid (1–4kHz), High (4–10kHz)
- 📊 **Live monitoring** via ITM (Instrumentation Trace Macrocell)
- 🔊 **Peak and RMS level detection**
- 🎛️ **DC offset removal** with high-pass filtering
- 📈 **Hamming windowing** for spectral leakage reduction

### Performance Metrics:
- **Processing Time**: ~2–3ms per FFT frame
- **CPU Usage**: <50% at 550MHz
- **Sample Rate**: 32kHz
- **Frequency Resolution**: 125Hz per bin

[➡️ View Full Documentation](02-Real-Time-FFT-Analyzer/README.md)

---

## 🚀 Project 03 — Active Noise Cancellation (ANC)

A complete adaptive noise cancellation system using the FxLMS algorithm. The system automatically calibrates the secondary path, locks onto dominant noise frequencies via FFT analysis, and generates anti-noise in real time.

### Key Features:
- 🧠 **FxLMS adaptive filter** with secondary path modeling
- 🔍 **Automatic frequency locking** via FFT dominant frequency detection
- 🔧 **3-phase operation**: Warm-up → Calibration → Active ANC
- 🛡️ **Safety clamping** to prevent filter coefficient explosion
- 📡 **SWV/ITM live reporting** for real-time state monitoring
- 🔄 **Modular architecture**: separate DSP, analysis, and core modules

### Performance Metrics:
- **Processing Time**: ~150–200μs per block
- **Sample Rate**: 48kHz
- **Filter Length**: Configurable (ANC_FILTER_LENGTH)
- **Calibration Duration**: ~2.67 seconds (128000 samples)

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
git clone https://github.com/YourUsername/STM32-Audio-Processing-Suite.git
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

## 📜 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

You are free to:
- ✅ Use commercially
- ✅ Modify and distribute
- ✅ Use in private projects
- ✅ Sublicense

---

## 🙏 Acknowledgments

- **STMicroelectronics** for excellent HAL and BSP libraries
- **ARM** for CMSIS-DSP optimized functions
- **Open-source embedded audio community** for algorithm references and inspiration

---

## 📧 Contact

**Author**: Bertan Kuzeyli  
**Email**: [Your Email]  
**GitHub**: [@YourUsername](https://github.com/YourUsername)

---

<div align="center">

**Built with ❤️ for the embedded audio community**

[Report Bug](https://github.com/YourUsername/STM32-Audio-Processing-Suite/issues) ·
[Documentation](https://github.com/YourUsername/STM32-Audio-Processing-Suite/wiki)

</div>

---

**Last Updated**: March 2026  
**Tested On**: STM32H723ZG-NUCLEO Board  
**IDE Version**: STM32CubeIDE 1.12.0
