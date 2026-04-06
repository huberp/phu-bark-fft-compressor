#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <juce_dsp/juce_dsp.h>

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
    static constexpr int FFT_ORDER = 11;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER; // 2048
    static constexpr int NUM_BINS = FFT_SIZE / 2;    // 1024 usable bins
    static constexpr int NUM_BARK_BANDS = 24;
    static constexpr int HOP_SIZE = FFT_SIZE / 2;    // 50% overlap

    // Equal-loudness contour presets
    enum class ContourPreset {
        ISO226_20Phon = 0,
        ISO226_40Phon,
        ISO226_60Phon,
        ISO226_80Phon,
        Flat,
        NumPresets
    };

    BarkFFTCompressor() {
        fft = std::make_unique<juce::dsp::FFT>(FFT_ORDER);
    }

    /**
     * Prepare the compressor for playback.
     * Must be called from prepareToPlay with the current sample rate.
     */
    void prepare(double sampleRate) {
        currentSampleRate = static_cast<float>(sampleRate);

        // Pre-compute Hann window
        for (int i = 0; i < FFT_SIZE; ++i) {
            hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i)
                                                     / static_cast<float>(FFT_SIZE - 1)));
        }

        // Pre-compute bin-to-Bark mapping
        computeBinToBarkMapping();

        // Pre-compute equal-loudness contour adjustments
        computeEqualLoudnessContours();

        // Update attack/release coefficients
        updateCoefficients();

        // Reset state
        reset();
    }

    /**
     * Reset all internal state (call on transport restart, etc.).
     */
    void reset() {
        std::fill(inputRing.begin(), inputRing.end(), 0.0f);
        std::fill(outputRing.begin(), outputRing.end(), 0.0f);
        ringWritePos = 0;
        samplesUntilNextFFT = HOP_SIZE;

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
        if (preset != currentPreset) {
            currentPreset = preset;
            // No recomputation needed - we index into the precomputed table at runtime
        }
    }

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
        // Mix to mono for FFT analysis
        float mono = (inputL + inputR) * 0.5f;

        // Write mono into input ring buffer
        inputRing[ringWritePos] = mono;

        // Read from output ring buffer (this was written HOP_SIZE ago by OLA)
        float outMono = outputRing[ringWritePos];

        // Clear the output ring position for the next OLA accumulation
        outputRing[ringWritePos] = 0.0f;

        // Advance write position
        ringWritePos = (ringWritePos + 1) % (FFT_SIZE * 2);

        // Count down to next FFT hop
        --samplesUntilNextFFT;
        if (samplesUntilNextFFT <= 0) {
            processFFTFrame();
            samplesUntilNextFFT = HOP_SIZE;
        }

        // Apply output gain to both channels proportionally
        // Use the mono output / mono input ratio to scale stereo
        float gain = 1.0f;
        if (std::abs(mono) > 1e-10f) {
            gain = outMono / mono;
        }

        // Clamp gain to prevent extreme values during silence/transients
        gain = std::max(-10.0f, std::min(10.0f, gain));

        return { inputL * gain, inputR * gain };
    }

    // ── UI accessors (read from UI thread) ───────────────────────────────

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
        if (band >= 0 && band < NUM_BARK_BANDS) {
            int idx = static_cast<int>(currentPreset);
            return contourTables[idx][band];
        }
        return 0.0f;
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

    /** Get latency in samples. */
    int getLatencySamples() const { return FFT_SIZE; }

    /** Get the current contour preset. */
    ContourPreset getContourPreset() const { return currentPreset; }

    /** Get current sample rate. */
    float getSampleRate() const { return currentSampleRate; }

    float getThresholdDb() const { return thresholdDb; }
    float getRatio() const { return ratio; }
    float getAttackMs() const { return attackMs; }
    float getReleaseMs() const { return releaseMs; }

    /** Get name for a contour preset. */
    static const char* getContourPresetName(ContourPreset preset) {
        switch (preset) {
            case ContourPreset::ISO226_20Phon: return "ISO 226 - 20 phon";
            case ContourPreset::ISO226_40Phon: return "ISO 226 - 40 phon";
            case ContourPreset::ISO226_60Phon: return "ISO 226 - 60 phon";
            case ContourPreset::ISO226_80Phon: return "ISO 226 - 80 phon";
            case ContourPreset::Flat:          return "Flat (no contour)";
            default:                           return "Unknown";
        }
    }

  private:
    static constexpr float kPi = 3.14159265358979323846f;

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
        for (int k = 0; k < NUM_BINS; ++k) {
            float binFreq = (static_cast<float>(k) * currentSampleRate) / static_cast<float>(FFT_SIZE);
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

        for (int k = 0; k < NUM_BINS; ++k) {
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
    }

    /**
     * Pre-compute equal-loudness contour SPL adjustments for each Bark band.
     *
     * These represent the relative SPL (dB) needed at each band's center frequency
     * to be perceived as equally loud. Values are derived from ISO 226 curves.
     * Positive = ear is less sensitive (needs more SPL), negative = more sensitive.
     */
    void computeEqualLoudnessContours() {
        // ISO 226 approximate equal-loudness contours for 24 Bark bands
        // Band center frequencies (Bark): 0.5, 1.5, 2.5, ..., 23.5
        // Values: relative dB adjustment (0 dB = reference at 1 kHz region)

        // 20 phon: very quiet listening - large bass/treble boost needed
        contourTables[0] = {{
            40.0f,  30.0f,  22.0f,  16.0f,  12.0f,  9.0f,   6.0f,   4.0f,
             2.0f,   0.0f,  -1.0f,  -2.0f,  -2.0f,  -2.0f,  -1.0f,   0.0f,
             1.0f,   3.0f,   5.0f,   8.0f,  12.0f,  16.0f,  22.0f,  30.0f
        }};

        // 40 phon: moderate listening level - moderate bass/treble boost
        contourTables[1] = {{
            28.0f,  20.0f,  14.0f,  10.0f,   7.0f,   5.0f,   3.0f,   2.0f,
             1.0f,   0.0f,  -1.0f,  -1.5f,  -1.5f,  -1.0f,  -0.5f,   0.0f,
             0.5f,   1.5f,   3.0f,   5.0f,   8.0f,  12.0f,  18.0f,  25.0f
        }};

        // 60 phon: comfortable listening - mild bass/treble boost
        contourTables[2] = {{
            18.0f,  12.0f,   8.0f,   5.0f,   3.0f,   2.0f,   1.0f,   0.5f,
             0.0f,   0.0f,  -0.5f,  -1.0f,  -1.0f,  -0.5f,   0.0f,   0.0f,
             0.5f,   1.0f,   2.0f,   3.0f,   5.0f,   8.0f,  13.0f,  19.0f
        }};

        // 80 phon: loud listening - nearly flat
        contourTables[3] = {{
            10.0f,   6.0f,   3.0f,   1.5f,   0.5f,   0.0f,   0.0f,   0.0f,
             0.0f,   0.0f,   0.0f,  -0.5f,  -0.5f,   0.0f,   0.0f,   0.0f,
             0.0f,   0.5f,   1.0f,   1.5f,   3.0f,   5.0f,   8.0f,  12.0f
        }};

        // Flat: no contour adjustment
        contourTables[4] = {{
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
        }};
    }

    /** Update attack/release smoothing coefficients from ms values. */
    void updateCoefficients() {
        if (currentSampleRate <= 0.0f) return;

        // Coefficients computed per-hop (HOP_SIZE samples per frame)
        float hopsPerSecond = currentSampleRate / static_cast<float>(HOP_SIZE);
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

    /** Process one FFT frame: analysis → compression → synthesis via OLA. */
    void processFFTFrame() {
        // 1. Copy input ring into FFT buffer with Hann window
        //    Read the last FFT_SIZE samples ending at current ringWritePos
        for (int i = 0; i < FFT_SIZE; ++i) {
            int ringIdx = (ringWritePos - FFT_SIZE + i + FFT_SIZE * 2) % (FFT_SIZE * 2);
            fftBuffer[i] = inputRing[ringIdx] * hannWindow[i];
        }

        // Zero-pad the second half (complex part for JUCE FFT)
        for (int i = FFT_SIZE; i < FFT_SIZE * 2; ++i) {
            fftBuffer[i] = 0.0f;
        }

        // 2. Forward FFT (in-place, interleaved real/imag)
        fft->performRealOnlyForwardTransform(fftBuffer.data());

        // 3. Compute power spectrum and Bark band energies
        std::array<float, NUM_BARK_BANDS> barkEnergies{};
        for (int k = 0; k < NUM_BINS; ++k) {
            float re = fftBuffer[k * 2];
            float im = fftBuffer[k * 2 + 1];
            float power = re * re + im * im;

            int band = binToBand[k];
            barkEnergies[band] += power;
        }

        // 4. Compute per-band compression gain
        const auto& splAdjustments = contourTables[static_cast<int>(currentPreset)];

        std::array<float, NUM_BARK_BANDS> bandGainLinear{};
        for (int i = 0; i < NUM_BARK_BANDS; ++i) {
            // Normalize energy by number of bins (avoid bands with more bins dominating)
            float normalizedEnergy = (binsPerBand[i] > 0)
                ? barkEnergies[i] / static_cast<float>(binsPerBand[i])
                : 0.0f;

            // Convert to dB
            float energyDb = 10.0f * std::log10(normalizedEnergy + 1e-20f);
            currentBandEnergyDb[i] = energyDb;

            // Deviation from equal-loudness contour
            float deviationDb = energyDb - splAdjustments[i];

            // Compression: if deviation exceeds threshold, compress
            float gainReductionDb = 0.0f;
            if (deviationDb > thresholdDb) {
                float overDb = deviationDb - thresholdDb;
                float compressedOver = overDb / ratio;
                gainReductionDb = -(overDb - compressedOver);
            }

            // Apply attack/release smoothing to gain reduction
            if (gainReductionDb < smoothedGainReductionDb[i]) {
                // Gain reduction increasing (compressor engaging) → use attack
                smoothedGainReductionDb[i] = gainReductionDb + attackCoeff
                    * (smoothedGainReductionDb[i] - gainReductionDb);
            } else {
                // Gain reduction decreasing (compressor releasing) → use release
                smoothedGainReductionDb[i] = gainReductionDb + releaseCoeff
                    * (smoothedGainReductionDb[i] - gainReductionDb);
            }

            // Store for UI display
            currentBandGainReductionDb[i] = smoothedGainReductionDb[i];

            // Convert smoothed gain reduction to linear
            bandGainLinear[i] = std::pow(10.0f, smoothedGainReductionDb[i] / 20.0f);
        }

        // 5. Apply per-band gain to FFT bins
        for (int k = 0; k < NUM_BINS; ++k) {
            float gain = bandGainLinear[binToBand[k]];
            fftBuffer[k * 2]     *= gain;
            fftBuffer[k * 2 + 1] *= gain;
        }
        // Mirror the negative frequencies
        // JUCE's performRealOnlyInverseTransform expects the full buffer
        // The DC and Nyquist bins don't have imaginary counterparts in JUCE's format

        // 6. Inverse FFT
        fft->performRealOnlyInverseTransform(fftBuffer.data());

        // 7. Overlap-add: window the IFFT output and accumulate into output ring
        for (int i = 0; i < FFT_SIZE; ++i) {
            int outIdx = (ringWritePos - FFT_SIZE + i + FFT_SIZE * 2) % (FFT_SIZE * 2);
            // Apply synthesis window and scale by 1/windowSum for perfect reconstruction
            // For 50% overlap Hann: scale factor = 2/3 per frame, but sum of two frames = 1
            // The Hann window OLA with 50% overlap sums to 1.0, so just apply window
            outputRing[outIdx] += fftBuffer[i] * hannWindow[i];
        }
    }

    // ── FFT engine ───────────────────────────────────────────────────────

    std::unique_ptr<juce::dsp::FFT> fft;

    // ── Parameters ───────────────────────────────────────────────────────

    float currentSampleRate = 44100.0f;
    float thresholdDb       = -20.0f;
    float ratio             = 4.0f;
    float attackMs          = 10.0f;
    float releaseMs         = 100.0f;
    ContourPreset currentPreset = ContourPreset::ISO226_40Phon;

    // ── Smoothing coefficients ───────────────────────────────────────────

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    // ── Pre-computed lookup tables ───────────────────────────────────────

    std::array<float, FFT_SIZE> hannWindow{};
    std::array<int, NUM_BINS> binToBand{};
    std::array<int, NUM_BARK_BANDS> binsPerBand{};
    std::array<float, NUM_BARK_BANDS> barkBandCenterFreqs{};
    std::array<float, NUM_BARK_BANDS> barkBandLowFreqs{};
    std::array<float, NUM_BARK_BANDS> barkBandHighFreqs{};

    // Equal-loudness contour tables: [preset_index][band]
    static constexpr int kNumContourPresets = static_cast<int>(ContourPreset::NumPresets);
    std::array<std::array<float, NUM_BARK_BANDS>, kNumContourPresets> contourTables{};

    // ── Per-band state ───────────────────────────────────────────────────

    std::array<float, NUM_BARK_BANDS> smoothedGainReductionDb{};
    std::array<float, NUM_BARK_BANDS> currentBandGainReductionDb{};
    std::array<float, NUM_BARK_BANDS> currentBandEnergyDb{};

    // ── Overlap-add ring buffers ─────────────────────────────────────────

    std::array<float, FFT_SIZE * 2> inputRing{};
    std::array<float, FFT_SIZE * 2> outputRing{};
    int ringWritePos = 0;
    int samplesUntilNextFFT = HOP_SIZE;

    // ── FFT workspace ────────────────────────────────────────────────────

    std::array<float, FFT_SIZE * 2> fftBuffer{};
};

} // namespace audio
} // namespace phu
