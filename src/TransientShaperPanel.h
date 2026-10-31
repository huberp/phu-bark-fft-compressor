#pragma once

#include "EditorLayout.h"
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * TransientShaperPanel — self-contained Transient Shaper parameter group.
 *
 * Holds the group box, the Attack / Sustain / Sensitivity sliders, the bypass toggle and their
 * APVTS attachments, and lays them out itself. Only compiled when TRANSIENT_SHAPER=ON.
 */
class TransientShaperPanel : public juce::Component {
  public:
    explicit TransientShaperPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

    /** Number of label/control rows inside the group (3 sliders + bypass toggle). */
    static constexpr int kRows = 4;

    /** Height the editor should reserve for this panel. */
    static constexpr int preferredHeight() noexcept { return LayoutMetrics::groupHeight(kRows); }

  private:
    juce::GroupComponent group;

    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;

    juce::Slider sustainSlider;
    juce::Label sustainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;

    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;

    juce::ToggleButton bypassToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransientShaperPanel)
};
