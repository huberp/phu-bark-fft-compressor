cmake_minimum_required(VERSION 3.15)
project(phu-bark-fft-compressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Intel MKL configuration (optional)
# Works with either:
# 1) NuGet package layout: .../intelmkl.static.win-x64/<version>/build/native
# 2) oneAPI layout:        .../oneapi/mkl/latest
option(USE_INTEL_MKL "Use Intel MKL for FFT acceleration" ON)
set(MKLROOT "" CACHE PATH "Path to MKL root (NuGet build/native or oneAPI mkl root)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ConfigureIntelMKL)

if(USE_INTEL_MKL)
    configure_intel_mkl(PHU_MKL)
endif()

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
