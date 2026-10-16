#pragma once
#include <vector>
#include <cmath>

namespace phu::audio {

/** Rectangular-window RMS meter; block-size-independent.
 *
 *  Maintains a circular buffer of x² values and a double-precision running sum
 *  so each processSample() call is O(1) regardless of how many samples the DAW
 *  delivers per processBlock() call.
 *
 *  All heap allocation happens in prepare(); processSample() is alloc-free and
 *  lock-free — safe on the audio thread.
 */
class WindowedRMS {
public:
    /** Resize and zero the internal buffer.  Call only from prepareToPlay(). */
    void prepare(double sampleRate, float windowMs) {
        m_windowSamples = static_cast<int>(
            std::ceil(sampleRate * static_cast<double>(windowMs) / 1000.0));
        m_buf.assign(static_cast<std::size_t>(m_windowSamples), 0.0f);
        m_runningSum    = 0.0;
        m_writePos      = 0;
    }

    /** Push one sample; returns the current window RMS (linear, ≥ 0). */
    [[nodiscard]] float processSample(float x) noexcept {
        if (m_windowSamples <= 0) return 0.0f;
        advance(static_cast<double>(x) * static_cast<double>(x));
        return toRms();
    }

    /** Push a block of samples; returns RMS after the last sample.
     *  Avoids a sqrt per intermediate sample — prefer this from processBlock(). */
    [[nodiscard]] float processSamples(const float* data, int numSamples) noexcept {
        if (m_windowSamples <= 0 || numSamples <= 0) return 0.0f;
        for (int i = 0; i < numSamples; ++i)
            advance(static_cast<double>(data[i]) * static_cast<double>(data[i]));
        return toRms();
    }

    void reset() noexcept {
        std::fill(m_buf.begin(), m_buf.end(), 0.0f);
        m_runningSum = 0.0;
        m_writePos   = 0;
    }

private:
    void advance(double newSq) noexcept {
        m_runningSum += newSq - static_cast<double>(m_buf[m_writePos]);
        m_buf[m_writePos] = static_cast<float>(newSq);
        if (++m_writePos >= m_windowSamples)
            m_writePos = 0;
    }

    // Guard against tiny negatives from floating-point cancellation.
    [[nodiscard]] float toRms() const noexcept {
        return (m_runningSum > 0.0)
            ? std::sqrt(static_cast<float>(m_runningSum / m_windowSamples))
            : 0.0f;
    }

    std::vector<float> m_buf;            // circular buffer of x² values
    double             m_runningSum = 0.0; // double limits summation drift
    int                m_windowSamples = 0;
    int                m_writePos      = 0;
};

} // namespace phu::audio
