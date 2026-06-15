#include "PluginProcessor.h"
#include "PluginEditor.h"

PhuBarkFFTCompressorAudioProcessor::PhuBarkFFTCompressorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
    // Cache raw parameter pointers for lock-free audio-thread access
    thresholdParam     = apvts.getRawParameterValue(PARAM_THRESHOLD);
    ratioParam         = apvts.getRawParameterValue(PARAM_RATIO);
    attackParam        = apvts.getRawParameterValue(PARAM_ATTACK);
    releaseParam       = apvts.getRawParameterValue(PARAM_RELEASE);
    contourParam       = apvts.getRawParameterValue(PARAM_CONTOUR);
    fftModeParam       = apvts.getRawParameterValue(PARAM_FFT_MODE);
    overlapModeParam   = apvts.getRawParameterValue(PARAM_OVERLAP_MODE);
    tsAttackParam      = apvts.getRawParameterValue(PARAM_TS_ATTACK);
    tsSustainParam     = apvts.getRawParameterValue(PARAM_TS_SUSTAIN);
    tsSensitivityParam = apvts.getRawParameterValue(PARAM_TS_SENSITIVITY);
    tsBypassParam      = apvts.getRawParameterValue(PARAM_TS_BYPASS);
    smoothingParam     = apvts.getRawParameterValue(PARAM_SMOOTHING);
}

PhuBarkFFTCompressorAudioProcessor::~PhuBarkFFTCompressorAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
PhuBarkFFTCompressorAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_THRESHOLD, 1},
        "Contour Offset",
        juce::NormalisableRange<float>(-60.0f, 20.0f, 0.1f),
        -30.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_RATIO, 1},
        "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f),
        4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_ATTACK, 1},
        "Attack",
        juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f, 0.4f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_RELEASE, 1},
        "Release",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.4f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{PARAM_CONTOUR, 1},
        "Loudness Contour",
        juce::StringArray{
            BarkFFTCompressor::getContourPresetName(BarkFFTCompressor::ContourPreset::ISO226_20Phon),
            BarkFFTCompressor::getContourPresetName(BarkFFTCompressor::ContourPreset::ISO226_40Phon),
            BarkFFTCompressor::getContourPresetName(BarkFFTCompressor::ContourPreset::ISO226_60Phon),
            BarkFFTCompressor::getContourPresetName(BarkFFTCompressor::ContourPreset::ISO226_80Phon),
            BarkFFTCompressor::getContourPresetName(BarkFFTCompressor::ContourPreset::Flat)},
        1)); // Default: 40 phon

    // ── FFT Mode ─────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{PARAM_FFT_MODE, 1},
        "FFT Mode",
        juce::StringArray{"Precision", "Transient"},
        0)); // Default: Precision

    // ── Overlap Mode ─────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{PARAM_OVERLAP_MODE, 1},
        "Overlap",
        juce::StringArray{"50% (Low CPU)", "75% (High Quality)", "90% (Highest Quality)"},
        0)); // Default: 50%

    // ── Transient Shaper ─────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_TS_ATTACK, 1},
        "Attack",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_TS_SUSTAIN, 1},
        "Sustain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_TS_SENSITIVITY, 1},
        "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{PARAM_TS_BYPASS, 1},
        "Bypass Transient Shaper",
        true)); // Default: bypassed

    // ── Smoothing ─────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_SMOOTHING, 1},
        "Smoothing Taps",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f,
        juce::AudioParameterFloatAttributes().withLabel("")));

    return {params.begin(), params.end()};
}

void PhuBarkFFTCompressorAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    // Apply FFT mode before prepare so buffers are sized correctly
    const int fftModeIndex = static_cast<int>(fftModeParam->load());
    lastFFTModeIndex = fftModeIndex;
    m_compressor.setFFTMode(fftModeIndex == 0 ? BarkFFTCompressor::FFTMode::Precision
                                              : BarkFFTCompressor::FFTMode::Transient);

    const int overlapModeIndex = static_cast<int>(overlapModeParam->load());
    lastOverlapModeIndex = overlapModeIndex;
    static const BarkFFTCompressor::OverlapMode overlapModes[] = {
        BarkFFTCompressor::OverlapMode::Half,
        BarkFFTCompressor::OverlapMode::ThreeQuarter,
        BarkFFTCompressor::OverlapMode::Ninety
    };
    m_compressor.setOverlapMode(overlapModes[overlapModeIndex]);

    m_compressor.prepare(sampleRate);

    // Apply current parameter values
    m_compressor.setThresholdDb(thresholdParam->load());
    m_compressor.setRatio(ratioParam->load());
    m_compressor.setAttackMs(attackParam->load());
    m_compressor.setReleaseMs(releaseParam->load());
    m_compressor.setContourPreset(
        static_cast<BarkFFTCompressor::ContourPreset>(static_cast<int>(contourParam->load())));
    m_compressor.setSmoothingAlpha(
        smoothingAmountToAlpha(smoothingParam->load()));

    // Report latency to DAW for compensation
    setLatencySamples(m_compressor.getLatencySamples());

    // Prepare transient shaper
    m_transientShaper.prepare(sampleRate);
    m_transientShaper.setParameters(
        tsAttackParam->load(),
        tsSustainParam->load(),
        tsSensitivityParam->load(),
        tsBypassParam->load() >= 0.5f);

    m_inputFifo.reset();
    m_outputFifo.reset();
}

