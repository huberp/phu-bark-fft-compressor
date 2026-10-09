#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

/** Vertical stereo L/R level meter with ballistic display and peak hold.
 *  Driven from the editor's 60 Hz timer via setLevel(). No internal timer. */
class LevelMeterComponent : public juce::Component {
public:
    LevelMeterComponent()
        : m_releaseCoeff(std::exp(-1.0f / (kTimerHz * kReleaseTimeSec))) {}

    /** Push new per-channel linear RMS values from the UI timer. */
    void setLevel(float rmsL, float rmsR) noexcept {
        auto advance = [this](float rms, float& display, float& peak, int& holdCount) noexcept {
            display = (rms >= display) ? rms : display * m_releaseCoeff;
            if (rms >= peak) {
                peak      = rms;
                holdCount = kPeakHoldFrames;
            } else if (holdCount > 0) {
                --holdCount;
            } else {
                peak *= m_releaseCoeff;
            }
        };
        advance(rmsL, m_displayL, m_peakL, m_peakHoldL);
        advance(rmsR, m_displayR, m_peakR, m_peakHoldR);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(0xFF0D0D1A));
        g.fillRect(bounds);

        constexpr float kBarGap = 3.0f;
        const float barW = (bounds.getWidth() - kBarGap) * 0.5f;

        paintBar(g, bounds.withWidth(barW),                                    m_displayL, m_peakL);
        paintBar(g, bounds.withX(bounds.getX() + barW + kBarGap).withWidth(barW), m_displayR, m_peakR);
    }

private:
    static constexpr float kTimerHz        = 60.0f;
    static constexpr float kReleaseTimeSec = 0.3f;
    static constexpr float kPeakHoldSec    = 2.0f;
    static constexpr int   kPeakHoldFrames = static_cast<int>(kTimerHz * kPeakHoldSec);
    static constexpr float kMinDb          = -60.0f;
    static constexpr float kMaxDb          =   0.0f;

    // Computed from kReleaseTimeSec and kTimerHz in the constructor.
    float m_releaseCoeff = 0.946f;

    float m_displayL = 0.0f, m_displayR = 0.0f;
    float m_peakL    = 0.0f, m_peakR    = 0.0f;
    int   m_peakHoldL = 0,   m_peakHoldR = 0;

    void paintBar(juce::Graphics& g, juce::Rectangle<float> b,
                  float displayLinear, float peakLinear) const {
        const float h = b.getHeight();

        // Maps a dBFS value to a Y coordinate within b (top = 0 dBFS, bottom = kMinDb).
        auto dbToY = [&](float dB) -> float {
            dB = juce::jlimit(kMinDb, kMaxDb, dB);
            return b.getY() + (1.0f - (dB - kMinDb) / (kMaxDb - kMinDb)) * h;
        };

        const float displayDb = juce::Decibels::gainToDecibels(displayLinear, kMinDb - 1.0f);

        if (displayDb > kMinDb) {
            const float barTop = dbToY(displayDb);
            const float barBot = dbToY(kMinDb);
            g.setColour(colourForDb(displayDb).withAlpha(0.85f));
            g.fillRect(b.getX(), barTop, b.getWidth(), barBot - barTop);
        }

        const float peakDb = juce::Decibels::gainToDecibels(peakLinear, kMinDb - 1.0f);
        if (peakDb > kMinDb) {
            g.setColour(colourForDb(peakDb).brighter(0.3f));
            g.fillRect(b.getX(), dbToY(peakDb), b.getWidth(), 2.0f);
        }
    }

    static juce::Colour colourForDb(float dB) noexcept {
        if (dB >= 0.0f)   return juce::Colour(0xFFCC2200);
        if (dB >= -6.0f)  return juce::Colour(0xFFDDAA00);
        return juce::Colour(0xFF44AA44);
    }
};
