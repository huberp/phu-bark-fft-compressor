#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "memory/AlignedAllocator.h"
#include "EqualLoudnessContour.h"
#include "OLAWindow.h"

namespace phu {
namespace audio {

/**
 * BarkFFTCompressor — FFT-based compressor operating on 24 Bark bands.
 *
 * Signal path (per overlap-add frame):
 *   1. Window input with Hann window
 *   2. Forward FFT
 *   3. Compute power per Bark band
 *   4. Compare band energy to equal-loudness contour (ISO 226)
 *   5. Apply per-band gain reduction (threshold/ratio/attack/release)
 *   6. Multiply FFT bins by band gain
 *   7. Inverse FFT + overlap-add reconstruction
 *
 * Header-only, no plugin/UI dependencies. Only requires <cmath> + juce_dsp.
 */
class BarkFFTCompressor {
  public:
    static constexpr int NUM_BARK_BANDS = 24;

    // FFT mode: Precision uses a larger FFT (better frequency resolution);
    // Transient uses a smaller FFT (better transient response).
    enum class FFTMode { Precision, Transient };

    // Overlap mode: controls the hop size as a fraction of the FFT window.
    //   Half          → hop = N/2 (50% overlap, lower CPU)
    //   ThreeQuarter  → hop = N/4 (75% overlap, higher quality, 2× CPU)
    //   Ninety        → hop = N/10 (90% overlap, highest quality, 10× CPU)
    enum class OverlapMode { Half, ThreeQuarter, Ninety };

    // Equal-loudness contour presets (re-exported from EqualLoudnessContour)
    using ContourPreset = EqualLoudnessContour::Preset;

    BarkFFTCompressor() {
        // Allocate FFT engine for the default mode (Precision, 44.1 kHz → order 11)
        fft = std::make_unique<juce::dsp::FFT>(11);
    }

    // ── FFT mode ─────────────────────────────────────────────────────────

    /** Set the FFT mode. Call before or after prepare(); effective on next prepare(). */
    void setFFTMode(FFTMode mode) { currentFFTMode = mode; }
    FFTMode getFFTMode() const { return currentFFTMode; }

    /** Set the overlap mode. Call before prepare(); effective on next prepare(). */
    void setOverlapMode(OverlapMode mode) { currentOverlapMode = mode; }
    OverlapMode getOverlapMode() const { return currentOverlapMode; }

    /**
     * Compute the FFT size for a given mode and sample rate.
     *   Precision: 2048 (≤48 kHz), 4096 (>48 kHz)
     *   Transient:  1024 (≤48 kHz), 2048 (>48 kHz)
     */
    static int computeFFTSize(FFTMode mode, float sampleRate) {
        if (mode == FFTMode::Precision)
            return (sampleRate <= 48000.0f) ? 2048 : 4096;
        else
            return (sampleRate <= 48000.0f) ? 1024 : 2048;
    }

    /** Current FFT size (valid after prepare()). */
    int getCurrentFFTSize() const { return currentFFTSize; }

