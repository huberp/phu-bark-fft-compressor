# Instructions for GitHub Copilot

## Building This Project
This is a JUCE-based audio plugin project (VST3 Bark-band FFT compressor).

### Before Building on Linux
1. Install dependencies: `sudo bash scripts/install-linux-deps.sh`
2. Initialize submodules: `git submodule update --init --recursive`
3. Use Linux preset: `cmake --preset linux-release && cmake --build --preset linux-build`

### Before Building on Windows (PowerShell)
1. Initialize submodules: `git submodule update --init --recursive`
2. Find cmake executable with script .\scripts\find-cmake.ps1. Don't try to use cmake without this step, as it may not be in your PATH.
3. Configure: `cmake --preset vs2026-x64`
4. Build: `cmake --build --preset release`

### Build Timeouts
- Use fewer parallel jobs on CI: `cmake --build --preset linux-build -j2`
- Ensure all dependencies installed before attempting build
- JUCE submodule must be fully initialized

## Architecture
- **lib/audio/BarkFFTCompressor.h** — Header-only FFT-based compressor DSP
  - 2048-point FFT with 50% overlap-add Hann window
  - 24 Bark bands mapped from FFT bins
  - Equal-loudness contour presets (ISO 226: 20/40/60/80 phon + flat)
  - Per-band compression with threshold/ratio/attack/release
- **lib/audio/AudioSampleFifo.h** — Lock-free FIFO for audio→UI sample transport
- **lib/audio/FFTProcessor.h** — UI-thread FFT processor for spectrum visualization
- **src/PluginProcessor** — JUCE AudioProcessor wrapper with APVTS parameters
- **src/PluginEditor** — Minimal UI with spectrum display, gain reduction meters, sliders
- **src/SpectrumDisplay** — FFT spectrum with Bark band overlays and contour display

## Conventions
- C++17, no template wizardry
- Header-only library code in lib/ (no plugin/UI dependencies)
- Lock-free communication between audio and UI threads (AudioSampleFifo)
- No memory allocation, system calls, or locks on audio thread
- JUCE APVTS for parameter management
- CMake with presets for cross-platform builds
