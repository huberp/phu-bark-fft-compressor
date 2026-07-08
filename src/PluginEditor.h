#pragma once

#include "audio/FFTProcessor.h"
#include "SpectrumDisplay.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PhuBarkFFTCompressorAudioProcessor;

template <typename SampleType = float>
class PhuBarkFFTCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                  public juce::Timer {
  public:
    PhuBarkFFTCompressorAudioProcessorEditor(PhuBarkFFTCompressorAudioProcessor&);
    ~PhuBarkFFTCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

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

    // ── Transient Shaper controls ────────────────────────────────────────
    juce::GroupComponent transientShaperGroup;

    juce::Slider tsAttackSlider;
    juce::Label tsAttackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tsAttackAttachment;

    juce::Slider tsSustainSlider;
    juce::Label tsSustainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tsSustainAttachment;

    juce::Slider tsSensitivitySlider;
    juce::Label tsSensitivityLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tsSensitivityAttachment;

    juce::ToggleButton tsBypassToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tsBypassAttachment;

    // ── FFT display controls ─────────────────────────────────────────────
    juce::GroupComponent displayGroup;
    juce::ToggleButton inputFFTToggle;
    juce::ToggleButton outputFFTToggle;
    juce::ToggleButton contourToggle;
    juce::ToggleButton barkEnergyToggle;
    juce::ToggleButton grCurveToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuBarkFFTCompressorAudioProcessorEditor)
};