    /**
     * Prepare the compressor for playback.
     * Must be called from prepareToPlay with the current sample rate.
     * Re-initialises all buffers whenever called (e.g. after setFFTMode).
     */
    void prepare(double sampleRate) {
        currentSampleRate = static_cast<float>(sampleRate);

        // Compute runtime FFT parameters from the current mode and sample rate.
        currentFFTSize  = computeFFTSize(currentFFTMode, currentSampleRate);
        currentFFTOrder = 0;
        // Count trailing shifts to find log2(currentFFTSize), e.g. 2048→11, 4096→12.
        { int sz = currentFFTSize; while (sz > 1) { ++currentFFTOrder; sz >>= 1; } }
        currentNumBins  = currentFFTSize / 2;
        
        // Create OLA window with appropriate overlap factor
        float overlapFactor = 0.5f;
        if (currentOverlapMode == OverlapMode::ThreeQuarter)
            overlapFactor = 0.75f;
        else if (currentOverlapMode == OverlapMode::Ninety)
            overlapFactor = 0.9f;
        olaWindow = std::make_unique<OLAWindow>(currentFFTSize, overlapFactor);
        currentHopSize = olaWindow->getHopSize();
        olaScaleFactor = olaWindow->getScaleFactor();

        // Normalise raw FFT power to dBFS (depends on FFT size so must be updated here).
        // JUCE's forward transform is unnormalized: for a 0-dBFS sine with a Hann window of
        // size N the peak bin power is (N/4)^2. Subtracting this offset maps the energy axis
        // so that 0 dBFS → 0 dB, matching the user-visible threshold range (-60..0 dB).
        kFFTNormDb = 20.0f * std::log10(static_cast<float>(currentFFTSize) / 4.0f);

        // (Re-)create the FFT engine if the order changed.
        if (!fft || fft->getSize() != currentFFTSize)
            fft = std::make_unique<juce::dsp::FFT>(currentFFTOrder);

        // Resize all dynamic buffers.
        const int ringSize = currentFFTSize * 2;
        binToBand       .assign(currentNumBins, 0);
        binGains        .assign(currentNumBins, 1.0f);
        binLogFreq      .assign(currentNumBins, 0.0f);
        inputRingMono .assign(ringSize, 0.0f);
        inputRingL    .assign(ringSize, 0.0f);
        inputRingR    .assign(ringSize, 0.0f);
        outputRingL   .assign(ringSize, 0.0f);
        outputRingR   .assign(ringSize, 0.0f);
        fftBuffer     .assign(ringSize, 0.0f);
        fftBufferL    .assign(ringSize, 0.0f);
        fftBufferR    .assign(ringSize, 0.0f);



        // Verify 32-byte alignment of performance-critical buffers (debug only).
        jassert (reinterpret_cast<uintptr_t>(binGains.data())      % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(binLogFreq.data())    % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(inputRingMono.data()) % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(inputRingL.data())    % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(inputRingR.data())    % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(outputRingL.data())   % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(outputRingR.data())   % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(fftBuffer.data())     % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(fftBufferL.data())    % 32 == 0);
        jassert (reinterpret_cast<uintptr_t>(fftBufferR.data())    % 32 == 0);

        // Pre-compute bin-to-Bark mapping
        computeBinToBarkMapping();

        // Update attack/release coefficients
        updateCoefficients();

        // Reset state
        reset();
    }

    /**
     * Reset all internal state (call on transport restart, etc.).
     */
    void reset() {
        std::fill(inputRingMono.begin(), inputRingMono.end(), 0.0f);
        std::fill(inputRingL.begin(),    inputRingL.end(),    0.0f);
        std::fill(inputRingR.begin(),    inputRingR.end(),    0.0f);
        std::fill(outputRingL.begin(),   outputRingL.end(),   0.0f);
        std::fill(outputRingR.begin(),   outputRingR.end(),   0.0f);
        ringWritePos = 0;
        samplesUntilNextFFT = currentHopSize;

        for (int i = 0; i < NUM_BARK_BANDS; ++i) {
            smoothedGainReductionDb[i] = 0.0f;
            currentBandGainReductionDb[i] = 0.0f;
            currentBandEnergyDb[i] = -100.0f;
        }


    }

    // ── Parameter setters ────────────────────────────────────────────────

    void setThresholdDb(float dB)    { thresholdDb = dB; }
    void setRatio(float r)           { ratio = std::max(r, 1.0f); }

    void setAttackMs(float ms) {
        attackMs = ms;
        updateCoefficients();
    }

    void setReleaseMs(float ms) {
        releaseMs = ms;
        updateCoefficients();
    }

    void setContourPreset(ContourPreset preset) {
        contour.setPreset(preset);
    }

    /**
     * Set the per-bin gain smoothing coefficient for the bidirectional one-pole IIR.
     * alpha = 1.0 → no smoothing (identity); alpha approaching 0 → maximum smoothing.
     * Effective transition width ≈ 1/(1-alpha) bins per pass (squared by the bidir pass).
     */
    void setSmoothingAlpha(float alpha) {
        smoothingAlpha = std::max(0.01f, std::min(1.0f, alpha));
    }

    float getSmoothingAlpha() const { return smoothingAlpha; }



    // ── Per-sample processing ────────────────────────────────────────────

