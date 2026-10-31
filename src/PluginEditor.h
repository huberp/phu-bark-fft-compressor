#pragma once

#include "audio/FFTProcessor.h"
#include "EditorLayout.h"
#include "LevelMeterComponent.h"
#include "SpectrumDisplay.h"
#include "TransientShaperUI.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PhuBarkFFTCompressorAudioProcessor;

/**
 * PhuBarkFFTCompressorAudioProcessorEditor — plugin editor.
 *
 * Stacks its sections (spectrum, gain reduction, level, compressor, optional Transient Shaper,
 * display) top to bottom from the sections_ registry; the editor height is the sum of the
 * registered sections, so an unregistered section needs no special handling.
 */
template <typename SampleType = float>
class PhuBarkFFTCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                  public juce::Timer {
  public:
    PhuBarkFFTCompressorAudioProcessorEditor(PhuBarkFFTCompressorAudioProcessor&);
    ~PhuBarkFFTCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    /** Sum of all visible section heights, group gaps and outer padding. */
    int computePreferredEditorHeight() const;

  private:
    PhuBarkFFTCompressorAudioProcessor& audioProcessor;

    // FFT processors for spectrum visualization (UI thread only)
    phu::audio::FFTProcessor<SampleType> inputFFT{11};  // Match compressor FFT order
    phu::audio::FFTProcessor<SampleType> outputFFT{11};

    // Spectrum display component
    SpectrumDisplay<SampleType> spectrumDisplay;

    // Gain reduction meter panel (24 vertical bars)
    class GainReductionPanel : public juce::Component {
      public:
        void paint(juce::Graphics& g) override;
        void setCompressor(const phu::audio::BarkFFTCompressor* comp) { compressorRef = comp; }
        void setShowGRCurve(bool v) { showGRCurve = v; }
      private:
        const phu::audio::BarkFFTCompressor* compressorRef = nullptr;
        bool showGRCurve = false;
    };
    GainReductionPanel gainReductionPanel;
    // ── I/O Level & Gain group ───────────────────────────────────────────────
    juce::GroupComponent levelGroup;

    LevelMeterComponent inputMeter;
    juce::Label         inputGainLabel;
    juce::Slider        inputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;

    LevelMeterComponent outputMeter;
    juce::Label         outputGainLabel;
    juce::Slider        outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    juce::Label inputLevelLabel;   // "IN: -xx.x" — repaints only when value changes
    juce::Label outputLevelLabel;  // "OUT: -xx.x"
    // ── Compressor controls ──────────────────────────────────────────────
    juce::GroupComponent compressorGroup;

    juce::Slider thresholdSlider;
    juce::Label thresholdLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;

    juce::Slider ratioSlider;
    juce::Label ratioLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttachment;

    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;

    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    juce::ComboBox contourCombo;
    juce::Label contourLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> contourAttachment;

    juce::ComboBox fftModeCombo;
    juce::Label fftModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftModeAttachment;

    juce::ComboBox overlapCombo;
    juce::Label overlapLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> overlapAttachment;

    juce::Slider smoothingSlider;
    juce::Label smoothingLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;

    // Optional Transient Shaper panel; a no-op unless built with TRANSIENT_SHAPER=ON.
    // Declared after audioProcessor, which supplies the APVTS it is constructed from.
    TransientShaperUi transientShaperUi_;

    // ── FFT display controls ─────────────────────────────────────────────
    juce::GroupComponent displayGroup;
    juce::ToggleButton inputFFTToggle;
    juce::ToggleButton outputFFTToggle;
    juce::ToggleButton contourToggle;
    juce::ToggleButton barkEnergyToggle;
    juce::ToggleButton grCurveToggle;

    // Ordered list of sections; remove/add entries to change editor layout.
    std::vector<LayoutSection> sections_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuBarkFFTCompressorAudioProcessorEditor)
};
