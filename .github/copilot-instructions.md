# Instructions for GitHub Copilot

JUCE audio-plugin project for Bark-band FFT compression. An optional Transient Shaper is available as a compile-time feature (`TRANSIENT_SHAPER`, default OFF).

## Build
- No submodules: JUCE and `phu-audio-lib` are fetched by CMake (`FetchContent`, see `cmake/PhuDependencies.cmake`) at configure time, so Git and network access are required on the first configure
- Presets live in `CMakePresets.json` and follow `<os>-<arch>-<format>-<config>` with matching `-build` presets
- Only `linux-x64-vst3-release` and `windows-x64-vst3-release` are documented below. For any other preset (macOS, other architectures, AU, Standalone), read `CMakePresets.json` for the exact name; do not guess preset names- Linux x64 VST3 release:
  - `sudo bash scripts/install-linux-deps.sh`
  - `cmake --preset linux-x64-vst3-release`
  - `cmake --build --preset linux-x64-vst3-release-build`
- Windows x64 VST3 release (PowerShell, Visual Studio 18 2026 generator):
  - Run `.\scripts\find-cmake.ps1`; use the reported `cmake.exe` path if `cmake` is not already on `PATH`
  - `cmake --preset windows-x64-vst3-release`
  - `cmake --build --preset windows-x64-vst3-release-build`
- Optional Intel MKL FFT acceleration: set `MKLROOT`; disable with `-DUSE_INTEL_MKL=OFF`. If `MKLROOT` is not set, configure with `-DUSE_INTEL_MKL=OFF`; do not try to install or locate MKL automatically (without MKL, FFT falls back to JUCE)
- Optional Transient Shaper (DSP + UI): configure with `-DTRANSIENT_SHAPER=ON` (default OFF). Verify both shaper states build: (a) configure and build the chosen preset with the default `TRANSIENT_SHAPER=OFF`, and (b) configure the same preset with `-DTRANSIENT_SHAPER=ON` and build it
- If builds time out, reduce parallelism, e.g. `cmake --build --preset linux-x64-vst3-release-build -j2`
- No standalone test suite is configured; validate changes by running the configure and build commands for `linux-x64-vst3-release` on Linux or `windows-x64-vst3-release` on Windows, and report which preset was used

## Architecture
- `lib/audio/BarkFFTCompressor.h` — header-only spectral compressor with Precision/Transient FFT modes, 24 Bark bands, equal-loudness contour thresholds (ISO 226 / Smile / Flat), and 50%, 75% or 90% overlap-add reconstruction
- `lib/audio/TransientShaper.h` — header-only shaper DSP (JUCE-free, applied after compression); only built when `TRANSIENT_SHAPER=ON`. All shaper DSP belongs here
- `phu-audio-lib/audio/AudioSampleFifo.h` — lock-free audio-to-UI sample transport (fetched dependency)
- `src/PluginProcessor.*` — JUCE `AudioProcessor` + APVTS, owns DSP chain and FIFOs
- `src/PluginEditor.*` — UI layout (stack of `LayoutSection`s), timer-driven updates, parameter attachments
- `src/SpectrumDisplay.*` — spectrum, Bark overlays and contour display
- `src/TransientShaperStage.*`, `src/TransientShaperPanel.*`, `src/TransientShaperUI.h` — optional shaper (processor side, UI panel, editor facade). Keep all JUCE/APVTS/UI shaper code in these `src/` files (DSP goes in `lib/audio/TransientShaper.h`). CMake always defines `PHU_TRANSIENT_SHAPER` as `1` (when `-DTRANSIENT_SHAPER=ON`) or `0`; use only that macro in C++ guards. The only `#if PHU_TRANSIENT_SHAPER` switches are in `TransientShaperStage.h` and `TransientShaperUI.h`; never add them to `PluginProcessor.*` or `PluginEditor.*`

## Conventions
- C++20
- Keep DSP code in `lib/` header-only and free of JUCE/UI dependencies
- No allocation, locks, or system calls on the audio thread
- Use lock-free audio/UI communication only
- Use JUCE APVTS for parameter state; audio-thread reads should stay lock-free
