juce_add_plugin(phu-bark-fft-compressor
    PLUGIN_MANUFACTURER_CODE Phub
    PLUGIN_CODE Pbfc
    FORMATS VST3
    PRODUCT_NAME "PHU BARK FFT COMPRESSOR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(phu-bark-fft-compressor PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    SpectrumDisplay.cpp
    SpectrumDisplay.h
)

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
