#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "memory/AlignedAllocator.h"

namespace phu {
namespace audio {

/**
 * OLAWindow — Overlap-Add window generator and applicator with SIMD acceleration.
 *
 * Handles window coefficient generation and application for FFT-based processing
 * with arbitrary overlap factors. Uses JUCE FloatVectorOperations for SIMD.
 *
 * Supports split-at-wrap ring buffer access via numSamples parameter in accumulate functions.
 *
 * Usage:
 *   OLAWindow window(2048, 0.5f);  // 2048-point FFT, 50% overlap
 *   window.applyForFFTInPlace(input);  // Apply analysis window (SIMD)
 *   window.applyAndAccumulate(ifftOutput, olaBuffer, numSamples);  // OLA synthesis (SIMD)
 *
 * COLA (Constant Overlap-Add) property:
 *   Periodic Hann window satisfies COLA at any hop size. The scale factor
 *   compensates for the window's overlap sum to maintain unity gain:
 *     - 50% overlap (hop = N/2): sum = 1.0 → scale = 1.0
 *     - 75% overlap (hop = N/4): sum = 2.0 → scale = 0.5
 *     - 90% overlap (hop = N/10): sum = 10.0 → scale = 0.1
 *
 * Header-only, requires juce_dsp for FloatVectorOperations and AlignedAllocator.
 */
class OLAWindow {
  public:
    /** Window function type. */
    enum class WindowType {
        Hann,       // Periodic Hann (raised cosine), COLA-compliant at any hop
        // Future: Hamming, Blackman-Harris, etc.
    };

    /**
     * Construct an OLA window.
     * @param fftSize      FFT size (must be power of 2, >= 64)
     * @param overlapFactor Overlap as a fraction in [0.0, 1.0).
     *                      0.5 = 50% overlap (hop = N/2),
     *                      0.75 = 75% overlap (hop = N/4),
     *                      0.9 = 90% overlap (hop = N/10).
     * @param type         Window function type (default: Hann)
     */
    OLAWindow(int fftSize, float overlapFactor, WindowType type = WindowType::Hann)
        : m_fftSize(fftSize)
        , m_overlapFactor(overlapFactor)
        , m_windowType(type)
    {
        jassert(fftSize >= 64 && (fftSize & (fftSize - 1)) == 0); // power of 2
        jassert(overlapFactor >= 0.0f && overlapFactor < 1.0f);

        // Compute hop size and COLA scale factor
        m_hopSize = static_cast<int>(static_cast<float>(fftSize) * (1.0f - overlapFactor));
        m_hopSize = std::max(1, m_hopSize);

        computeWindow();
        computeScaleFactor();
        
        // Verify alignment (critical for SIMD performance)
        jassert(reinterpret_cast<uintptr_t>(m_windowCoeffs.data()) % 32 == 0);
    }

    /** Get FFT size. */
    int getFFTSize() const { return m_fftSize; }

    /** Get hop size (samples between successive FFT frames). */
    int getHopSize() const { return m_hopSize; }

    /**
     * Get COLA normalization scale factor.
     * Apply this to IFFT output during OLA accumulation to maintain unity gain.
     */
    float getScaleFactor() const { return m_scaleFactor; }

    /** Get overlap factor. */
    float getOverlapFactor() const { return m_overlapFactor; }

    /** Get window type. */
    WindowType getWindowType() const { return m_windowType; }

    /** Direct access to window coefficients (read-only). */
    const float* getWindowCoefficients() const { return m_windowCoeffs.data(); }

    /**
     * Apply analysis window for FFT.
     * output[i] = input[i] * window[i]
     *
     * Uses JUCE FloatVectorOperations::multiply() for SIMD acceleration.
     *
     * @param input  Source samples (must be aligned, length = FFT size)
     * @param output Destination (must be aligned, length = FFT size)
     */
    void applyForFFT(const float* input, float* output) const {
        juce::FloatVectorOperations::multiply(output, input, m_windowCoeffs.data(), m_fftSize);
    }

    /**
     * Apply analysis window in-place for FFT.
     * buffer[i] *= window[i]
     *
     * Uses JUCE FloatVectorOperations for SIMD. Handles misaligned buffers.
     *
     * @param buffer Buffer to window in-place (length = FFT size)
     */
    void applyForFFTInPlace(float* buffer) const {
        juce::FloatVectorOperations::multiply(buffer, m_windowCoeffs.data(), m_fftSize);
    }

