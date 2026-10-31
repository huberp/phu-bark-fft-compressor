#pragma once

/**
 * Editor-side Transient Shaper feature.
 *
 * PHU_TRANSIENT_SHAPER is set by CMake (option TRANSIENT_SHAPER, default OFF). This header is
 * the only place on the editor side that switches between the real panel and a no-op stand-in
 * with the same API, so PluginEditor can use it unconditionally without #ifdefs. When OFF, no
 * section is registered and the editor height shrinks automatically.
 */

#ifndef PHU_TRANSIENT_SHAPER
#error "PHU_TRANSIENT_SHAPER must be defined to 0 or 1 (set by CMake option TRANSIENT_SHAPER)."
#endif

#include "EditorLayout.h"
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#if PHU_TRANSIENT_SHAPER

#include "TransientShaperPanel.h"

/** TransientShaperUi — owns the Transient Shaper panel and plugs it into the editor layout. */
class TransientShaperUi {
  public:
    explicit TransientShaperUi(juce::AudioProcessorValueTreeState& apvts) : panel(apvts) {}

    /** Add the panel to @p parent and append its layout section to @p sections. */
    void addTo(juce::Component& parent, std::vector<LayoutSection>& sections) {
        parent.addAndMakeVisible(panel);
        sections.push_back({&panel, TransientShaperPanel::preferredHeight()});
    }

  private:
    TransientShaperPanel panel;
};

#else

/** TransientShaperUi — no-op stand-in used when TRANSIENT_SHAPER=OFF; adds no component or section. */
class TransientShaperUi {
  public:
    explicit TransientShaperUi(juce::AudioProcessorValueTreeState&) {}
    void addTo(juce::Component&, std::vector<LayoutSection>&) {}
};

#endif
