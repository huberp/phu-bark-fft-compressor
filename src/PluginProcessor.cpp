#include "PluginProcessor.h"
#include "PluginEditor.h"

PhuBarkFFTCompressorAudioProcessor::PhuBarkFFTCompressorAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
    // Cache raw parameter pointers for lock-free audio-thread access
    thresholdParam = apvts.getRawParameterValue(PARAM_THRESHOLD);
    ratioParam     = apvts.getRawParameterValue(PARAM_RATIO);
    attackParam    = apvts.getRawParameterValue(PARAM_ATTACK);
    releaseParam   = apvts.getRawParameterValue(PARAM_RELEASE);
    contourParam   = apvts.getRawParameterValue(PARAM_CONTOUR);
}

PhuBarkFFTCompressorAudioProcessor::~PhuBarkFFTCompressorAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
PhuBarkFFTCompressorAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{PARAM_THRESHOLD, 1},
        "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -20.0f,
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

    return {params.begin(), params.end()};
}

void PhuBarkFFTCompressorAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    m_compressor.prepare(sampleRate);

    // Apply current parameter values
    m_compressor.setThresholdDb(thresholdParam->load());
    m_compressor.setRatio(ratioParam->load());
    m_compressor.setAttackMs(attackParam->load());
    m_compressor.setReleaseMs(releaseParam->load());
    m_compressor.setContourPreset(
        static_cast<BarkFFTCompressor::ContourPreset>(static_cast<int>(contourParam->load())));

    // Report latency to DAW for compensation
    setLatencySamples(m_compressor.getLatencySamples());

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

    // Update compressor parameters from APVTS (lock-free atomic reads)
    m_compressor.setThresholdDb(thresholdParam->load());
    m_compressor.setRatio(ratioParam->load());
    m_compressor.setAttackMs(attackParam->load());
    m_compressor.setReleaseMs(releaseParam->load());
    m_compressor.setContourPreset(
        static_cast<BarkFFTCompressor::ContourPreset>(static_cast<int>(contourParam->load())));

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
    return new PhuBarkFFTCompressorAudioProcessorEditor(*this);
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