void PhuBarkFFTCompressorAudioProcessor::releaseResources() {
    m_compressor.reset();
}

void PhuBarkFFTCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have corresponding inputs
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // ── FFT mode change detection ─────────────────────────────────────────
    // If the user switched the FFT mode, re-initialise the compressor.
    // This reallocates buffers (not real-time-safe) but only fires once per
    // mode change — the brief audio glitch (silence for one block) is acceptable
    // for this kind of structural reconfiguration.
    const int fftModeIndex = static_cast<int>(fftModeParam->load());
    if (fftModeIndex != lastFFTModeIndex) {
        lastFFTModeIndex = fftModeIndex;
        m_compressor.setFFTMode(fftModeIndex == 0 ? BarkFFTCompressor::FFTMode::Precision
                                                  : BarkFFTCompressor::FFTMode::Transient);
        m_compressor.prepare(getSampleRate());
        setLatencySamples(m_compressor.getLatencySamples());
    }

    // ── Overlap mode change detection ─────────────────────────────────────
    const int overlapModeIndex = static_cast<int>(overlapModeParam->load());
    if (overlapModeIndex != lastOverlapModeIndex) {
        lastOverlapModeIndex = overlapModeIndex;
        m_compressor.setOverlapMode(overlapModeIndex == 0 ? BarkFFTCompressor::OverlapMode::Half
                                                          : BarkFFTCompressor::OverlapMode::ThreeQuarter);
        m_compressor.prepare(getSampleRate());
        setLatencySamples(m_compressor.getLatencySamples());
    }

    // Update compressor parameters from APVTS (lock-free atomic reads)
    m_compressor.setThresholdDb(thresholdParam->load());
    m_compressor.setRatio(ratioParam->load());
    m_compressor.setAttackMs(attackParam->load());
    m_compressor.setReleaseMs(releaseParam->load());
    m_compressor.setContourPreset(
        static_cast<BarkFFTCompressor::ContourPreset>(static_cast<int>(contourParam->load())));
    m_compressor.setSmoothingAlpha(
        smoothingAmountToAlpha(smoothingParam->load()));

    // Update transient shaper parameters
    m_transientShaper.setParameters(
        tsAttackParam->load(),
        tsSustainParam->load(),
        tsSensitivityParam->load(),
        tsBypassParam->load() >= 0.5f);

    // Push input samples to FIFO for UI display
    const float* inputPtrs[2] = {buffer.getReadPointer(0),
                                  totalNumInputChannels > 1 ? buffer.getReadPointer(1)
                                                            : buffer.getReadPointer(0)};
    m_inputFifo.push(inputPtrs, numSamples);

    // Process sample-by-sample through the FFT compressor
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = totalNumInputChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float inL = leftChannel[i];
        float inR = rightChannel ? rightChannel[i] : inL;

        auto result = m_compressor.processSample(inL, inR);

        // Apply transient shaper as a post-processing stage
        result.left  = m_transientShaper.processSample(result.left);
        result.right = m_transientShaper.processSample(result.right);

        leftChannel[i] = result.left;
        if (rightChannel)
            rightChannel[i] = result.right;
    }

    // Push output samples to FIFO for UI display
    const float* outputPtrs[2] = {buffer.getReadPointer(0),
                                   totalNumInputChannels > 1 ? buffer.getReadPointer(1)
                                                             : buffer.getReadPointer(0)};
    m_outputFifo.push(outputPtrs, numSamples);
}

juce::AudioProcessorEditor* PhuBarkFFTCompressorAudioProcessor::createEditor() {
    return new PhuBarkFFTCompressorAudioProcessorEditor<float>(*this);
}

bool PhuBarkFFTCompressorAudioProcessor::hasEditor() const { return true; }

const juce::String PhuBarkFFTCompressorAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool PhuBarkFFTCompressorAudioProcessor::acceptsMidi() const { return false; }
bool PhuBarkFFTCompressorAudioProcessor::producesMidi() const { return false; }
bool PhuBarkFFTCompressorAudioProcessor::isMidiEffect() const { return false; }
double PhuBarkFFTCompressorAudioProcessor::getTailLengthSeconds() const { return 0.0; }

bool PhuBarkFFTCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

int PhuBarkFFTCompressorAudioProcessor::getNumPrograms() { return 1; }
int PhuBarkFFTCompressorAudioProcessor::getCurrentProgram() { return 0; }
void PhuBarkFFTCompressorAudioProcessor::setCurrentProgram(int /*index*/) {}
const juce::String PhuBarkFFTCompressorAudioProcessor::getProgramName(int /*index*/) {
    return {};
}
void PhuBarkFFTCompressorAudioProcessor::changeProgramName(int /*index*/,
                                                            const juce::String& /*newName*/) {}

void PhuBarkFFTCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhuBarkFFTCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhuBarkFFTCompressorAudioProcessor();
}
