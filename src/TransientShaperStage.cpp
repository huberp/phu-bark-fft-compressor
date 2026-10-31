#include "TransientShaperStage.h"

/** Parameter ranges mirror TransientShaper::setParameters(): ±24 dB gains, 0..100 % sensitivity. */
void TransientShaperStage::addParameters(
    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
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
}

void TransientShaperStage::cacheParams(juce::AudioProcessorValueTreeState& apvts) {
    m_attackParam      = apvts.getRawParameterValue(PARAM_TS_ATTACK);
    m_sustainParam     = apvts.getRawParameterValue(PARAM_TS_SUSTAIN);
    m_sensitivityParam = apvts.getRawParameterValue(PARAM_TS_SENSITIVITY);
    m_bypassParam      = apvts.getRawParameterValue(PARAM_TS_BYPASS);
}

void TransientShaperStage::prepare(double sampleRate) {
    m_shaper.prepare(sampleRate);
    // prepare() resets the envelopes; re-apply parameters so the first block starts consistent.
    updateParams();
}

void TransientShaperStage::updateParams() {
    // Atomic loads only — safe on the audio thread.
    m_shaper.setParameters(
        m_attackParam->load(),
        m_sustainParam->load(),
        m_sensitivityParam->load(),
        m_bypassParam->load() >= 0.5f);
}
