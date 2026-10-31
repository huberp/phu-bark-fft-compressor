#include "TransientShaperPanel.h"
#include "TransientShaperStage.h"

TransientShaperPanel::TransientShaperPanel(juce::AudioProcessorValueTreeState& apvts) {
    // ── Group box ───────────────────────────────────────────────────────
    group.setText("Transient Shaper");
    group.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(group);

    // ── Attack ──────────────────────────────────────────────────────────
    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centredLeft);
    attackLabel.setTooltip("Boost or attenuate the attack portion of detected transients.");
    addAndMakeVisible(attackLabel);

    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    attackSlider.setTextValueSuffix(" dB");
    attackSlider.setTooltip("Boost or attenuate the attack portion of detected transients.");
    addAndMakeVisible(attackSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, TransientShaperStage::PARAM_TS_ATTACK, attackSlider);

    // ── Sustain ─────────────────────────────────────────────────────────
    sustainLabel.setText("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType(juce::Justification::centredLeft);
    sustainLabel.setTooltip("Boost or attenuate the sustained portion of the signal.");
    addAndMakeVisible(sustainLabel);

    sustainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    sustainSlider.setTextValueSuffix(" dB");
    sustainSlider.setTooltip("Boost or attenuate the sustained portion of the signal.");
    addAndMakeVisible(sustainSlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, TransientShaperStage::PARAM_TS_SUSTAIN, sustainSlider);

    // ── Sensitivity ─────────────────────────────────────────────────────
    sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.setJustificationType(juce::Justification::centredLeft);
    sensitivityLabel.setTooltip("Threshold for detecting transients (0 = least sensitive, 100 = most sensitive).");
    addAndMakeVisible(sensitivityLabel);

    sensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    sensitivitySlider.setTextValueSuffix(" %");
    sensitivitySlider.setTooltip("Threshold for detecting transients (0 = least sensitive, 100 = most sensitive).");
    addAndMakeVisible(sensitivitySlider);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, TransientShaperStage::PARAM_TS_SENSITIVITY, sensitivitySlider);

    // ── Bypass ──────────────────────────────────────────────────────────
    bypassToggle.setButtonText("Bypass Transient Shaper");
    bypassToggle.setTooltip("When enabled, bypasses the transient shaper (passes audio through unchanged).");
    addAndMakeVisible(bypassToggle);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, TransientShaperStage::PARAM_TS_BYPASS, bypassToggle);
}

void TransientShaperPanel::resized() {
    // The panel is sized by the editor's FlexBox; the group box fills it and the rows sit
    // inside the group's padding, in panel-local coordinates.
    group.setBounds(getLocalBounds());

    auto content = getLocalBounds().reduced(LayoutMetrics::groupPaddingH, LayoutMetrics::groupPaddingV);
    layoutLabelRow(content, attackLabel,      attackSlider);
    layoutLabelRow(content, sustainLabel,     sustainSlider);
    layoutLabelRow(content, sensitivityLabel, sensitivitySlider);
    bypassToggle.setBounds(content.removeFromTop(LayoutMetrics::rowHeight));
}
