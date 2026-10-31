# Contributing to PHU Bark FFT Compressor

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++20/JUCE code style
3. Keep to the conventions in [`.github/copilot-instructions.md`](.github/copilot-instructions.md) — in particular: **no memory allocation, system calls, or locks on the audio thread**
4. Verify the project builds (default configuration **and** `-DTRANSIENT_SHAPER=ON`) and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-bark-fft-compressor/issues).

---

## Optional Features

### Transient Shaper (`TRANSIENT_SHAPER`, default `OFF`)

The Transient Shaper is a compile-time option. Configure with `-DTRANSIENT_SHAPER=ON` to include it. When `OFF`, no shaper DSP, `ts_*` parameters or UI group are compiled in, and the editor is 150 px shorter. When `ON`, the editor height adjusts automatically.

CMake always defines `PHU_TRANSIENT_SHAPER` as `0` or `1` for the plugin target. To keep `#if` blocks out of the processor and editor, all shaper code is grouped in dedicated files:

| File | Role |
|---|---|
| `src/TransientShaperStage.h/cpp` | Processor side: `ts_*` parameter ids and definitions, parameter caching, per-sample processing. The header switches between the real class and a no-op stand-in. |
| `src/TransientShaperPanel.h/cpp` | Editor side: the parameter group (sliders, toggle, attachments) and its layout. |
| `src/TransientShaperUI.h` | Editor facade: adds the panel and its layout section, or does nothing when `OFF`. |
| `lib/audio/TransientShaper.h` | The DSP itself (header-only, no plugin dependencies). |

The `.cpp` files and `TransientShaper.h` are only added to the build when the option is `ON`. If you add shaper-related code, put it in these files rather than adding `#if PHU_TRANSIENT_SHAPER` to `PluginProcessor.*` or `PluginEditor.*`.

---

## Project Layout

```
phu-bark-fft-compressor/
├── CMakeLists.txt / CMakePresets.json
├── cmake/
│   ├── PhuDependencies.cmake        Fetches JUCE and phu-audio-lib (FetchContent)
│   ├── ConfigureIntelMKL.cmake      Optional Intel MKL FFT acceleration
│   ├── compiler_flags.cmake
│   └── juce_config.cmake
├── doc/                             Screenshots
├── JUCE/                            JUCE 8.0.15, fetched by CMake (git-ignored)
├── phu-audio-lib/                   Shared audio library, fetched by CMake
│   └── audio/
│       ├── AudioSampleFifo.h        Lock-free audio→UI sample transport
│       └── FFTProcessor.h           UI-thread FFT for spectrum display
├── src/
│   ├── PluginProcessor.h/cpp        processBlock, APVTS, FIFOs
│   ├── PluginEditor.h/cpp           UI layout, 60 Hz timer, control wiring
│   ├── EditorLayout.h               Shared layout metrics, LayoutSection, row helper
│   ├── SpectrumDisplay.h/cpp        Spectrum + Bark overlay
│   ├── LevelMeterComponent.h        Stereo input/output level meters
│   ├── TransientShaperStage.h/cpp   Optional shaper — processor side (TRANSIENT_SHAPER=ON)
│   ├── TransientShaperPanel.h/cpp   Optional shaper — parameter group UI (TRANSIENT_SHAPER=ON)
│   ├── TransientShaperUI.h          Optional shaper — editor facade
│   └── CMakeLists.txt
├── lib/
│   ├── CMakeLists.txt               BarkCompressorLib (header-only)
│   └── audio/
│       ├── BarkFFTCompressor.h      FFT-based Bark-band compressor
│       ├── EqualLoudnessContour.h   ISO 226 equal-loudness contours
│       ├── OLAWindow.h              Overlap-add window
│       ├── WindowedRMS.h            RMS integration window for the level meters
│       └── TransientShaper.h        Dual-envelope transient processor (TRANSIENT_SHAPER=ON only)
├── .github/workflows/               CI build + pluginval + release workflows
└── scripts/
    ├── find-cmake.ps1               Locates CMake on Windows
    └── install-linux-deps.sh        Installs JUCE Linux dependencies
```
