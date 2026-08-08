cmake_minimum_required(VERSION 3.31.6)
project(phu-bark-fft-compressor VERSION 0.1.0 LANGUAGES C CXX)

# =============================================================================
# Top-Level Guard
# =============================================================================
if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

# =============================================================================
# Global Settings
# =============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/compiler_flags.cmake)

# =============================================================================
# Platform-Specific Settings
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJC OBJCXX)
endif()

# =============================================================================
# Standard Settings
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Intel MKL Configuration (optional, bark-specific)
# Works with either:
# 1) NuGet package layout: .../intelmkl.static.win-x64/<version>/build/native
# 2) oneAPI layout:        .../oneapi/mkl/latest
# =============================================================================
option(USE_INTEL_MKL "Use Intel MKL for FFT acceleration" ON)
set(MKLROOT "" CACHE PATH "Path to MKL root (NuGet build/native or oneAPI mkl root)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ConfigureIntelMKL)

if(USE_INTEL_MKL)
    configure_intel_mkl(PHU_MKL)
endif()

# =============================================================================
# Dependency Validation
# =============================================================================
foreach(SUBMODULE IN ITEMS JUCE phu-audio-lib)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBMODULE}/CMakeLists.txt")
        message(FATAL_ERROR
            "${SUBMODULE} submodule not initialized. "
            "Run: git submodule update --init --recursive"
        )
    endif()
endforeach()

# =============================================================================
# JUCE & Dependencies
# =============================================================================
include(cmake/juce_config.cmake)
add_subdirectory(JUCE)
add_subdirectory(phu-audio-lib EXCLUDE_FROM_ALL)

add_subdirectory(lib)
add_subdirectory(src)
