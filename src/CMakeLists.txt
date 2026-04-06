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

target_compile_definitions(phu-bark-fft-compressor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CAMERA=0
        JUCE_USE_MP3AUDIOFORMAT=0
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_FLAC=0
)

if(WIN32)
    target_compile_definitions(phu-bark-fft-compressor PUBLIC NOMINMAX)
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
