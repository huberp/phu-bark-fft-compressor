# Semicolon-separated JUCE formats; set by the "formats-*" presets in CMakePresets.json.
set(PHU_PLUGIN_FORMATS "VST3" CACHE STRING "JUCE plugin formats to build (e.g. \"VST3;AU\")")

juce_add_plugin(phu-bark-fft-compressor
    PLUGIN_MANUFACTURER_CODE Phub
    PLUGIN_CODE Pbfc
    FORMATS ${PHU_PLUGIN_FORMATS}
    PRODUCT_NAME "PHU BARK FFT COMPRESSOR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(phu-bark-fft-compressor PRIVATE
    EditorLayout.h
    LevelMeterComponent.h
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    SpectrumDisplay.cpp
    SpectrumDisplay.h
    TransientShaperStage.h
    TransientShaperUI.h
)

# Optional Transient Shaper (option TRANSIENT_SHAPER, default OFF). When OFF, none of the
# shaper sources are compiled; the facade headers fall back to no-op stand-ins.
if(TRANSIENT_SHAPER)
    target_sources(phu-bark-fft-compressor PRIVATE
        TransientShaperStage.cpp
        TransientShaperPanel.cpp
        TransientShaperPanel.h
    )
    target_compile_definitions(phu-bark-fft-compressor PRIVATE PHU_TRANSIENT_SHAPER=1)
    message(STATUS "Transient Shaper: ON")
else()
    target_compile_definitions(phu-bark-fft-compressor PRIVATE PHU_TRANSIENT_SHAPER=0)
    message(STATUS "Transient Shaper: OFF")
endif()

target_link_libraries(phu-bark-fft-compressor
    PRIVATE
        BarkCompressorLib
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Intel MKL support
if(USE_INTEL_MKL AND MKL_FOUND)
    target_compile_definitions(phu-bark-fft-compressor PRIVATE JUCE_DSP_USE_INTEL_MKL=1)
    target_link_libraries(phu-bark-fft-compressor PRIVATE PHU_MKL PUBLIC juce::juce_recommended_lto_flags)
    message(STATUS "Intel MKL FFT support enabled for phu-bark-fft-compressor")
endif()