    /**
     * Process a single stereo sample pair. Returns the compressed output.
     * Feed samples one at a time from processBlock.
     */
    struct StereoSample {
        float left;
        float right;
    };

    StereoSample processSample(float inputL, float inputR) {
        // Write both channels and their mono mix into the respective ring buffers.
        // The mono mix is used for analysis (gain computation) only; the individual
        // L/R rings are used for synthesis so per-band gains affect ONLY their own
        // frequency band — no cross-band pumping.
        const int ringSize = currentFFTSize * 2;
        inputRingMono[ringWritePos] = (inputL + inputR) * 0.5f;
        inputRingL[ringWritePos]    = inputL;
        inputRingR[ringWritePos]    = inputR;

        // Read the OLA-reconstructed output for this position (written by past frames)
        float outL = outputRingL[ringWritePos];
        float outR = outputRingR[ringWritePos];

        // Clear for the next OLA accumulation cycle
        outputRingL[ringWritePos] = 0.0f;
        outputRingR[ringWritePos] = 0.0f;

        // Advance write position
        ringWritePos = (ringWritePos + 1) % ringSize;

        // Count down to next FFT hop
        --samplesUntilNextFFT;
        if (samplesUntilNextFFT <= 0) {
            processFFTFrame();
            samplesUntilNextFFT = currentHopSize;
        }

        return { outL, outR };
    }

    // ── UI accessors (read from UI thread) ───────────────────────────────

    /** Total number of FFT bins (valid after prepare()). */
    int getNumBins() const { return currentNumBins; }

    /**
     * Per-bin smoothed gain in dB (≤ 0) for visualization.
     * Read from UI thread — floating-point tearing is acceptable for display.
     */
    float getBinGainDb(int bin) const {
        if (bin >= 0 && bin < currentNumBins)
            return 20.0f * std::log10(std::max(binGains[bin], 1e-10f));
        return 0.0f;
    }

    /** Get per-band gain reduction in dB (negative = compression). */
    float getBandGainReductionDb(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS)
            return currentBandGainReductionDb[band];
        return 0.0f;
    }

    /** Get per-band energy in dB (for visualization). */
    float getBandEnergyDb(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS)
            return currentBandEnergyDb[band];
        return -100.0f;
    }

    /** Get the SPL adjustment for a given band in the current contour preset. */
    float getContourAdjustmentDb(int band) const {
        return contour.getAdjustmentDb(band);
    }

