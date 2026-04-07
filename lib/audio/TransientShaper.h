#pragma once

#include <cmath>

namespace phu {
namespace audio {

/**
 * TransientShaper — envelope-follower-based transient processor.
 *
 * Uses two envelope followers (fast and slow) to detect transients:
 *   - When the fast envelope significantly exceeds the slow envelope
 *     (by a ratio controlled by Sensitivity), the signal is in "attack" phase.
 *   - Otherwise it is in "sustain" phase.
 *
 * Independent dB gains are applied for each phase, allowing boost or
 * cut of attacks and/or sustain. A bypass flag makes the processor
 * fully transparent.
 *
 * Header-only, no plugin/UI dependencies.
 */
class TransientShaper {
  public:
    TransientShaper() = default;

    /**
     * Prepare for playback.
     * Must be called from prepareToPlay before any processing.
     */
    void prepare(double sampleRate) {
        currentSampleRate = sampleRate;

        // Fast envelope: short attack (~2 ms) and release (~10 ms) tracks peaks
        fastAttackCoeff   = computeCoeff(0.002, sampleRate);
        fastReleaseCoeff  = computeCoeff(0.010, sampleRate);

        // Slow envelope: longer attack (~50 ms) and release (~200 ms) tracks body
        slowAttackCoeff   = computeCoeff(0.050, sampleRate);
        slowReleaseCoeff  = computeCoeff(0.200, sampleRate);

        reset();
    }

    /** Reset envelope follower state. */
    void reset() {
        fastEnv = 0.0f;
        slowEnv = 0.0f;
    }

    /**
     * Update shaper parameters. Safe to call from the audio thread.
     *
     * @param attackDb   Gain (dB) applied during detected transient attacks (-24..+24 dB).
     * @param sustainDb  Gain (dB) applied during sustained portions (-24..+24 dB).
     * @param sensitivity Transient detection sensitivity (0..100 %).
     * @param bypass     When true the processor is fully transparent.
     */
    void setParameters(float attackDb, float sustainDb, float sensitivity, bool bypass) {
        attackGainDb  = attackDb;
        sustainGainDb = sustainDb;
        // Map sensitivity (0-100%) to a detection ratio threshold.
        // sensitivity=100% → threshold=1.01 (detect almost any fast peak)
        // sensitivity=0%   → threshold=10.0 (only strong transients detected)
        const float s = sensitivity * 0.01f; // normalise to 0..1
        detectionThreshold = 1.0f + (1.0f - s) * 9.0f;
        isBypassed = bypass;
    }

    /**
     * Process a single sample.
     * Must be called sample-by-sample (same as BarkFFTCompressor::processSample).
     */
    float processSample(float input) {
        if (isBypassed)
            return input;

        const float absInput = std::abs(input);

        // Update fast envelope (tracks rapid peaks)
        if (absInput > fastEnv)
            fastEnv = fastEnv * fastAttackCoeff  + absInput * (1.0f - fastAttackCoeff);
        else
            fastEnv = fastEnv * fastReleaseCoeff + absInput * (1.0f - fastReleaseCoeff);

        // Update slow envelope (tracks sustained body)
        if (absInput > slowEnv)
            slowEnv = slowEnv * slowAttackCoeff  + absInput * (1.0f - slowAttackCoeff);
        else
            slowEnv = slowEnv * slowReleaseCoeff + absInput * (1.0f - slowReleaseCoeff);

        // Transient detection: fast envelope significantly exceeds slow envelope
        const float ratio = (slowEnv > 1e-10f) ? (fastEnv / slowEnv) : 1.0f;
        const bool isTransient = (ratio > detectionThreshold);

        const float gainDb = isTransient ? attackGainDb : sustainGainDb;
        const float gainLinear = std::pow(10.0f, gainDb / 20.0f);

        return input * gainLinear;
    }

  private:
    /** Compute one-pole smoothing coefficient for the given time constant (seconds). */
    static float computeCoeff(double timeSec, double sampleRate) {
        if (timeSec <= 0.0 || sampleRate <= 0.0)
            return 0.0f;
        return static_cast<float>(std::exp(-1.0 / (timeSec * sampleRate)));
    }

    double currentSampleRate = 44100.0;

    // Parameters (updated via setParameters)
    float attackGainDb       = 0.0f;
    float sustainGainDb      = 0.0f;
    float detectionThreshold = 5.5f; // corresponds to sensitivity=50%
    bool  isBypassed         = false;

    // Envelope follower coefficients (computed in prepare())
    float fastAttackCoeff    = 0.0f;
    float fastReleaseCoeff   = 0.0f;
    float slowAttackCoeff    = 0.0f;
    float slowReleaseCoeff   = 0.0f;

    // Envelope follower state
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
};

} // namespace audio
} // namespace phu
