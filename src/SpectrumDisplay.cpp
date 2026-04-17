#include "SpectrumDisplay.h"

// ============================================================================
// Band colours - 24 colours cycling through a distinct palette
// ============================================================================

static const juce::Colour kBandColours[BarkFFTCompressor::NUM_BARK_BANDS] = {
    juce::Colour(0xFF8B0000u), // 0: dark red
    juce::Colour(0xFFCC3300u), // 1: red-orange
    juce::Colour(0xFFCC5500u), // 2: burnt orange
    juce::Colour(0xFFDD7700u), // 3: amber
    juce::Colour(0xFFCCAA00u), // 4: gold
    juce::Colour(0xFFBBCC00u), // 5: yellow-green
    juce::Colour(0xFF88AA00u), // 6: olive
    juce::Colour(0xFF44AA00u), // 7: green
    juce::Colour(0xFF228B22u), // 8: forest green
    juce::Colour(0xFF008866u), // 9: teal
    juce::Colour(0xFF006699u), // 10: steel blue
    juce::Colour(0xFF0055AAu), // 11: blue
    juce::Colour(0xFF3344BBu), // 12: medium blue
    juce::Colour(0xFF4B0082u), // 13: indigo
    juce::Colour(0xFF6600AAu), // 14: purple
    juce::Colour(0xFF880088u), // 15: magenta
    juce::Colour(0xFF993366u), // 16: plum
    juce::Colour(0xFFAA3355u), // 17: rose
    juce::Colour(0xFFBB4444u), // 18: coral
    juce::Colour(0xFFCC6633u), // 19: sienna
    juce::Colour(0xFF997744u), // 20: tan
    juce::Colour(0xFF778855u), // 21: sage
    juce::Colour(0xFF558877u), // 22: sea green
    juce::Colour(0xFF446699u), // 23: slate blue
};

// ============================================================================
// Construction
// ============================================================================

template <typename SampleType>
SpectrumDisplay<SampleType>::SpectrumDisplay() {
    setOpaque(true);
}

// ============================================================================
// Configuration
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::setProcessors(FFTProcessor<SampleType>* inputFFT, FFTProcessor<SampleType>* outputFFT,
                                     const BarkFFTCompressor* compressor) {
    inputFFTProcessor = inputFFT;
    outputFFTProcessor = outputFFT;
    compressorRef = compressor;
}

template <typename SampleType>
juce::Colour SpectrumDisplay<SampleType>::getBandColour(int band) {
    if (band >= 0 && band < NUM_BARK_BANDS)
        return kBandColours[band];
    return juce::Colours::grey;
}

// ============================================================================
// Coordinate conversion
// ============================================================================

template <typename SampleType>
float SpectrumDisplay<SampleType>::freqToX(float freq, float width) const {
    if (freq <= 0.0f) return 0.0f;
    float logMin = std::log10(MIN_FREQ);
    float logMax = std::log10(MAX_FREQ);
    float logFreq = std::log10(std::max(freq, MIN_FREQ));
    return ((logFreq - logMin) / (logMax - logMin)) * width;
}

template <typename SampleType>
float SpectrumDisplay<SampleType>::dbToY(float db, float height) const {
    float normalized = (db - MIN_DB) / (MAX_DB - MIN_DB);
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    return height * (1.0f - normalized);
}

// ============================================================================
// Paint
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    drawBackground(g, bounds);
    drawBarkBands(g, bounds);

    if (showInputFFT && inputFFTProcessor)
        drawSpectrum(g, bounds, inputFFTProcessor, juce::Colours::green.withAlpha(0.6f), 1.0f);

    if (showOutputFFT && outputFFTProcessor)
        drawSpectrum(g, bounds, outputFFTProcessor, juce::Colours::white, 1.5f);

    if (showContour && compressorRef)
        drawContour(g, bounds);

    if (showBarkEnergy && compressorRef)
        drawBarkEnergy(g, bounds);
}

// ============================================================================
// Drawing: background with grid
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(juce::Colour(0xFF1A1A2Eu));
    g.fillRect(bounds);

    auto w = static_cast<float>(bounds.getWidth());
    auto h = static_cast<float>(bounds.getHeight());

    // Frequency grid lines (log scale)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    static const float gridFreqs[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (float freq : gridFreqs) {
        float x = freqToX(freq, w) + static_cast<float>(bounds.getX());
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(bounds.getY()),
                           static_cast<float>(bounds.getBottom()));
    }

    // Frequency labels
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    static const char* gridLabels[] = {"50", "100", "200", "500", "1k", "2k", "5k", "10k"};
    for (int i = 0; i < 8; ++i) {
        float x = freqToX(gridFreqs[i], w) + static_cast<float>(bounds.getX());
        g.drawText(gridLabels[i], static_cast<int>(x) - 15, bounds.getBottom() - 14, 30, 12,
                   juce::Justification::centred);
    }

    // dB grid lines
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    for (float db = -70.0f; db <= -10.0f; db += 10.0f) {
        float y = dbToY(db, h) + static_cast<float>(bounds.getY());
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
    }

    // dB labels
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    for (float db = -60.0f; db <= 0.0f; db += 20.0f) {
        float y = dbToY(db, h) + static_cast<float>(bounds.getY());
        g.drawText(juce::String(static_cast<int>(db)) + " dB",
                   bounds.getX() + 2, static_cast<int>(y) - 6, 40, 12,
                   juce::Justification::centredLeft);
    }
}