    /** Get the center frequency of a Bark band. */
    float getBandCenterFrequency(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS)
            return barkBandCenterFreqs[band];
        return 0.0f;
    }

    /** Get the lower edge frequency of a Bark band. */
    float getBandLowFrequency(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS)
            return barkBandLowFreqs[band];
        return 0.0f;
    }

    /** Get the upper edge frequency of a Bark band. */
    float getBandHighFrequency(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS)
            return barkBandHighFreqs[band];
        return 0.0f;
    }

    /** Get latency in samples (equals the current FFT size). */
    int getLatencySamples() const { return currentFFTSize; }

    /** Get the current contour preset. */
    ContourPreset getContourPreset() const { return contour.getPreset(); }

    /** Get current sample rate. */
    float getSampleRate() const { return currentSampleRate; }

    float getThresholdDb() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttackMs() const { return attackMs; }
    float getReleaseMs() const { return releaseMs; }

    /** Get name for a contour preset. */
    static const char* getContourPresetName(ContourPreset preset) {
        return EqualLoudnessContour::getPresetName(preset);
    }

  private:
    static constexpr float kPi    = 3.14159265358979323846f;
    static constexpr float kTwoPi = 2.0f * kPi;

    // ── Static precomputations ───────────────────────────────────────────

    /** Convert frequency (Hz) to Bark scale. */
    static float freqToBark(float freq) {
        return 13.0f * std::atan(0.00076f * freq)
             + 3.5f * std::atan((freq / 7500.0f) * (freq / 7500.0f));
    }

    /** Convert Bark scale to approximate frequency (Hz). Inverse of freqToBark. */
    static float barkToFreq(float bark) {
        // Newton-Raphson approximation (3 iterations sufficient for display)
        float freq = 600.0f * std::sinh(bark / 6.0f);
        for (int iter = 0; iter < 3; ++iter) {
            float currentBark = freqToBark(freq);
            float error = currentBark - bark;
            // Numerical derivative
            float dBark = freqToBark(freq + 1.0f) - currentBark;
            if (std::abs(dBark) > 1e-10f)
                freq -= error / dBark;
        }
        return std::max(freq, 0.0f);
    }

    /** Pre-compute bin-to-Bark-band assignment. */
    void computeBinToBarkMapping() {
        // Compute Bark band edges: 24 bands from 0 to 24 Bark
        std::array<float, NUM_BARK_BANDS + 1> barkEdges;
        for (int i = 0; i <= NUM_BARK_BANDS; ++i) {
            barkEdges[i] = static_cast<float>(i);
        }

        // Assign each FFT bin to a Bark band
        for (int k = 0; k < currentNumBins; ++k) {
            float binFreq = (static_cast<float>(k) * currentSampleRate) / static_cast<float>(currentFFTSize);
            float bark = freqToBark(binFreq);

            // Find the band this bin belongs to
            int band = static_cast<int>(bark);
            if (band < 0) band = 0;
            if (band >= NUM_BARK_BANDS) band = NUM_BARK_BANDS - 1;

            binToBand[k] = band;
        }

        // Count bins per band
        for (int i = 0; i < NUM_BARK_BANDS; ++i)
            binsPerBand[i] = 0;

        for (int k = 0; k < currentNumBins; ++k) {
            binsPerBand[binToBand[k]]++;
        }

        // Compute band center frequencies and edge frequencies
        for (int i = 0; i < NUM_BARK_BANDS; ++i) {
            float lowBark = static_cast<float>(i);
            float highBark = static_cast<float>(i + 1);
            float centerBark = (lowBark + highBark) * 0.5f;

            barkBandLowFreqs[i] = barkToFreq(lowBark);
            barkBandHighFreqs[i] = barkToFreq(highBark);
            barkBandCenterFreqs[i] = barkToFreq(centerBark);
        }

        // ── Precompute log-frequency lookup tables ────────────────────────
        //
        // Used in processFFTFrame() to interpolate per-bin gains smoothly
        // between adjacent Bark band centres without any transcendental calls
        // on the audio thread.  Both arrays are indexed by bin k.
        //
        //   binLogFreq[k]       — log10 of the bin's centre frequency in Hz
        //   bandCenterLogFreq[i]— log10 of each Bark band's centre frequency
        //
        // Why log-frequency?  The Bark scale (and human pitch perception) is
        // approximately logarithmic in Hz, so linearly interpolating *in dB*
        // between two points that are equally-spaced on a log-Hz axis gives
        // a perceptually smooth spectral envelope — exactly what an analogue
        // multiband processor produces at its crossover transitions.
        binLogFreq.assign(currentNumBins, 0.0f);
        constexpr float kLogFreqFloor = -0.30103f; // log10(0.5 Hz) — floor for DC/sub-Hz bins
        for (int k = 0; k < currentNumBins; ++k) {
            float binFreq = (static_cast<float>(k) * currentSampleRate)
                            / static_cast<float>(currentFFTSize);
            binLogFreq[k] = (binFreq > 0.5f) ? std::log10(binFreq) : kLogFreqFloor;
        }
        for (int i = 0; i < NUM_BARK_BANDS; ++i)
            bandCenterLogFreq[i] = std::log10(std::max(barkBandCenterFreqs[i], 0.5f));
    }



    /** Update attack/release smoothing coefficients from ms values. */
    void updateCoefficients() {
        if (currentSampleRate <= 0.0f) return;

        // Coefficients computed per-hop (currentHopSize samples per frame)
        float hopsPerSecond = currentSampleRate / static_cast<float>(currentHopSize);
        if (hopsPerSecond <= 0.0f) return;

        float attackTimeSec = attackMs * 0.001f;
        float releaseTimeSec = releaseMs * 0.001f;

        // One-pole smoothing: coeff = exp(-1 / (time * hopsPerSecond))
        attackCoeff = (attackTimeSec > 0.0001f)
            ? std::exp(-1.0f / (attackTimeSec * hopsPerSecond))
            : 0.0f;

        releaseCoeff = (releaseTimeSec > 0.0001f)
            ? std::exp(-1.0f / (releaseTimeSec * hopsPerSecond))
            : 0.0f;
    }

    // ── FFT frame processing ─────────────────────────────────────────────

    /** Process one FFT frame: analysis → per-band gain computation → stereo synthesis via OLA.
     *
     * Signal path:
     *   1. Analysis: forward FFT of the mono mix → compute per-band gains.
     *   2. Synthesis L: forward FFT of L → apply per-band gains → IFFT → OLA.
     *   3. Synthesis R: same as step 2 but for the R channel.
     *
     * Gain reduction is computed from the mono mix (sufficient for level-dependent
     * processing) while each channel is synthesised independently.
     */
    void processFFTFrame() {
        // ── Step 1: Analysis ─────────────────────────────────────────────
        // Window + FFT on the mono mix to compute per-band compression gains.
        const int ringSize = currentFFTSize * 2;
        
        // Split-at-wrap ring buffer copy (SIMD-friendly)
        const int startIdx = (ringWritePos - currentFFTSize + ringSize) % ringSize;
        const int firstLen = ringSize - startIdx;
        if (firstLen >= currentFFTSize) {
            // No wrap: single contiguous copy
            juce::FloatVectorOperations::copy(fftBuffer.data(), inputRingMono.data() + startIdx, currentFFTSize);
        } else {
            // Wraps: two segments
            juce::FloatVectorOperations::copy(fftBuffer.data(), inputRingMono.data() + startIdx, firstLen);
            juce::FloatVectorOperations::copy(fftBuffer.data() + firstLen, inputRingMono.data(), currentFFTSize - firstLen);
        }
        
        // Apply Hann window (SIMD)
        olaWindow->applyForFFTInPlace(fftBuffer.data());
        
        // Zero-pad second half for real FFT
        juce::FloatVectorOperations::clear(fftBuffer.data() + currentFFTSize, currentFFTSize);

        fft->performRealOnlyForwardTransform(fftBuffer.data());

        // Accumulate POWER per Bark band - using sqrt here is not necessary.
        // we use power later to convert to dB, so we can save some CPU by skipping the sqrt and working with power directly.
        std::array<float, NUM_BARK_BANDS> barkEnergies{};
        // the following two loops are split on purpose to allow the compiler use simd instructions for the first loop 
        // without worrying about the band accumulation in the second loop.
        // first individually square each number in the fftBuffer, then accumulate into barkEnergies 
        // the fftBuffer contains interleaved real and imaginary parts of the FFT output: [Re(0), Im(0), Re(1), Im(1), ..., Re(N/2-1), Im(N/2-1)]
        for (int k = 0; k < currentNumBins*2; ++k)
        {
            const auto num = fftBuffer[k];
            fftBuffer[k] = num * num;
        }
        // next accumulate the power of each bin into the corresponding Bark band
        for (int k = 0; k < currentNumBins; ++k)
        {   
            const auto power = fftBuffer[k*2] + fftBuffer[k*2+1];
            barkEnergies[binToBand[k]] += power;
        }

        // ── Step 2: Compute per-band gains ───────────────────────────────
        const float* splAdjustments = contour.getAdjustments();

        std::array<float, NUM_BARK_BANDS> bandGainLinear{};
        for (int i = 0; i < NUM_BARK_BANDS; ++i) {
            float normalizedEnergy = (binsPerBand[i] > 0)
                ? barkEnergies[i] / static_cast<float>(binsPerBand[i])
                : 0.0f;

            float energyDb = 10.0f * std::log10(normalizedEnergy + 1e-20f) - kFFTNormDb;
            currentBandEnergyDb[i] = energyDb;

            // Apply contour offset: threshold is the contour value shifted by the offset
            float adjustedContourDb = splAdjustments[i] + thresholdDb;

            float gainReductionDb = 0.0f;
            if (energyDb > adjustedContourDb) {
                float overDb = energyDb - adjustedContourDb;
                gainReductionDb = -(overDb - overDb / ratio);
            }

            if (gainReductionDb < smoothedGainReductionDb[i])
                smoothedGainReductionDb[i] = gainReductionDb + attackCoeff
                    * (smoothedGainReductionDb[i] - gainReductionDb);
            else
                smoothedGainReductionDb[i] = gainReductionDb + releaseCoeff
                    * (smoothedGainReductionDb[i] - gainReductionDb);

            currentBandGainReductionDb[i] = smoothedGainReductionDb[i];
            bandGainLinear[i] = std::pow(10.0f, smoothedGainReductionDb[i] / 20.0f);
        }

        // ── Step 2b: Per-bin gain via log-frequency interpolation ────────
        //
        // WHY NOT PIECEWISE-CONSTANT (the naive approach)?
        // ─────────────────────────────────────────────────
        // Assigning one gain value per Bark band:
        //
        //   binGains[k] = bandGainLinear[binToBand[k]]
        //
        // creates 24 rectangular amplitude steps in the frequency domain.
        // A rectangular step in spectrum space is equivalent to a brickwall
        // filter whose time-domain impulse response is a sinc function that
        // lasts the full analysis window duration (~46ms at 44.1kHz / 2048pt).
        // When the compressor drives the step depth to 6–15dB (ratio > 2:1),
        // this produces clearly audible pre-echo, ringing, and smearing
        // artefacts — especially on transients.
        //
        // A short IIR pass (alpha ≈ 0.7, ~3-bin transition width) only
        // mitigates low-frequency bands (2–5 bins wide per Bark unit at
        // 44.1kHz); high-frequency bands span 200–300 bins so a 3-bin blur
        // has negligible effect at those edges.
        //
        // SOLUTION: log-frequency interpolation in dB space
        // ─────────────────────────────────────────────────
        // Instead of a step function, we use the 24 Bark band centre gains as
        // *knots* of a piecewise-linear curve on the log-Hz axis.  Each bin's
        // gain is an exponential (dB-space) blend that is proportional to its
        // log-frequency distance between the two nearest band centres.
        //
        // This gives a smooth, globally monotone spectral gain envelope with
        // NO sharp edges anywhere → the effective filter is smooth → no ringing.
        //
        // CORRECTNESS:
        //   • At ratio 1:1 all bandGainLinear[i] == 1.0 → all gainLn == 0
        //     → exp(0) == 1.0 → perfect unity gain, bit-for-bit identical to
        //     bypassing the gain stage.
        //   • Interpolation in ln space == geometric mean in linear space,
        //     which is the perceptually correct in-between point for gains
        //     (arithmetic mean of dB values).
        //   • Each bin's result is still determined primarily by the band it
        //     belongs to; the neighbouring band's gain only influences the
        //     transition region between the two centres.
        {
            // Project per-band linear gains into natural-log (dB-proportional) space.
            std::array<float, NUM_BARK_BANDS> bandGainLn{};
            for (int i = 0; i < NUM_BARK_BANDS; ++i)
                bandGainLn[i] = std::log(std::max(bandGainLinear[i], 1e-10f));

            for (int k = 0; k < currentNumBins; ++k) {
                const int   band    = binToBand[k];
                const float logFreq = binLogFreq[k]; // precomputed in computeBinToBarkMapping
                float gainLn;

                if (logFreq <= bandCenterLogFreq[band]) {
                    // Bin is at or below the current band's centre frequency.
                    // Blend toward the next-lower band.
                    if (band == 0) {
                        gainLn = bandGainLn[0]; // flat extrapolation at the low end
                    } else {
                        const float lo  = bandCenterLogFreq[band - 1];
                        const float hi  = bandCenterLogFreq[band];
                        const float den = hi - lo;
                        // t=0 → lower neighbour's gain, t=1 → this band's gain
                        const float t = (den > 1e-10f)
                            ? std::max(0.0f, std::min(1.0f, (logFreq - lo) / den))
                            : 1.0f;
                        gainLn = bandGainLn[band - 1] + t * (bandGainLn[band] - bandGainLn[band - 1]);
                    }
                } else {
                    // Bin is above the current band's centre frequency.
                    // Blend toward the next-higher band.
                    if (band >= NUM_BARK_BANDS - 1) {
                        gainLn = bandGainLn[NUM_BARK_BANDS - 1]; // flat extrapolation at high end
                    } else {
                        const float lo  = bandCenterLogFreq[band];
                        const float hi  = bandCenterLogFreq[band + 1];
                        const float den = hi - lo;
                        // t=0 → this band's gain, t=1 → upper neighbour's gain
                        const float t = (den > 1e-10f)
                            ? std::max(0.0f, std::min(1.0f, (logFreq - lo) / den))
                            : 0.0f;
                        gainLn = bandGainLn[band] + t * (bandGainLn[band + 1] - bandGainLn[band]);
                    }
                }

                binGains[k] = std::exp(gainLn);
            }
        }

        // Optional secondary IIR pass: gentle cosmetic finish.
        // With the log-frequency interpolation above producing an already-smooth
        // spectral envelope, this pass only rounds off micro-steps that arise
        // at the coarse boundary between the discrete bin grid and the
        // continuous interpolation curve.  It has no effect on audible ringing.
        if (smoothingAlpha < 0.99f) {
            const float a = smoothingAlpha;
            const float b = 1.0f - a;
            // Forward pass
            for (int k = 1; k < currentNumBins; ++k)
                binGains[k] = a * binGains[k] + b * binGains[k - 1];
            // Backward pass (zero-phase: two passes cancel group delay)
            for (int k = currentNumBins - 2; k >= 0; --k)
                binGains[k] = a * binGains[k] + b * binGains[k + 1];
        }

        // ── Steps 3 & 4: Stereo synthesis ────────────────────────────────
        // For each channel: window + FFT -> apply same per-band gains -> IFFT -> OLA.
        //
        // The Hann window is applied ONLY at the analysis stage (before FFT).
        // No synthesis window is applied. The periodic Hann COLA sum is:
        //   50% overlap (hop=N/2): sum = 1.0 → olaScaleFactor = 1.0
        //   75% overlap (hop=N/4): sum = 2.0 → olaScaleFactor = 0.5
        // The olaScaleFactor compensates for the non-unity COLA sum at higher overlap.

        auto synthesiseChannel = [&](const phu::memory::AlignedVector<float>& inRing,
                                     phu::memory::AlignedVector<float>& outRing,
                                     phu::memory::AlignedVector<float>& buf) {
            // Split-at-wrap ring buffer copy (SIMD-friendly)
            const int startIdx = (ringWritePos - currentFFTSize + ringSize) % ringSize;
            const int firstLen = ringSize - startIdx;
            if (firstLen >= currentFFTSize) {
                // No wrap: single contiguous copy
                juce::FloatVectorOperations::copy(buf.data(), inRing.data() + startIdx, currentFFTSize);
            } else {
                // Wraps: two segments
                juce::FloatVectorOperations::copy(buf.data(), inRing.data() + startIdx, firstLen);
                juce::FloatVectorOperations::copy(buf.data() + firstLen, inRing.data(), currentFFTSize - firstLen);
            }
            
            // Apply Hann window (SIMD)
            olaWindow->applyForFFTInPlace(buf.data());
            
            // Zero-pad second half for real FFT
            juce::FloatVectorOperations::clear(buf.data() + currentFFTSize, currentFFTSize);

            fft->performRealOnlyForwardTransform(buf.data());

            for (int k = 0; k < currentNumBins; ++k) {
                float g = binGains[k];
                buf[k * 2]     *= g;
                buf[k * 2 + 1] *= g;
            }

            fft->performRealOnlyInverseTransform(buf.data());

            // OLA: accumulate IFFT output with COLA scale (SIMD)
            const int outStartIdx = (ringWritePos - currentFFTSize + ringSize) % ringSize;
            const int outFirstLen = ringSize - outStartIdx;
            if (outFirstLen >= currentFFTSize) {
                // No wrap: single accumulation
                olaWindow->applyAndAccumulate(buf.data(), outRing.data() + outStartIdx, currentFFTSize);
            } else {
                // Wraps: two accumulations
                olaWindow->applyAndAccumulate(buf.data(), outRing.data() + outStartIdx, outFirstLen);
                olaWindow->applyAndAccumulate(buf.data() + outFirstLen, outRing.data(), currentFFTSize - outFirstLen);
            }
        };

        synthesiseChannel(inputRingL, outputRingL, fftBufferL);
        synthesiseChannel(inputRingR, outputRingR, fftBufferR);
    }

    // ── FFT engine ───────────────────────────────────────────────────────

    std::unique_ptr<juce::dsp::FFT> fft;

    // ── Runtime FFT parameters (set in prepare()) ────────────────────────

    FFTMode currentFFTMode = FFTMode::Precision;
    OverlapMode currentOverlapMode = OverlapMode::Half;
    int currentFFTOrder    = 11;
    int currentFFTSize     = 2048;
    int currentNumBins     = 1024;
    int currentHopSize     = 1024;
    float olaScaleFactor   = 1.0f;  // COLA normalisation: 1.0 at 50%, 0.5 at 75%
    float kFFTNormDb       = 54.2f; // 20*log10(2048/4), updated in prepare()

    // ── Parameters ───────────────────────────────────────────────────────

    float currentSampleRate = 44100.0f;
    float thresholdDb       = -20.0f;
    float ratio             = 4.0f;
    float attackMs          = 10.0f;
    float releaseMs         = 100.0f;
    float smoothingAlpha    = 0.70f; // bidirectional IIR coefficient (1=off, 0=max smooth)

    // Equal-loudness contour engine
    EqualLoudnessContour contour;

    // ── Smoothing coefficients ───────────────────────────────────────────

    float attackCoeff      = 0.0f;
    float releaseCoeff     = 0.0f;

    // ── Pre-computed lookup tables (size depends on FFT mode) ────────────

    std::unique_ptr<OLAWindow> olaWindow;  // Handles window generation and OLA operations
    std::vector<int>   binToBand;     // [currentNumBins]
    // Per-bin gain array. Populated each frame by log-frequency interpolation
    // (processFFTFrame step 2b), then optionally refined by the secondary IIR pass.
    phu::memory::AlignedVector<float> binGains;

    // Precomputed log10(bin_centre_frequency_hz) for every FFT bin.
    // Avoids transcendental calls inside the audio-thread hot loop.
    // Rebuilt whenever the FFT size or sample rate changes (computeBinToBarkMapping).
    phu::memory::AlignedVector<float> binLogFreq;

    // Precomputed log10(band_centre_frequency_hz) for all 24 Bark bands.
    // Used as the interpolation knot positions in processFFTFrame step 2b.
    std::array<float, NUM_BARK_BANDS> bandCenterLogFreq{};
    std::array<int, NUM_BARK_BANDS> binsPerBand{};
    std::array<float, NUM_BARK_BANDS> barkBandCenterFreqs{};
    std::array<float, NUM_BARK_BANDS> barkBandLowFreqs{};
    std::array<float, NUM_BARK_BANDS> barkBandHighFreqs{};

    // ── Per-band state ───────────────────────────────────────────────────

    std::array<float, NUM_BARK_BANDS> smoothedGainReductionDb{};
    std::array<float, NUM_BARK_BANDS> currentBandGainReductionDb{};
    std::array<float, NUM_BARK_BANDS> currentBandEnergyDb{};

    // ── Overlap-add ring buffers (size = currentFFTSize * 2) ─────────────
    //
    // Mono mix is used for analysis only.  L/R have independent synthesis
    // paths so per-band gains affect only the corresponding frequency range
    // in each channel (no cross-band pumping).
    phu::memory::AlignedVector<float> inputRingMono;
    phu::memory::AlignedVector<float> inputRingL;
    phu::memory::AlignedVector<float> inputRingR;
    phu::memory::AlignedVector<float> outputRingL;
    phu::memory::AlignedVector<float> outputRingR;
    int ringWritePos = 0;
    int samplesUntilNextFFT = 1024; // initialised to currentHopSize in prepare()

    // ── FFT workspace (size = currentFFTSize * 2) ────────────────────────
    //
    // fftBuffer  — analysis (mono mix)
    // fftBufferL — synthesis channel L
    // fftBufferR — synthesis channel R
    phu::memory::AlignedVector<float> fftBuffer;
    phu::memory::AlignedVector<float> fftBufferL;
    phu::memory::AlignedVector<float> fftBufferR;


};

} // namespace audio
} // namespace phu
