add_library(BarkCompressorLib INTERFACE)

target_include_directories(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BarkFFTCompressor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/TransientShaper.h
)

target_link_libraries(BarkCompressorLib INTERFACE PhuAudioLib)
