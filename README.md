# STM32 Audio Processing Suite 🎵

[![STM32](https://img.shields.io/badge/STM32-H723ZG-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-STM32H7-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)

A collection of professional-grade audio processing projects for STM32H7 microcontrollers, featuring real-time DSP algorithms, I2S audio interfacing, and advanced signal analysis.

---

## 🎯 Project Overview

This repository contains multiple audio processing projects demonstrating:
- **Real-time signal processing** using ARM CMSIS-DSP
- **High-performance I2S audio acquisition** at 32kHz/24-bit
- **DMA-based zero-copy buffer management** for continuous streaming
- **FFT-based frequency analysis** with live visualization
- **Professional embedded software architecture**

---

## 📂 Repository Structure

```
STM32-Audio-Processing-Suite/
│
├── 02-Real-Time-FFT-Analyzer/     ✅ Available
│   └── Real-time spectrum analysis with ITM telemetry
│
├── 01-Audio-Gain-Booster/         🚧 Coming Soon
│   └── Digital audio amplification with normalization
│
└── 03-Active-Noise-Cancellation/  🚧 Coming Soon
    └── Adaptive filtering for ANC applications
```

---

## 🚀 Featured Project: Real-Time FFT Analyzer

A high-performance audio spectrum analyzer that processes audio in real-time and provides frequency band analysis.

### Key Features:
- ⚡ **256-point FFT** using ARM CMSIS-DSP (hardware-optimized)
- 🎚️ **Frequency band analysis**: Bass (0-1kHz), Mid (1-4kHz), High (4-10kHz)
- 📊 **Live monitoring** via ITM (Instrumentation Trace Macrocell)
- 🔊 **Peak and RMS level detection**
- 🎛️ **DC offset removal** with high-pass filtering
- 📈 **Hamming windowing** for spectral leakage reduction

### Performance Metrics:
- **Processing Time**: ~2-3ms per FFT frame
- **CPU Usage**: <50% at 480MHz
- **Sample Rate**: 32kHz
- **Frequency Resolution**: 125Hz per bin

[➡️ View Full Documentation](02-Real-Time-FFT-Analyzer/README.md)

---

## 🛠️ Hardware Requirements

### Microcontroller:
- **STM32H723ZG** (Cortex-M7 @ 550MHz)
- 1MB Flash, 564KB RAM
- Hardware FPU for DSP operations

### Peripherals Used:
- **I2S1** - Audio input interface (Master RX mode)
- **DMA1** - Circular buffer management
- **USB** - Virtual COM port (optional)
- **ITM** - Real-time data streaming to debugger

### External Components:
- I2S Microphone (24-bit) or Audio Codec
- ST-LINK/V3 debugger (for ITM support)

---

## 💻 Software Requirements

### Development Tools:
- **STM32CubeIDE** v1.12.0 or later
- **STM32CubeMX** (for hardware configuration)
- **ARM CMSIS-DSP** library (included in HAL)

### Optional Tools:
- **Serial Wire Viewer (SWV)** for ITM data visualization
- **STM Studio** for real-time variable plotting
- **MATLAB/Python** for offline analysis

---

## 🔧 Getting Started

### 1. Clone the Repository:
```bash
git clone https://github.com/YourUsername/STM32-Audio-Processing-Suite.git
cd STM32-Audio-Processing-Suite
```

### 2. Open in STM32CubeIDE:
```
File → Open Projects from File System
Select: 02-Real-Time-FFT-Analyzer
```

### 3. Build and Flash:
```
Project → Build All (Ctrl+B)
Run → Debug (F11)
```

### 4. Enable ITM Monitoring:
```
Debug Configuration → Debugger → Serial Wire Viewer (SWV)
Enable ITM Stimulus Ports 1-5
Window → Show View → SWV → Data Trace Timeline Graph
```

---

## 📊 Live Variable Monitoring

Add these variables to the **Live Expressions** window in STM32CubeIDE:

| Variable | Description | Range |
|----------|-------------|-------|
| `live_bass_level` | Bass frequencies (0-1kHz) | 0-100 |
| `live_mid_level` | Mid frequencies (1-4kHz) | 0-100 |
| `live_high_level` | High frequencies (4-10kHz) | 0-100 |
| `live_peak_level` | Peak amplitude | 0-100 |
| `live_rms_level` | RMS signal level | 0-100 |
| `live_total_energy` | Total spectral energy | Float |
| `live_fft_counter` | FFT execution count | Integer |

---

## 🎓 Educational Value

### Concepts Demonstrated:
1. **Real-time DSP**: FFT computation within hard real-time constraints
2. **Efficient Memory Management**: Ping-pong DMA buffering
3. **Signal Processing Theory**: Windowing, DC removal, spectral analysis
4. **Embedded Optimization**: ARM SIMD instructions, cache management
5. **Professional Debugging**: ITM telemetry, live variable monitoring

### Learning Resources:
- [ARM CMSIS-DSP Documentation](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- [I2S Protocol Guide](https://www.nxp.com/docs/en/user-guide/UM10732.pdf)
- [FFT Windowing Tutorial](https://download.ni.com/evaluation/pxi/Understanding%20FFTs%20and%20Windowing.pdf)

---

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. **Report Bugs**: Open an issue with reproduction steps
2. **Suggest Features**: Describe your audio processing use case
3. **Submit Pull Requests**: 
   - Fork the repository
   - Create a feature branch
   - Add your improvements
   - Submit a PR with clear description

### Contribution Guidelines:
- Follow existing code style (comments, formatting)
- Test on real hardware before submitting
- Update documentation for new features
- Provide performance benchmarks where applicable

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

You are free to:
- ✅ Use commercially
- ✅ Modify and distribute
- ✅ Use in private projects
- ✅ Sublicense

**Attribution appreciated but not required!**

---

## 🙏 Acknowledgments

- **STMicroelectronics** for excellent HAL libraries
- **ARM** for CMSIS-DSP optimized functions
- **Open-source community** for inspiration and support

---

## 📧 Contact

**Author**: [Your Name]  
**Email**: [Your Email]  
**GitHub**: [@YourUsername](https://github.com/YourUsername)  
**LinkedIn**: [Your Profile](https://linkedin.com/in/yourprofile)

---

## 🔮 Roadmap

### Coming Soon:
- [ ] Audio Gain Booster project
- [ ] Active Noise Cancellation (ANC) system
- [ ] Python visualization tools
- [ ] MATLAB integration examples
- [ ] Performance optimization guides

### Future Ideas:
- Audio effects (reverb, EQ, compressor)
- Voice activity detection
- Musical note detection
- Audio streaming over network

---

## ⭐ Star This Repository!

If you find this project useful, please consider giving it a star ⭐  
It helps others discover these learning resources!

---

<div align="center">

**Built with ❤️ for the embedded audio community**

[Report Bug](https://github.com/YourUsername/STM32-Audio-Processing-Suite/issues) · 
[Request Feature](https://github.com/YourUsername/STM32-Audio-Processing-Suite/issues) · 
[Documentation](https://github.com/YourUsername/STM32-Audio-Processing-Suite/wiki)

</div>
