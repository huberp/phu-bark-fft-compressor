#pragma once

/**
 * Processor-side Transient Shaper feature.
 *
 * PHU_TRANSIENT_SHAPER is set by CMake (option TRANSIENT_SHAPER, default OFF). This header is
 * the only place on the processor side that switches between the real stage and a no-op
 * stand-in with the same API, so PluginProcessor can call it unconditionally without #ifdefs.
 */

#ifndef PHU_TRANSIENT_SHAPER
#error "PHU_TRANSIENT_SHAPER must be defined to 0 or 1 (set by CMake option TRANSIENT_SHAPER)."
#endif

#include <atomic>
#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#if PHU_TRANSIENT_SHAPER

#include "audio/TransientShaper.h"

/**
 * TransientShaperStage — APVTS-driven wrapper around phu::audio::TransientShaper.
 *
 * Owns the ts_* parameter ids and definitions, caches the raw parameter pointers and applies
 * them to the shaper. Definitions live in TransientShaperStage.cpp, which is only compiled
 * when TRANSIENT_SHAPER=ON.
 */
class TransientShaperStage {
  public:
    static constexpr const char* PARAM_TS_ATTACK      = "ts_attack_db";
    static constexpr const char* PARAM_TS_SUSTAIN     = "ts_sustain_db";
    static constexpr const char* PARAM_TS_SENSITIVITY = "ts_sensitivity";
    static constexpr const char* PARAM_TS_BYPASS      = "ts_bypass";

    /** Append the four ts_* parameters to the plugin's parameter layout. */
    static void addParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params);

    /** Cache raw parameter pointers. Call once after the APVTS is constructed. */
    void cacheParams(juce::AudioProcessorValueTreeState& apvts);

    /** Prepare the shaper for playback and apply current parameters. Call from prepareToPlay. */
    void prepare(double sampleRate);

    /** Push the current parameter values to the shaper. Lock-free; call once per block. */
    void updateParams();

    /** Shape one stereo sample pair in place. Audio thread; no allocation or locking. */
    void processSample(float& left, float& right) {
        // One shaper instance serves both channels, as before the feature was made optional.
        left  = m_shaper.processSample(left);
        right = m_shaper.processSample(right);
    }

  private:
    std::atomic<float>* m_attackParam      = nullptr;
    std::atomic<float>* m_sustainParam     = nullptr;
    std::atomic<float>* m_sensitivityParam = nullptr;
    std::atomic<float>* m_bypassParam      = nullptr;

    phu::audio::TransientShaper m_shaper;
};

#else

/** TransientShaperStage — no-op stand-in used when TRANSIENT_SHAPER=OFF; compiles away entirely. */
class TransientShaperStage {
  public:
    static void addParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>&) {}

    void cacheParams(juce::AudioProcessorValueTreeState&) {}
    void prepare(double) {}
    void updateParams() {}
    void processSample(float&, float&) {}
};

#endif