    /**
     * Apply COLA scale and accumulate for OLA synthesis (no synthesis window).
     * output[i] += input[i] * scaleFactor
     *
     * Uses JUCE FloatVectorOperations::addWithMultiply() for SIMD acceleration.
     *
     * This is the standard OLA accumulation step after IFFT when using
     * analysis-only windowing (no synthesis window applied).
     *
     * @param input      IFFT output (must be aligned, length >= numSamples)
     * @param output     OLA accumulation buffer (must be aligned, length >= numSamples)
     * @param numSamples Number of samples to accumulate (for split-at-wrap: can be < FFT size)
     */
    void applyAndAccumulate(const float* input, float* output, int numSamples) const {
        juce::FloatVectorOperations::addWithMultiply(output, input, m_scaleFactor, numSamples);
    }

    /**
     * Apply synthesis window with COLA scale and accumulate.
     * output[i] += input[i] * window[i] * scaleFactor
     *
     * Uses JUCE FloatVectorOperations for SIMD. Requires a temporary buffer
     * for the windowed intermediate result.
     *
     * Use this when you want to apply a synthesis window after IFFT
     * (analysis-synthesis windowing). For analysis-only windowing (more common),
     * use applyAndAccumulate() instead.
     *
     * @param input      IFFT output (must be aligned, length >= numSamples)
     * @param output     OLA accumulation buffer (must be aligned, length >= numSamples)
     * @param temp       Temporary buffer (must be aligned, length >= numSamples)
     * @param numSamples Number of samples to process (for split-at-wrap: can be < FFT size)
     */
    void applyWindowAndAccumulate(const float* input, float* output, float* temp, int numSamples) const {
        // temp = input * window (only process numSamples worth)
        juce::FloatVectorOperations::multiply(temp, input, m_windowCoeffs.data(), numSamples);
        // output += temp * scale
        juce::FloatVectorOperations::addWithMultiply(output, temp, m_scaleFactor, numSamples);
    }

  private:
    static constexpr float kPi = 3.14159265358979323846f;

    /** Generate window coefficients based on window type. */
    void computeWindow() {
        m_windowCoeffs.assign(m_fftSize, 0.0f);

        switch (m_windowType) {
            case WindowType::Hann:
                computeHannWindow();
                break;
            // Future window types here
        }
    }

    /**
     * Compute periodic Hann window.
     * w[n] = 0.5 * (1 - cos(2π*n/N)), n = 0..N-1
     *
     * The periodic form (denominator = N, not N-1) satisfies the COLA condition
     * for overlap-add reconstruction at any integer hop size.
     */
    void computeHannWindow() {
        const float N = static_cast<float>(m_fftSize);
        for (int i = 0; i < m_fftSize; ++i) {
            m_windowCoeffs[i] = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) / N));
        }
    }

    /**
     * Compute COLA scale factor.
     *
     * For a periodic Hann window, the overlap-add sum at hop H is:
     *   S = Σ w[n + kH] over k
     *
     * The scale factor is chosen so that S * scale = 1.0 (unity gain).
     *
     * For Hann at 50% overlap (H = N/2): S ≈ 1.0 → scale = 1.0
     * For Hann at 75% overlap (H = N/4): S ≈ 2.0 → scale = 0.5
     * For Hann at 90% overlap (H = N/10): S ≈ 10.0 → scale = 0.1
     *
     * General formula: scale ≈ (1 - overlap) for Hann window.
     */
    void computeScaleFactor() {
        // For periodic Hann, the COLA sum is approximately 1.0 / (1 - overlap)
        // So the scale factor is (1 - overlap) to normalize back to unity gain.
        m_scaleFactor = 1.0f - m_overlapFactor;

        // For very high overlap (>87.5%), clamp to avoid tiny scale factors
        // that could amplify quantization noise.
        if (m_scaleFactor < 0.125f) {
            m_scaleFactor = 0.125f;
        }

        // Sanity check
        jassert(m_scaleFactor > 0.0f && m_scaleFactor <= 1.0f);
    }

    int m_fftSize;
    int m_hopSize;
    float m_overlapFactor;
    float m_scaleFactor;
    WindowType m_windowType;
    phu::memory::AlignedVector<float> m_windowCoeffs;
};

} // namespace audio
} // namespace phu