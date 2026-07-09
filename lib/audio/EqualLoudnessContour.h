#pragma once

#include <array>
#include <algorithm>
#include "Iso226.h"

namespace phu {
namespace audio {

/**
 * EqualLoudnessContour — Equal-loudness contour adjustments for 24 Bark bands.
 *
 * Provides relative SPL (dB) adjustments for each Bark band at different listening levels.
 * Supports both hardcoded "Smile" curves (legacy) and computed ISO 226 contours.
 *
 * Usage:
 *   EqualLoudnessContour contour;
 *   contour.setPreset(EqualLoudnessContour::Preset::ISO226_40Phon);
 *   float adjustment = contour.getAdjustmentDb(bandIndex);
 *
 * Header-only, depends on Iso226.h for computed contours.
 */
class EqualLoudnessContour {
  public:
    // Equal-loudness contour presets
    enum class Preset {
        // Legacy hardcoded "Smile" curves (renamed from ISO226_*Phon)
        Smile_20Phon = 0,  // Very quiet listening - large bass/treble boost needed
        Smile_40Phon,      // Moderate listening - moderate bass/treble boost
        Smile_60Phon,      // Comfortable listening - mild bass/treble boost
        Smile_80Phon,      // Loud listening - nearly flat
        
        // New ISO 226 computed contours (normalized to min=0.0)
        ISO226_20Phon,     // Very quiet listening - computed from ISO 226
        ISO226_40Phon,     // Moderate listening - computed from ISO 226
        ISO226_60Phon,     // Comfortable listening - computed from ISO 226
        ISO226_80Phon,     // Loud listening - computed from ISO 226
        
        Flat,              // No contour adjustment
        NumPresets
    };

    static constexpr int NUM_BARK_BANDS = 24;

    EqualLoudnessContour() {
        computeHardcodedContours();
        computeIso226Contours();
    }

    /**
     * Set the contour preset.
     * Changes take effect immediately (no recomputation needed - we use precomputed tables).
     */
    void setPreset(Preset preset) {
        if (preset != currentPreset && 
            preset >= Preset::Smile_20Phon && 
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
            // Legacy Smile curves
            case Preset::Smile_20Phon: return "Smile - 20 phon";
            case Preset::Smile_40Phon: return "Smile - 40 phon";
            case Preset::Smile_60Phon: return "Smile - 60 phon";
            case Preset::Smile_80Phon: return "Smile - 80 phon";
            
            // Computed ISO 226 contours
            case Preset::ISO226_20Phon: return "ISO 226 - 20 phon";
            case Preset::ISO226_40Phon: return "ISO 226 - 40 phon";
            case Preset::ISO226_60Phon: return "ISO 226 - 60 phon";
            case Preset::ISO226_80Phon: return "ISO 226 - 80 phon";
            
            case Preset::Flat:          return "Flat (no contour)";
            default:                    return "Unknown";
        }
    }

  private:
    // Pre-computed Bark band center frequencies (Hz)
    static constexpr std::array<float, NUM_BARK_BANDS> barkBandCenterFreqs = {
        50.0f, 150.0f, 250.0f, 350.0f, 450.0f, 570.0f, 700.0f, 840.0f,
        1000.0f, 1170.0f, 1370.0f, 1600.0f, 1850.0f, 2150.0f, 2500.0f, 2900.0f,
        3400.0f, 3950.0f, 4600.0f, 5350.0f, 6200.0f, 7200.0f, 8400.0f, 9800.0f
    };

