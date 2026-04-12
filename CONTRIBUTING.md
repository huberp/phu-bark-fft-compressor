# Contributing to PHU Bark FFT Compressor

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++17/JUCE code style
3. Keep to the conventions in [`.github/copilot-instructions.md`](.github/copilot-instructions.md) — in particular: **no memory allocation, system calls, or locks on the audio thread**
4. Verify the project builds and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-bark-fft-compressor/issues).

---

## Project Layout

```
phu-bark-fft-compressor/
├── CMakeLists.txt / CMakePresets.json
├── doc/                             Screenshots
├── JUCE/                            JUCE 8.0.12 (git submodule)
├── src/
│   ├── PluginProcessor.h/cpp        processBlock, APVTS, FIFOs
│   ├── PluginEditor.h/cpp           UI layout, 60 Hz timer, control wiring
│   ├── SpectrumDisplay.h/cpp        Spectrum + Bark overlay + GR bar
│   └── CMakeLists.txt
├── lib/
│   └── audio/
│       ├── BarkFFTCompressor.h      FFT-based Bark-band compressor (header-only)
│       ├── TransientShaper.h        Dual-envelope transient processor (header-only)
│       ├── AudioSampleFifo.h        Lock-free audio→UI sample transport
│       └── FFTProcessor.h           UI-thread FFT for spectrum display
├── .github/workflows/               CI build + pluginval + release workflows
└── scripts/
    ├── find-cmake.ps1               Locates CMake on Windows
    └── install-linux-deps.sh        Installs JUCE Linux dependencies
```
