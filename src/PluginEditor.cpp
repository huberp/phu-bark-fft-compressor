#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace {
struct LayoutMetrics {
    static constexpr int outerPadding      = 10;
    static constexpr int rowHeight         = 24;
    static constexpr int rowGap            = 4;
    static constexpr int labelWidth        = 110;
    static constexpr int groupPaddingV     = 18;
    static constexpr int groupPaddingH     = 10;
    static constexpr int groupGap          = 6;
    static constexpr int spectrumHeight    = 220;
    static constexpr int gainReductionHeight = 80;
    static constexpr int defaultEditorWidth  = 700;
    static constexpr int toggleColumnGap   = 10;

    static constexpr int groupHeight(int numRows) noexcept {
        return 2 * groupPaddingV + numRows * rowHeight + (numRows - 1) * rowGap;
    }
};
} // namespace

// ============================================================================
// GainReductionPanel
// ============================================================================

template <typename SampleType>
void PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::GainReductionPanel::paint(juce::Graphics& g) {
    auto fullBounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xFF1A1A2Eu));
    g.fillRect(fullBounds);

    // Title
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    auto titleArea = fullBounds.removeFromTop(14);
    g.drawText("Gain Reduction (dB)", titleArea, juce::Justification::centred);

    auto bounds = fullBounds; // remaining area below title

    if (!compressorRef) return;

    const int   numBands = phu::audio::BarkFFTCompressor::NUM_BARK_BANDS;
    const float maxGR    = 30.0f;
    const float bw       = static_cast<float>(bounds.getWidth());
    const float bh       = static_cast<float>(bounds.getHeight());
    const float bx       = static_cast<float>(bounds.getX());
    const float by       = static_cast<float>(bounds.getY());

    // Log-frequency mapping matching SpectrumDisplay (20 Hz – 20 kHz)
    const float kMinFreq = SpectrumDisplay<SampleType>::MIN_FREQ;
    const float kMaxFreq = SpectrumDisplay<SampleType>::MAX_FREQ;
    const float logMin   = std::log10(kMinFreq);
    const float logMax   = std::log10(kMaxFreq);
    auto freqToX = [&](float freq) -> float {
        freq = std::max(freq, kMinFreq);
        return bx + ((std::log10(freq) - logMin) / (logMax - logMin)) * bw;
    };

    for (int band = 0; band < numBands; ++band) {
        float freqLow  = compressorRef->getBandLowFrequency(band);
        float freqHigh = compressorRef->getBandHighFrequency(band);
        freqLow  = std::max(freqLow,  kMinFreq);
        freqHigh = std::min(freqHigh, kMaxFreq);
        if (freqLow >= freqHigh) continue;

        float xLeft  = freqToX(freqLow);
        float xRight = freqToX(freqHigh);
        float barW   = xRight - xLeft;

        float gr         = compressorRef->getBandGainReductionDb(band);
        float normalized = std::min(std::abs(gr) / maxGR, 1.0f);
        float barHeight  = normalized * bh;

        juce::Colour barColour = SpectrumDisplay<SampleType>::getBandColour(band);

        if (barHeight > 0.5f) {
            g.setColour(barColour.withAlpha(0.7f));
            g.fillRect(xLeft + 1.0f, by, barW - 2.0f, barHeight);
        }

        // Band number label at bottom, centred in the bar
        float centerX = (xLeft + xRight) * 0.5f;
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.drawText(juce::String(band + 1),
                   static_cast<int>(centerX) - 8, bounds.getBottom() - 10,
                   16, 10, juce::Justification::centred);
    }

    // Scale labels
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("0", bounds.getX() - 18, bounds.getY() - 4, 16, 12,
               juce::Justification::centredRight);
    g.drawText(juce::String(static_cast<int>(-maxGR)),
               bounds.getX() - 24, bounds.getBottom() - 8, 22, 12,
               juce::Justification::centredRight);

    // ── Per-bin smoothed GR curve (log-freq x, matching bars above) ───
    if (showGRCurve) {
        const int   numBins = compressorRef->getNumBins();
        const int   fftSize = compressorRef->getCurrentFFTSize();
        const float sr      = compressorRef->getSampleRate();

        if (numBins > 0 && fftSize > 0 && sr > 0.0f) {
            constexpr int kSamplesPerBand = 8;
            juce::Path curvePath;
            bool started = false;

            for (int band = 0; band < numBands; ++band) {
                float freqLow  = compressorRef->getBandLowFrequency(band);
                float freqHigh = compressorRef->getBandHighFrequency(band);

                for (int s = 0; s <= kSamplesPerBand; ++s) {
                    float frac = static_cast<float>(s) / static_cast<float>(kSamplesPerBand);
                    float freq = freqLow + frac * (freqHigh - freqLow);
                    int bin = static_cast<int>(freq * static_cast<float>(fftSize) / sr);
                    if (bin < 0) bin = 0;
                    if (bin >= numBins) bin = numBins - 1;

                    float gainDb = compressorRef->getBinGainDb(bin);
                    float grAbs  = std::min(std::abs(gainDb) / maxGR, 1.0f);

                    float x = freqToX(std::max(freq, kMinFreq));
                    float y = by + grAbs * bh;

                    if (!started) {
                        curvePath.startNewSubPath(x, y);
                        started = true;
                    } else {
                        curvePath.lineTo(x, y);
                    }
                }
            }

            if (started) {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.strokePath(curvePath, juce::PathStrokeType(1.5f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}

// ============================================================================
// Editor Construction
// ============================================================================

template <typename SampleType>
PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::PhuBarkFFTCompressorAudioProcessorEditor(
    PhuBarkFFTCompressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    // Set up spectrum display
    spectrumDisplay.setProcessors(&inputFFT, &outputFFT, &audioProcessor.getCompressor());
    spectrumDisplay.setSampleRate(audioProcessor.getSampleRate() > 0.0
                                      ? audioProcessor.getSampleRate()
                                      : 48000.0);
    addAndMakeVisible(spectrumDisplay);

    // Set up gain reduction panel
    gainReductionPanel.setCompressor(&audioProcessor.getCompressor());
    addAndMakeVisible(gainReductionPanel);

    // ── Compressor parameter group ──────────────────────────────────────

    compressorGroup.setText("Compressor");
    compressorGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(compressorGroup);

    // Threshold slider
    thresholdLabel.setText("Contour Offset", juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centredLeft);
    thresholdLabel.setTooltip("Adjusts the equal-loudness contour threshold up or down in dB. "
                              "Positive values increase the threshold, negative values decrease it.");
    addAndMakeVisible(thresholdLabel);

    thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    thresholdSlider.setTextValueSuffix(" dB");
    thresholdSlider.setTooltip("Adjusts the equal-loudness contour threshold up or down in dB. "
                               "Positive values increase the threshold, negative values decrease it.");
    addAndMakeVisible(thresholdSlider);
    thresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_THRESHOLD,
        thresholdSlider);

    // Ratio slider
    ratioLabel.setText("Ratio", juce::dontSendNotification);
    ratioLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(ratioLabel);

    ratioSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ratioSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    ratioSlider.setTextValueSuffix(":1");
    addAndMakeVisible(ratioSlider);
    ratioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_RATIO, ratioSlider);

    // Attack slider
    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(attackLabel);

    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    attackSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(attackSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_ATTACK, attackSlider);

    // Release slider
    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(releaseLabel);

    releaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    releaseSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(releaseSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_RELEASE,
        releaseSlider);

    // Contour preset combo box
    contourLabel.setText("Loudness Contour", juce::dontSendNotification);
    contourLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(contourLabel);

    // Populate contour presets in enum order so that item ID - 1 == ContourPreset int value.
    // This keeps the APVTS choice index aligned with the enum, which the processor casts directly.
    {
        const int numPresets = static_cast<int>(phu::audio::BarkFFTCompressor::ContourPreset::NumPresets);
        for (int i = 0; i < numPresets; ++i)
        {
            auto preset = static_cast<phu::audio::BarkFFTCompressor::ContourPreset>(i);
            contourCombo.addItem(phu::audio::BarkFFTCompressor::getContourPresetName(preset), i + 1);
        }
    }
    addAndMakeVisible(contourCombo);
    contourAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_CONTOUR,
        contourCombo);

    // FFT Mode combo box
    fftModeLabel.setText("FFT Mode", juce::dontSendNotification);
    fftModeLabel.setJustificationType(juce::Justification::centredLeft);
    fftModeLabel.setTooltip("Select between Precision (better frequency resolution) or "
                            "Transient (better transient response) modes.");
    addAndMakeVisible(fftModeLabel);

    fftModeCombo.addItem("Precision", 1);
    fftModeCombo.addItem("Transient", 2);
    fftModeCombo.setTooltip("Select between Precision (better frequency resolution) or "
                            "Transient (better transient response) modes.");
    addAndMakeVisible(fftModeCombo);
    fftModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_FFT_MODE,
        fftModeCombo);

    // Overlap mode combo box
    overlapLabel.setText("Overlap", juce::dontSendNotification);
    overlapLabel.setJustificationType(juce::Justification::centredLeft);
    overlapLabel.setTooltip("Select between 50% overlap (lower CPU) or "
                            "75% overlap (higher quality, 2x CPU cost).");
    addAndMakeVisible(overlapLabel);

    overlapCombo.addItem("50% (Low CPU)", 1);
    overlapCombo.addItem("75% (High Quality)", 2);
    overlapCombo.addItem("90% (Highest Quality)", 3);
    overlapCombo.setTooltip("Select between 50% overlap (lower CPU), "
                            "75% overlap (higher quality, 2x CPU cost) or "
                            "90% overlap (highest quality, 4x CPU cost).");
    addAndMakeVisible(overlapCombo);
    overlapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_OVERLAP_MODE,
        overlapCombo);

    // Smoothing slider
    smoothingLabel.setText("Smoothing Taps", juce::dontSendNotification);
    smoothingLabel.setJustificationType(juce::Justification::centredLeft);
    smoothingLabel.setTooltip("Per-bin gain smoothing amount using bidirectional IIR. "
                              "0 = no smoothing; 1 = maximum smoothing.");
    addAndMakeVisible(smoothingLabel);

    smoothingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    smoothingSlider.setTooltip("Per-bin gain smoothing amount using bidirectional IIR. "
                               "0 = no smoothing; 1 = maximum smoothing.");
    addAndMakeVisible(smoothingSlider);
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_SMOOTHING,
        smoothingSlider);

    // ── Transient Shaper group ──────────────────────────────────────────

    transientShaperGroup.setText("Transient Shaper");
    transientShaperGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(transientShaperGroup);

    // TS Attack slider
    tsAttackLabel.setText("Attack", juce::dontSendNotification);
    tsAttackLabel.setJustificationType(juce::Justification::centredLeft);
    tsAttackLabel.setTooltip("Boost or attenuate the attack portion of detected transients.");
    addAndMakeVisible(tsAttackLabel);

    tsAttackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tsAttackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    tsAttackSlider.setTextValueSuffix(" dB");
    tsAttackSlider.setTooltip("Boost or attenuate the attack portion of detected transients.");
    addAndMakeVisible(tsAttackSlider);
    tsAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_TS_ATTACK,
        tsAttackSlider);

    // TS Sustain slider
    tsSustainLabel.setText("Sustain", juce::dontSendNotification);
    tsSustainLabel.setJustificationType(juce::Justification::centredLeft);
    tsSustainLabel.setTooltip("Boost or attenuate the sustained portion of the signal.");
    addAndMakeVisible(tsSustainLabel);

    tsSustainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tsSustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    tsSustainSlider.setTextValueSuffix(" dB");
    tsSustainSlider.setTooltip("Boost or attenuate the sustained portion of the signal.");
    addAndMakeVisible(tsSustainSlider);
    tsSustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_TS_SUSTAIN,
        tsSustainSlider);

    // TS Sensitivity slider
    tsSensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    tsSensitivityLabel.setJustificationType(juce::Justification::centredLeft);
    tsSensitivityLabel.setTooltip("Threshold for detecting transients (0 = least sensitive, 100 = most sensitive).");
    addAndMakeVisible(tsSensitivityLabel);

    tsSensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tsSensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    tsSensitivitySlider.setTextValueSuffix(" %");
    tsSensitivitySlider.setTooltip("Threshold for detecting transients (0 = least sensitive, 100 = most sensitive).");
    addAndMakeVisible(tsSensitivitySlider);
    tsSensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_TS_SENSITIVITY,
        tsSensitivitySlider);

    // TS Bypass toggle
    tsBypassToggle.setButtonText("Bypass Transient Shaper");
    tsBypassToggle.setTooltip("When enabled, bypasses the transient shaper (passes audio through unchanged).");
    addAndMakeVisible(tsBypassToggle);
    tsBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_TS_BYPASS,
        tsBypassToggle);

    // ── Display toggle group ────────────────────────────────────────────

    displayGroup.setText("Display");
    displayGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(displayGroup);

    inputFFTToggle.setButtonText("Input FFT");
    inputFFTToggle.setToggleState(false, juce::dontSendNotification);
    inputFFTToggle.onClick = [this]() {
        spectrumDisplay.setInputFFTEnabled(inputFFTToggle.getToggleState());
        spectrumDisplay.repaint();
    };
    addAndMakeVisible(inputFFTToggle);

    outputFFTToggle.setButtonText("Output FFT");
    outputFFTToggle.setToggleState(true, juce::dontSendNotification);
    outputFFTToggle.onClick = [this]() {
        spectrumDisplay.setOutputFFTEnabled(outputFFTToggle.getToggleState());
        spectrumDisplay.repaint();
    };
    addAndMakeVisible(outputFFTToggle);

    contourToggle.setButtonText("Equal-Loudness Contour");
    contourToggle.setToggleState(true, juce::dontSendNotification);
    contourToggle.onClick = [this]() {
        spectrumDisplay.setContourEnabled(contourToggle.getToggleState());
        spectrumDisplay.repaint();
    };
    addAndMakeVisible(contourToggle);

    barkEnergyToggle.setButtonText("Bark Band Energy");
    barkEnergyToggle.setToggleState(true, juce::dontSendNotification);
    barkEnergyToggle.onClick = [this]() {
        spectrumDisplay.setBarkEnergyEnabled(barkEnergyToggle.getToggleState());
        spectrumDisplay.repaint();
    };
    addAndMakeVisible(barkEnergyToggle);

    grCurveToggle.setButtonText("GR Curve");
    grCurveToggle.setToggleState(false, juce::dontSendNotification);
    grCurveToggle.onClick = [this]() {
        gainReductionPanel.setShowGRCurve(grCurveToggle.getToggleState());
    };
    addAndMakeVisible(grCurveToggle);

    // Start UI timer at 60 Hz
    startTimerHz(60);

    // Section registry — remove or reorder entries to change the editor layout.
    sections_ = {
        { &spectrumDisplay,      LayoutMetrics::spectrumHeight },
        { &gainReductionPanel,   LayoutMetrics::gainReductionHeight },
        { &compressorGroup,      LayoutMetrics::groupHeight(9) },
        { &transientShaperGroup, LayoutMetrics::groupHeight(4) },
        { &displayGroup,         LayoutMetrics::groupHeight(3) },
    };

    setSize(LayoutMetrics::defaultEditorWidth, computePreferredEditorHeight());
}

