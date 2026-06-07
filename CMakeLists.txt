cmake_minimum_required(VERSION 3.15)
project(phu-bark-fft-compressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Intel MKL configuration (optional)
# Works with either:
# 1) NuGet package layout: .../intelmkl.static.win-x64/<version>/build/native
# 2) oneAPI layout:        .../oneapi/mkl/latest
option(USE_INTEL_MKL "Use Intel MKL for FFT acceleration" ON)
set(MKLROOT "" CACHE PATH "Path to MKL root (NuGet build/native or oneAPI mkl root)")

if(USE_INTEL_MKL)
    message(STATUS "Searching for Intel MKL...")
    set(_mkl_root "${MKLROOT}")
    if(NOT _mkl_root)
        set(_mkl_root "$ENV{MKLROOT}")
    endif()

    if(NOT _mkl_root AND WIN32 AND DEFINED ENV{USERPROFILE})
        file(GLOB _nuget_mkl_roots "$ENV{USERPROFILE}/.nuget/packages/intelmkl.static.win-x64/*/build/native")
        list(SORT _nuget_mkl_roots COMPARE NATURAL ORDER DESCENDING)
        list(LENGTH _nuget_mkl_roots _nuget_roots_len)
        if(_nuget_roots_len GREATER 0)
            list(GET _nuget_mkl_roots 0 _mkl_root)
            message(STATUS "Using MKL from local NuGet cache: ${_mkl_root}")
        endif()
    endif()

    if(_mkl_root)
        file(TO_CMAKE_PATH "${_mkl_root}" _mkl_root)
    endif()

    if(WIN32)
        set(_mkl_include_dir "${_mkl_root}/include")
        set(_mkl_lib_dir "${_mkl_root}/win-x64")

        set(_mkl_required_libs
            "${_mkl_lib_dir}/mkl_intel_lp64.lib"
            "${_mkl_lib_dir}/mkl_sequential.lib"
            "${_mkl_lib_dir}/mkl_core.lib"
        )
    else()
        set(_mkl_include_dir "${_mkl_root}/include")
        set(_mkl_lib_dir "${_mkl_root}/lib/intel64")

        set(_mkl_required_libs
            "${_mkl_lib_dir}/libmkl_intel_lp64.a"
            "${_mkl_lib_dir}/libmkl_sequential.a"
            "${_mkl_lib_dir}/libmkl_core.a"
        )
    endif()

    set(_mkl_all_present TRUE)
    if(NOT EXISTS "${_mkl_include_dir}/mkl.h")
        set(_mkl_all_present FALSE)
    endif()
    foreach(_mkl_lib IN LISTS _mkl_required_libs)
        if(NOT EXISTS "${_mkl_lib}")
            set(_mkl_all_present FALSE)
        endif()
    endforeach()

    if(_mkl_all_present)
        add_library(PHU_MKL INTERFACE)
        target_include_directories(PHU_MKL INTERFACE "${_mkl_include_dir}")
        target_link_libraries(PHU_MKL INTERFACE ${_mkl_required_libs})

        set(MKL_FOUND TRUE)
        set(MKLROOT "${_mkl_root}" CACHE PATH "Path to MKL root (NuGet build/native or oneAPI mkl root)" FORCE)
        message(STATUS "Intel MKL found at: ${_mkl_root}")
        message(STATUS "Intel MKL static libs: ${_mkl_required_libs}")
    else()
        set(MKL_FOUND FALSE)
        message(STATUS "Intel MKL not found or incomplete (set MKLROOT to enable). FFT will use JUCE fallback.")
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
