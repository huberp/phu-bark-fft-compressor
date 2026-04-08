#pragma once

#include "../lib/audio/AudioSampleFifo.h"
#include "../lib/audio/BarkFFTCompressor.h"
#include "../lib/audio/TransientShaper.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

using phu::audio::AudioSampleFifo;
using phu::audio::BarkFFTCompressor;
using phu::audio::TransientShaper;

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
    static constexpr const char* PARAM_THRESHOLD   = "threshold";
    static constexpr const char* PARAM_RATIO        = "ratio";
    static constexpr const char* PARAM_ATTACK       = "attack_ms";
    static constexpr const char* PARAM_RELEASE      = "release_ms";
    static constexpr const char* PARAM_CONTOUR      = "contour_preset";
    static constexpr const char* PARAM_FFT_MODE     = "fft_mode";
    static constexpr const char* PARAM_TS_ATTACK    = "ts_attack_db";
    static constexpr const char* PARAM_TS_SUSTAIN   = "ts_sustain_db";
    static constexpr const char* PARAM_TS_SENSITIVITY = "ts_sensitivity";
    static constexpr const char* PARAM_TS_BYPASS    = "ts_bypass";
    static constexpr const char* PARAM_SMOOTHING    = "smoothing";

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Maps smoothing amount [0,1] to IIR alpha: 0=no smoothing, 1=max smoothing. */
    static float smoothingAmountToAlpha(float amount) noexcept {
        return 1.0f - amount * 0.97f; // alpha range [0.03, 1.0]
    }

    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic parameter pointers for audio thread
    std::atomic<float>* thresholdParam    = nullptr;
    std::atomic<float>* ratioParam        = nullptr;
    std::atomic<float>* attackParam       = nullptr;
    std::atomic<float>* releaseParam      = nullptr;
    std::atomic<float>* contourParam      = nullptr;
    std::atomic<float>* fftModeParam      = nullptr;
    std::atomic<float>* tsAttackParam     = nullptr;
    std::atomic<float>* tsSustainParam    = nullptr;
    std::atomic<float>* tsSensitivityParam = nullptr;
    std::atomic<float>* tsBypassParam     = nullptr;
    std::atomic<float>* smoothingParam    = nullptr;

    // Tracks the last applied FFT mode to detect changes in processBlock
    int lastFFTModeIndex = 0;

    // Core DSP
    BarkFFTCompressor m_compressor;
    TransientShaper   m_transientShaper;

    // Lock-free FIFOs for UI display
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_outputFifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuBarkFFTCompressorAudioProcessor)
};
