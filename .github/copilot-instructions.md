# Instructions for GitHub Copilot

JUCE audio-plugin project for Bark-band FFT compression with an integrated transient shaper.

## Build
- Initialize submodules first: `git submodule update --init --recursive`
- Presets live in `CMakePresets.json` and follow `<os>-<arch>-<format>-<config>` with matching `-build` presets
- Linux x64 VST3 release:
  - `sudo bash scripts/install-linux-deps.sh`
  - `cmake --preset linux-x64-vst3-release`
  - `cmake --build --preset linux-x64-vst3-release-build`
- Windows x64 VST3 release (PowerShell):
  - `.\scripts\find-cmake.ps1`
  - `cmake --preset windows-x64-vst3-release`
  - `cmake --build --preset windows-x64-vst3-release-build`
- Optional Intel MKL FFT acceleration: set `MKLROOT`; disable with `-DUSE_INTEL_MKL=OFF`
- If builds time out, reduce parallelism, e.g. `cmake --build --preset linux-x64-vst3-release-build -j2`
- No standalone test suite is configured; validate changes with the relevant configure/build preset

## Architecture
- `lib/audio/BarkFFTCompressor.h` — header-only spectral compressor with Precision/Transient FFT modes, 24 Bark bands, ISO 226 contour thresholds, and 50% overlap-add reconstruction
- `lib/audio/TransientShaper.h` — header-only transient processor applied after compression
- `lib/audio/AudioSampleFifo.h` — lock-free audio-to-UI sample transport
- `src/PluginProcessor.*` — JUCE `AudioProcessor` + APVTS, owns DSP chain and FIFOs
- `src/PluginEditor.*` — UI layout, timer-driven updates, parameter attachments
- `src/SpectrumDisplay.*` — spectrum, Bark overlays, contour, and gain-reduction display

## Conventions
- C++20
- Keep DSP code in `lib/` header-only and free of JUCE/UI dependencies
- No allocation, locks, or system calls on the audio thread
- Use lock-free audio/UI communication only
- Use JUCE APVTS for parameter state; audio-thread reads should stay lock-free
