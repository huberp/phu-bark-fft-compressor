add_library(BarkCompressorLib INTERFACE)

target_include_directories(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/AudioSampleFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/FFTProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BarkFFTCompressor.h
)
