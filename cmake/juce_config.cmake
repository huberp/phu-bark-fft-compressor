# =============================================================================
# JUCE Configuration
# =============================================================================

# --- JUCE Feature Flags ---
# Disable features not needed by this project. This is the single place for JUCE module flags.
# JUCE compiles its module sources inside the consuming targets, so these must be plain
# compile definitions (JUCE's CMake support does not read them as CMake variables).
# Codec flags (FLAC/Ogg/Opus/WebP) only turn the separate JUCE 9 codec .c files into empty
# translation units; CMake still lists them because it globs every module source file.
add_compile_definitions(
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CAMERA=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_FLAC=0
    JUCE_USE_OPUS=0
    JUCE_USE_WEBP=0
)

# --- JUCE Build Options ---
# Disable JUCE extras and examples for faster builds
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Build JUCE Extras")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Build JUCE Examples")

# --- Platform-Specific JUCE Flags ---
if(WIN32)
    # Prevent Windows.h from defining min/max macros
    add_compile_definitions(NOMINMAX)
endif()
