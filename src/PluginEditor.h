#pragma once

#include "../lib/audio/FFTProcessor.h"
#include "SpectrumDisplay.h"
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::FFTProcessor;

class PhuBarkFFTCompressorAudioProcessor;

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
    FFTProcessor inputFFT{11};  // Match compressor FFT order
    FFTProcessor outputFFT{11};

    // Spectrum display component
    SpectrumDisplay spectrumDisplay;

    // Gain reduction meter panel (24 vertical bars)
    class GainReductionPanel : public juce::Component {
      public:
        void paint(juce::Graphics& g) override;
        void setCompressor(const phu::audio::BarkFFTCompressor* comp) { compressorRef = comp; }
      private:
        const phu::audio::BarkFFTCompressor* compressorRef = nullptr;
    };
    GainReductionPanel gainReductionPanel;

    // Parameter controls
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

    // FFT display controls
    juce::GroupComponent displayGroup;
    juce::ToggleButton inputFFTToggle;
    juce::ToggleButton outputFFTToggle;
    juce::ToggleButton contourToggle;
    juce::ToggleButton barkEnergyToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuBarkFFTCompressorAudioProcessorEditor)
};