// ============================================================================
// Drawing: FFT spectrum line
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds,
                                    FFTProcessor<SampleType>* fftProc, juce::Colour colour, float lineWidth) {
    if (!fftProc) return;

    const float* magnitudes = fftProc->getMagnitudeSpectrum();
    const int numBins = fftProc->getNumBins();
    if (numBins <= 0) return;

    auto w = static_cast<float>(bounds.getWidth());
    auto h = static_cast<float>(bounds.getHeight());
    float x0 = static_cast<float>(bounds.getX());
    float y0 = static_cast<float>(bounds.getY());

    juce::Path path;
    bool started = false;

    for (int bin = 1; bin < numBins; ++bin) {
        float freq = fftProc->getBinFrequency(bin, sampleRate);
        if (freq < MIN_FREQ || freq > MAX_FREQ)
            continue;

        float magnitude = magnitudes[bin];
        float db = (magnitude > 1e-10f) ? 20.0f * std::log10(magnitude) : MIN_DB;

        float x = freqToX(freq, w) + x0;
        float y = dbToY(db, h) + y0;

        if (!started) {
            path.startNewSubPath(x, y);
            started = true;
        } else {
            path.lineTo(x, y);
        }
    }

    if (started) {
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(lineWidth));
    }
}

// ============================================================================
// Drawing: Bark band overlays (vertical shading)
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::drawBarkBands(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!compressorRef) return;

    auto w = static_cast<float>(bounds.getWidth());
    float x0 = static_cast<float>(bounds.getX());

    for (int band = 0; band < NUM_BARK_BANDS; ++band) {
        float lowFreq = compressorRef->getBandLowFrequency(band);
        float highFreq = compressorRef->getBandHighFrequency(band);

        // Clamp to display range
        lowFreq = std::max(lowFreq, MIN_FREQ);
        highFreq = std::min(highFreq, MAX_FREQ);
        if (lowFreq >= highFreq) continue;

        float xLeft = freqToX(lowFreq, w) + x0;
        float xRight = freqToX(highFreq, w) + x0;

        // Draw semi-transparent band fill
        g.setColour(getBandColour(band).withAlpha(0.08f));
        g.fillRect(xLeft, static_cast<float>(bounds.getY()),
                   xRight - xLeft, static_cast<float>(bounds.getHeight()));

        // Draw thin vertical divider at band edge
        g.setColour(getBandColour(band).withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(xLeft), static_cast<float>(bounds.getY()),
                           static_cast<float>(bounds.getBottom()));
    }
}

// ============================================================================
// Drawing: equal-loudness contour
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::drawContour(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!compressorRef) return;

    auto w = static_cast<float>(bounds.getWidth());
    auto h = static_cast<float>(bounds.getHeight());
    float x0 = static_cast<float>(bounds.getX());
    float y0 = static_cast<float>(bounds.getY());

    // Draw the contour as a stepped line at each band's center frequency
    juce::Path contourPath;
    bool started = false;

    for (int band = 0; band < NUM_BARK_BANDS; ++band) {
        float centerFreq = compressorRef->getBandCenterFrequency(band);
        if (centerFreq < MIN_FREQ || centerFreq > MAX_FREQ)
            continue;

        float adjustDb = compressorRef->getContourAdjustmentDb(band);
        // The contour threshold in the DSP is: thresholdDb + splAdjustment[band]
        // Display at exactly that value so the yellow line and the energy bars
        // share the same dBFS coordinate system.
        float displayDb = compressorRef->getThresholdDb() + adjustDb;

        float x = freqToX(centerFreq, w) + x0;
        float y = dbToY(displayDb, h) + y0;

        if (!started) {
            contourPath.startNewSubPath(x, y);
            started = true;
        } else {
            contourPath.lineTo(x, y);
        }
    }

    if (started) {
        g.setColour(juce::Colours::yellow.withAlpha(0.6f));
        g.strokePath(contourPath, juce::PathStrokeType(2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ============================================================================
// Drawing: Bark-band energy bars
// ============================================================================

template <typename SampleType>
void SpectrumDisplay<SampleType>::drawBarkEnergy(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!compressorRef) return;

    auto w = static_cast<float>(bounds.getWidth());
    auto h = static_cast<float>(bounds.getHeight());
    float x0 = static_cast<float>(bounds.getX());
    float y0 = static_cast<float>(bounds.getY());

    for (int band = 0; band < NUM_BARK_BANDS; ++band) {
        float lowFreq = compressorRef->getBandLowFrequency(band);
        float highFreq = compressorRef->getBandHighFrequency(band);

        lowFreq = std::max(lowFreq, MIN_FREQ);
        highFreq = std::min(highFreq, MAX_FREQ);
        if (lowFreq >= highFreq) continue;

        float xLeft = freqToX(lowFreq, w) + x0;
        float xRight = freqToX(highFreq, w) + x0;

        float energyDb = compressorRef->getBandEnergyDb(band);
        float yTop = dbToY(energyDb, h) + y0;
        float yBottom = static_cast<float>(bounds.getBottom());

        // Draw energy bar from bottom up to energy level
        if (yTop < yBottom) {
            g.setColour(getBandColour(band).withAlpha(0.3f));
            g.fillRect(xLeft + 1.0f, yTop, xRight - xLeft - 2.0f, yBottom - yTop);
        }
    }
}

// Explicit instantiation for the default float specialisation.
template class SpectrumDisplay<float>;
