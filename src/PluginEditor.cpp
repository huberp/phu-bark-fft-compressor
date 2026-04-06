#include "PluginEditor.h"
#include "PluginProcessor.h"

// ============================================================================
// GainReductionPanel
// ============================================================================

void PhuBarkFFTCompressorAudioProcessorEditor::GainReductionPanel::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xFF1A1A2Eu));
    g.fillRect(bounds);

    // Title
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(10.0f));
    g.drawText("Gain Reduction (dB)", bounds.removeFromTop(14), juce::Justification::centred);

    if (!compressorRef) return;

    const int numBands = phu::audio::BarkFFTCompressor::NUM_BARK_BANDS;
    float bandWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(numBands);
    float maxGR = 30.0f; // Max gain reduction displayed (dB)

    for (int band = 0; band < numBands; ++band) {
        float gr = compressorRef->getBandGainReductionDb(band); // negative = reduction
        float grAbs = std::abs(gr);
        float normalized = std::min(grAbs / maxGR, 1.0f);

        float x = static_cast<float>(bounds.getX()) + static_cast<float>(band) * bandWidth;
        float barHeight = normalized * static_cast<float>(bounds.getHeight());

        // Draw from top down (gain reduction is shown as bars growing downward)
        juce::Colour barColour = SpectrumDisplay::getBandColour(band);

        if (barHeight > 0.5f) {
            g.setColour(barColour.withAlpha(0.7f));
            g.fillRect(x + 1.0f, static_cast<float>(bounds.getY()),
                       bandWidth - 2.0f, barHeight);
        }

        // Band number label at bottom
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(8.0f));
        g.drawText(juce::String(band + 1),
                   static_cast<int>(x), bounds.getBottom() - 10,
                   static_cast<int>(bandWidth), 10,
                   juce::Justification::centred);
    }

    // Scale labels
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(juce::Font(9.0f));
    g.drawText("0", bounds.getX() - 18, bounds.getY() - 4, 16, 12,
               juce::Justification::centredRight);
    g.drawText(juce::String(static_cast<int>(-maxGR)),
               bounds.getX() - 24, bounds.getBottom() - 8, 22, 12,
               juce::Justification::centredRight);
}

// ============================================================================
// Editor Construction
// ============================================================================

PhuBarkFFTCompressorAudioProcessorEditor::PhuBarkFFTCompressorAudioProcessorEditor(
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
    thresholdLabel.setText("Threshold", juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(thresholdLabel);

    thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    thresholdSlider.setTextValueSuffix(" dB");
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

    contourCombo.addItem(
        phu::audio::BarkFFTCompressor::getContourPresetName(
            phu::audio::BarkFFTCompressor::ContourPreset::ISO226_20Phon), 1);
    contourCombo.addItem(
        phu::audio::BarkFFTCompressor::getContourPresetName(
            phu::audio::BarkFFTCompressor::ContourPreset::ISO226_40Phon), 2);
    contourCombo.addItem(
        phu::audio::BarkFFTCompressor::getContourPresetName(
            phu::audio::BarkFFTCompressor::ContourPreset::ISO226_60Phon), 3);
    contourCombo.addItem(
        phu::audio::BarkFFTCompressor::getContourPresetName(
            phu::audio::BarkFFTCompressor::ContourPreset::ISO226_80Phon), 4);
    contourCombo.addItem(
        phu::audio::BarkFFTCompressor::getContourPresetName(
            phu::audio::BarkFFTCompressor::ContourPreset::Flat), 5);
    addAndMakeVisible(contourCombo);
    contourAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), PhuBarkFFTCompressorAudioProcessor::PARAM_CONTOUR,
        contourCombo);

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

    // Start UI timer at 60 Hz
    startTimerHz(60);

    // Set editor size
    setSize(700, 620);
}

PhuBarkFFTCompressorAudioProcessorEditor::~PhuBarkFFTCompressorAudioProcessorEditor() {
    stopTimer();
}

// ============================================================================
// Paint
// ============================================================================

void PhuBarkFFTCompressorAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

// ============================================================================
// Layout
// ============================================================================

void PhuBarkFFTCompressorAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    // Spectrum display (top section)
    spectrumDisplay.setBounds(area.removeFromTop(220));
    area.removeFromTop(6);

    // Gain reduction meters
    gainReductionPanel.setBounds(area.removeFromTop(80));
    area.removeFromTop(8);

    // ── Layout constants ────────────────────────────────────────────────
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kGroupPaddingV = 18;
    constexpr int kGroupPaddingH = 10;
    constexpr int kGroupSpacing = 8;
    constexpr int kLabelWidth = 110;

    // Helper lambda: group height for N rows
    auto groupHeight = [&](int numRows) {
        return 2 * kGroupPaddingV + numRows * kRowHeight + (numRows - 1) * kRowGap;
    };

    // ── Compressor controls group (5 rows) ──────────────────────────────

    auto compGroupArea = area.removeFromTop(groupHeight(5));
    compressorGroup.setBounds(compGroupArea);
    auto compContent = compGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);

    // Threshold row
    auto row = compContent.removeFromTop(kRowHeight);
    thresholdLabel.setBounds(row.removeFromLeft(kLabelWidth));
    thresholdSlider.setBounds(row);
    compContent.removeFromTop(kRowGap);

    // Ratio row
    row = compContent.removeFromTop(kRowHeight);
    ratioLabel.setBounds(row.removeFromLeft(kLabelWidth));
    ratioSlider.setBounds(row);
    compContent.removeFromTop(kRowGap);

    // Attack row
    row = compContent.removeFromTop(kRowHeight);
    attackLabel.setBounds(row.removeFromLeft(kLabelWidth));
    attackSlider.setBounds(row);
    compContent.removeFromTop(kRowGap);

    // Release row
    row = compContent.removeFromTop(kRowHeight);
    releaseLabel.setBounds(row.removeFromLeft(kLabelWidth));
    releaseSlider.setBounds(row);
    compContent.removeFromTop(kRowGap);

    // Contour row
    row = compContent.removeFromTop(kRowHeight);
    contourLabel.setBounds(row.removeFromLeft(kLabelWidth));
    contourCombo.setBounds(row);

    area.removeFromTop(kGroupSpacing);

    // ── Display toggles group (2 rows) ──────────────────────────────────

    auto displayGroupArea = area.removeFromTop(groupHeight(2));
    displayGroup.setBounds(displayGroupArea);
    auto displayContent = displayGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);

    // Row 1: Input FFT | Output FFT
    auto toggleRow = displayContent.removeFromTop(kRowHeight);
    int toggleWidth = (toggleRow.getWidth() - 10) / 2;
    inputFFTToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
    toggleRow.removeFromLeft(10);
    outputFFTToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
    displayContent.removeFromTop(kRowGap);

    // Row 2: Contour | Bark Energy
    toggleRow = displayContent.removeFromTop(kRowHeight);
    contourToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
    toggleRow.removeFromLeft(10);
    barkEnergyToggle.setBounds(toggleRow.removeFromLeft(toggleWidth));
}

// ============================================================================
// Timer: update FFT display at 60 Hz
// ============================================================================

void PhuBarkFFTCompressorAudioProcessorEditor::timerCallback() {
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