template <typename SampleType>
PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::~PhuBarkFFTCompressorAudioProcessorEditor() {
    stopTimer();
}

// ============================================================================
// Paint
// ============================================================================

template <typename SampleType>
void PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

// ============================================================================
// Layout helpers
// ============================================================================

template <typename SampleType>
int PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::computePreferredEditorHeight() const {
    int visibleCount = 0;
    int total = 2 * LayoutMetrics::outerPadding;
    for (const auto& s : sections_) {
        if (!s.visible) continue;
        total += s.preferredHeight;
        ++visibleCount;
    }
    if (visibleCount > 1)
        total += (visibleCount - 1) * LayoutMetrics::groupGap;
    return total;
}

// ── Internal group layout helpers ───────────────────────────────────────────

namespace {
// Lays out a label+control row and advances the content rect.
void layoutLabelRow(juce::Rectangle<int>& content, juce::Label& label,
                    juce::Component& control) {
    auto row = content.removeFromTop(LayoutMetrics::rowHeight);
    label.setBounds(row.removeFromLeft(LayoutMetrics::labelWidth));
    control.setBounds(row);
    content.removeFromTop(LayoutMetrics::rowGap);
}
} // namespace

// ============================================================================
// Layout
// ============================================================================

template <typename SampleType>
void PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::resized() {
    // ── Outer FlexBox stack (only visible sections) ──────────────────────
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::column;
    fb.flexWrap      = juce::FlexBox::Wrap::noWrap;

    const int lastVisible = [&]() {
        int idx = -1;
        for (int i = 0; i < static_cast<int>(sections_.size()); ++i)
            if (sections_[i].visible) idx = i;
        return idx;
    }();

    for (int i = 0; i < static_cast<int>(sections_.size()); ++i) {
        const auto& s = sections_[i];
        if (!s.visible) {
            s.component->setVisible(false);
            continue;
        }
        s.component->setVisible(true);
        juce::FlexItem item(*s.component);
        item.height = static_cast<float>(s.preferredHeight);
        item.width  = static_cast<float>(getWidth() - 2 * LayoutMetrics::outerPadding);
        // gap below every section except the last
        item.margin = juce::FlexItem::Margin(
            0.0f, 0.0f,
            i != lastVisible ? static_cast<float>(LayoutMetrics::groupGap) : 0.0f,
            0.0f);
        fb.items.add(item);
    }

    fb.performLayout(getLocalBounds().reduced(LayoutMetrics::outerPadding).toFloat());

    // ── Compressor group internals ───────────────────────────────────────
    {
        auto content = compressorGroup.getBounds()
                           .reduced(LayoutMetrics::groupPaddingH, LayoutMetrics::groupPaddingV);
        layoutLabelRow(content, thresholdLabel,   thresholdSlider);
        layoutLabelRow(content, ratioLabel,        ratioSlider);
        layoutLabelRow(content, attackLabel,       attackSlider);
        layoutLabelRow(content, releaseLabel,      releaseSlider);
        layoutLabelRow(content, contourLabel,      contourCombo);
        layoutLabelRow(content, fftModeLabel,      fftModeCombo);
        layoutLabelRow(content, overlapLabel,      overlapCombo);
        // last row — no gap after it
        auto row = content.removeFromTop(LayoutMetrics::rowHeight);
        smoothingLabel.setBounds(row.removeFromLeft(LayoutMetrics::labelWidth));
        smoothingSlider.setBounds(row);
    }

    // ── Transient Shaper group internals ────────────────────────────────
    if (transientShaperGroup.isVisible()) {
        auto content = transientShaperGroup.getBounds()
                           .reduced(LayoutMetrics::groupPaddingH, LayoutMetrics::groupPaddingV);
        layoutLabelRow(content, tsAttackLabel,     tsAttackSlider);
        layoutLabelRow(content, tsSustainLabel,    tsSustainSlider);
        layoutLabelRow(content, tsSensitivityLabel, tsSensitivitySlider);
        auto row = content.removeFromTop(LayoutMetrics::rowHeight);
        tsBypassToggle.setBounds(row);
    }

    // ── Display toggles group internals ─────────────────────────────────
    if (displayGroup.isVisible()) {
        auto content = displayGroup.getBounds()
                           .reduced(LayoutMetrics::groupPaddingH, LayoutMetrics::groupPaddingV);
        const int toggleWidth =
            (content.getWidth() - LayoutMetrics::toggleColumnGap) / 2;

        auto toggleRow = content.removeFromTop(LayoutMetrics::rowHeight);
        inputFFTToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        toggleRow.removeFromLeft(LayoutMetrics::toggleColumnGap);
        outputFFTToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        content.removeFromTop(LayoutMetrics::rowGap);

        toggleRow = content.removeFromTop(LayoutMetrics::rowHeight);
        contourToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        toggleRow.removeFromLeft(LayoutMetrics::toggleColumnGap);
        barkEnergyToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
        content.removeFromTop(LayoutMetrics::rowGap);

        toggleRow = content.removeFromTop(LayoutMetrics::rowHeight);
        grCurveToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
    }
}

// ============================================================================
// Timer: update FFT display at 60 Hz
// ============================================================================

template <typename SampleType>
void PhuBarkFFTCompressorAudioProcessorEditor<SampleType>::timerCallback() {
    // Update sample rate if changed
    double sr = audioProcessor.getSampleRate();
    if (sr > 0.0)
        spectrumDisplay.setSampleRate(sr);

    // Process FFT on UI thread
    if (inputFFTToggle.getToggleState())
        inputFFT.process(audioProcessor.getInputFifo());
    if (outputFFTToggle.getToggleState())
        outputFFT.process(audioProcessor.getOutputFifo());

    // Repaint spectrum and gain reduction
    spectrumDisplay.repaint();
    gainReductionPanel.repaint();
}

template class PhuBarkFFTCompressorAudioProcessorEditor<float>;
