#pragma once

#include <array>

namespace phu {
namespace audio {

/**
 * EqualLoudnessContour — ISO 226 equal-loudness contour adjustments for 24 Bark bands.
 *
 * Provides relative SPL (dB) adjustments for each Bark band at different listening levels
 * (phon curves). Positive values = ear is less sensitive (needs more SPL), negative = more sensitive.
 *
 * Usage:
 *   EqualLoudnessContour contour;
 *   contour.setPreset(EqualLoudnessContour::Preset::ISO226_40Phon);
 *   float adjustment = contour.getAdjustmentDb(bandIndex);
 *
 * Header-only, no dependencies beyond standard library.
 */
class EqualLoudnessContour {
  public:
    // Equal-loudness contour presets based on ISO 226
    enum class Preset {
        ISO226_20Phon = 0,  // Very quiet listening - large bass/treble boost needed
        ISO226_40Phon,      // Moderate listening - moderate bass/treble boost
        ISO226_60Phon,      // Comfortable listening - mild bass/treble boost
        ISO226_80Phon,      // Loud listening - nearly flat
        Flat,               // No contour adjustment
        NumPresets
    };

    static constexpr int NUM_BARK_BANDS = 24;

    EqualLoudnessContour() {
        computeContours();
    }

    /**
     * Set the contour preset.
     * Changes take effect immediately (no recomputation needed - we use precomputed tables).
     */
    void setPreset(Preset preset) {
        if (preset != currentPreset && 
            preset >= Preset::ISO226_20Phon && 
            preset < Preset::NumPresets) {
            currentPreset = preset;
        }
    }

    /** Get the current contour preset. */
    Preset getPreset() const { 
        return currentPreset; 
    }

    /**
     * Get the SPL adjustment (dB) for a given Bark band in the current preset.
     * Returns 0.0 if band index is out of range.
     */
    float getAdjustmentDb(int band) const {
        if (band >= 0 && band < NUM_BARK_BANDS) {
            int idx = static_cast<int>(currentPreset);
            return contourTables[idx][band];
        }
        return 0.0f;
    }

    /**
     * Get all adjustments for the current preset (read-only array view).
     * Returns a pointer to the 24-element contour table.
     */
    const float* getAdjustments() const {
        int idx = static_cast<int>(currentPreset);
        return contourTables[idx].data();
    }

    /** Get human-readable name for a contour preset. */
    static const char* getPresetName(Preset preset) {
        switch (preset) {
            case Preset::ISO226_20Phon: return "ISO 226 - 20 phon";
            case Preset::ISO226_40Phon: return "ISO 226 - 40 phon";
            case Preset::ISO226_60Phon: return "ISO 226 - 60 phon";
            case Preset::ISO226_80Phon: return "ISO 226 - 80 phon";
            case Preset::Flat:          return "Flat (no contour)";
            default:                    return "Unknown";
        }
    }

  private:
    /**
     * Pre-compute equal-loudness contour SPL adjustments for each Bark band.
     *
     * These represent the relative SPL (dB) needed at each band's center frequency
     * to be perceived as equally loud. Values are derived from ISO 226 curves.
     * Positive = ear is less sensitive (needs more SPL), negative = more sensitive.
     *
     * Band center frequencies (Bark): 0.5, 1.5, 2.5, ..., 23.5
     * Values: relative dB adjustment (0 dB = reference at 1 kHz region)
     */
    void computeContours() {
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

    Preset currentPreset = Preset::ISO226_40Phon;
    static constexpr int kNumContourPresets = static_cast<int>(Preset::NumPresets);
    std::array<std::array<float, NUM_BARK_BANDS>, kNumContourPresets> contourTables{};
};

} // namespace audio
} // namespace phu
