cmake_minimum_required(VERSION 3.15)
project(phu-bark-fft-compressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE configuration
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Build JUCE Extras")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Build JUCE Examples")

set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE root")
if(NOT EXISTS "${JUCE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE submodule not found. Run: git submodule update --init --recursive")
endif()
add_subdirectory("${JUCE_ROOT}")

set(PHU_AUDIO_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/phu-audio-lib" CACHE PATH "Path to PhuAudioLib root")
if(NOT EXISTS "${PHU_AUDIO_LIB_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "PhuAudioLib submodule not found. Run: git submodule update --init --recursive")
endif()
add_subdirectory("${PHU_AUDIO_LIB_ROOT}" EXCLUDE_FROM_ALL)

add_subdirectory(lib)
add_subdirectory(src)