    /**
     * Pre-compute hardcoded "Smile" contours (legacy).
     * These are the original hand-tuned curves.
     */
    void computeHardcodedContours() {
        // 20 phon: very quiet listening - large bass/treble boost needed
        contourTables[static_cast<int>(Preset::Smile_20Phon)] = {{
            40.0f,  30.0f,  22.0f,  16.0f,  12.0f,  9.0f,   6.0f,   4.0f,
             2.0f,   0.0f,  -1.0f,  -2.0f,  -2.0f,  -2.0f,  -1.0f,   0.0f,
             1.0f,   3.0f,   5.0f,   8.0f,  12.0f,  16.0f,  22.0f,  30.0f
        }};

        // 40 phon: moderate listening level - moderate bass/treble boost
        contourTables[static_cast<int>(Preset::Smile_40Phon)] = {{
            28.0f,  20.0f,  14.0f,  10.0f,   7.0f,   5.0f,   3.0f,   2.0f,
             1.0f,   0.0f,  -1.0f,  -1.5f,  -1.5f,  -1.0f,  -0.5f,   0.0f,
             0.5f,   1.5f,   3.0f,   5.0f,   8.0f,  12.0f,  18.0f,  25.0f
        }};

        // 60 phon: comfortable listening - mild bass/treble boost
        contourTables[static_cast<int>(Preset::Smile_60Phon)] = {{
            18.0f,  12.0f,   8.0f,   5.0f,   3.0f,   2.0f,   1.0f,   0.5f,
             0.0f,   0.0f,  -0.5f,  -1.0f,  -1.0f,  -0.5f,   0.0f,   0.0f,
             0.5f,   1.0f,   2.0f,   3.0f,   5.0f,   8.0f,  13.0f,  19.0f
        }};

        // 80 phon: loud listening - nearly flat
        contourTables[static_cast<int>(Preset::Smile_80Phon)] = {{
            10.0f,   6.0f,   3.0f,   1.5f,   0.5f,   0.0f,   0.0f,   0.0f,
             0.0f,   0.0f,   0.0f,  -0.5f,  -0.5f,   0.0f,   0.0f,   0.0f,
             0.0f,   0.5f,   1.0f,   1.5f,   3.0f,   5.0f,   8.0f,  12.0f
        }};

        // Flat: no contour adjustment
        contourTables[static_cast<int>(Preset::Flat)] = {{
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
        }};
    }

    /**
     * Pre-compute ISO 226 contours for the 24 Bark band center frequencies.
     * Uses Iso226::byGivenFreqs() and normalizes so the minimum value is 0.0.
     */
    void computeIso226Contours() {
        // Convert Bark band center frequencies to double for Iso226
        std::array<double, NUM_BARK_BANDS> freqs;
        for (int i = 0; i < NUM_BARK_BANDS; ++i) {
            freqs[i] = static_cast<double>(barkBandCenterFreqs[i]);
        }

        // Temporary buffers for computation
        std::array<double, NUM_BARK_BANDS> contourBuffer;

        // Compute and normalize contours for each phon level
        for (int phon : {20, 40, 60, 80}) {
            // Compute raw contour using Iso226
            Iso226::byGivenFreqs(static_cast<double>(phon), freqs, contourBuffer);

            // Find minimum value
            auto minIt = std::min_element(contourBuffer.begin(), contourBuffer.end());
            double minVal = *minIt;

            // Store in contourTables with normalization applied
            Preset preset;
            switch (phon) {
                case 20: preset = Preset::ISO226_20Phon; break;
                case 40: preset = Preset::ISO226_40Phon; break;
                case 60: preset = Preset::ISO226_60Phon; break;
                case 80: preset = Preset::ISO226_80Phon; break;
                default: continue;
            }
            
            // Transform: copy from contourBuffer to contourTables and normalize in one pass
            std::transform(
                contourBuffer.begin(), contourBuffer.end(),
                contourTables[static_cast<int>(preset)].begin(),
                [minVal](double val) { return static_cast<float>(val - minVal); }
            );
        }
    }

    Preset currentPreset = Preset::Smile_40Phon;
    static constexpr int kNumContourPresets = static_cast<int>(Preset::NumPresets);
    std::array<std::array<float, NUM_BARK_BANDS>, kNumContourPresets> contourTables{};
};

} // namespace audio
} // namespace phu
