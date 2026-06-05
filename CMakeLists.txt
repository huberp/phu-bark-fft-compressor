cmake_minimum_required(VERSION 3.15)
project(phu-bark-fft-compressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Intel MKL configuration (optional)
# Set MKLROOT and, when needed, MKL_DIR to enable MKL FFT support
option(USE_INTEL_MKL "Use Intel MKL for FFT acceleration" ON)

if(USE_INTEL_MKL)
    message(STATUS "Searching for Intel MKL...")
    set(MKL_LINK "static")
    # Use sequential threading to avoid runtime dependency on threading libraries
    # This prevents the plugin from requiring libopenmp5 on target systems
    set(MKL_THREADING "sequential")

    set(_mkl_hints)
    foreach(_mkl_hint IN ITEMS
        "$ENV{MKL_DIR}"
        "$ENV{MKLROOT}"
        "$ENV{MKLROOT}/lib/cmake/mkl"
        "$ENV{MKLROOT}/lib/native/lib/cmake/mkl"
    )
        if(_mkl_hint)
            list(APPEND _mkl_hints "${_mkl_hint}")
        endif()
    endforeach()

    find_package(MKL CONFIG HINTS ${_mkl_hints})
    
    if(MKL_FOUND)
        message(STATUS "Intel MKL found: ${MKL_IMPORTED_TARGETS}")
        message(STATUS "Intel MKL link mode: ${MKL_LINK}, threading: ${MKL_THREADING}")
    else()
        message(STATUS "Intel MKL not found - FFT will use JUCE's fallback implementation")
        set(USE_INTEL_MKL OFF)
    endif()
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
