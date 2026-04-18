#pragma once

#include "audio/BarkFFTCompressor.h"
#include "audio/FFTProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

using phu::audio::BarkFFTCompressor;
using phu::audio::FFTProcessor;

/**
 * SpectrumDisplay — draws the FFT spectrum with Bark band overlays,
 * equal-loudness contour, and Bark-band energy bars.
 *
 * Rendered on the UI thread at 60 Hz.
 */
template <typename SampleType = float>
class SpectrumDisplay : public juce::Component {
  public:
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    static constexpr float MIN_DB   = -80.0f;
    static constexpr float MAX_DB   = 0.0f;

    // Band colours for the 24 Bark bands (cycling through a palette)
    static constexpr int NUM_BARK_BANDS = BarkFFTCompressor::NUM_BARK_BANDS;

    SpectrumDisplay();

    void paint(juce::Graphics& g) override;

    /** Set references to the FFT processors and compressor for drawing. */
    void setProcessors(FFTProcessor<SampleType>* inputFFT, FFTProcessor<SampleType>* outputFFT,
                       const BarkFFTCompressor* compressor);

    /** Set the sample rate for frequency axis calculations. */
    void setSampleRate(double sr) { sampleRate = static_cast<float>(sr); }

    /** Toggle display of input/output spectrum. */
    void setInputFFTEnabled(bool enabled) { showInputFFT = enabled; }
    void setOutputFFTEnabled(bool enabled) { showOutputFFT = enabled; }

    /** Toggle display of equal-loudness contour overlay. */
    void setContourEnabled(bool enabled) { showContour = enabled; }

    /** Toggle display of Bark-band energy bars. */
    void setBarkEnergyEnabled(bool enabled) { showBarkEnergy = enabled; }

    /** Get the colour for a specific Bark band. */
    static juce::Colour getBandColour(int band);

  private:
    // Coordinate conversion helpers
    float freqToX(float freq, float width) const;
    float dbToY(float db, float height) const;

    // Drawing helpers
    void drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds,
                      FFTProcessor<SampleType>* fftProc, juce::Colour colour, float lineWidth);
    void drawBarkBands(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawContour(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawBarkEnergy(juce::Graphics& g, juce::Rectangle<int> bounds);

    FFTProcessor<SampleType>* inputFFTProcessor = nullptr;
    FFTProcessor<SampleType>* outputFFTProcessor = nullptr;
    const BarkFFTCompressor* compressorRef = nullptr;

    float sampleRate = 48000.0f;
    bool showInputFFT = false;
    bool showOutputFFT = true;
    bool showContour = true;
    bool showBarkEnergy = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumDisplay)
};
