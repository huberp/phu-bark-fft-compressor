add_library(BarkCompressorLib INTERFACE)

target_include_directories(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(BarkCompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BarkFFTCompressor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/TransientShaper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/OLAWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/EqualLoudnessContour.h
)

target_link_libraries(BarkCompressorLib INTERFACE PhuAudioLib)
