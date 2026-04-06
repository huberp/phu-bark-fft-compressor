#pragma once

#include "../lib/audio/AudioSampleFifo.h"
#include "../lib/audio/BarkFFTCompressor.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::AudioSampleFifo;
using phu::audio::BarkFFTCompressor;

class PhuBarkFFTCompressorAudioProcessor : public juce::AudioProcessor {
  public:
    PhuBarkFFTCompressorAudioProcessor();
    ~PhuBarkFFTCompressorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter tree state for automatable parameters
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Lock-free FIFOs for UI spectrum display
    AudioSampleFifo<2>& getInputFifo() { return m_inputFifo; }
    AudioSampleFifo<2>& getOutputFifo() { return m_outputFifo; }

    // Access the compressor for UI visualization
    const BarkFFTCompressor& getCompressor() const { return m_compressor; }

    // Parameter IDs
    static constexpr const char* PARAM_THRESHOLD = "threshold";
    static constexpr const char* PARAM_RATIO     = "ratio";
    static constexpr const char* PARAM_ATTACK    = "attack_ms";
    static constexpr const char* PARAM_RELEASE   = "release_ms";
    static constexpr const char* PARAM_CONTOUR   = "contour_preset";

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic parameter pointers for audio thread
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam     = nullptr;
    std::atomic<float>* attackParam    = nullptr;
    std::atomic<float>* releaseParam   = nullptr;
    std::atomic<float>* contourParam   = nullptr;

    // Core DSP
    BarkFFTCompressor m_compressor;

    // Lock-free FIFOs for UI display
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_outputFifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuBarkFFTCompressorAudioProcessor)
};
