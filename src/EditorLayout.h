#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * LayoutMetrics — layout constants shared by the editor and its feature panels.
 *
 * All sizes are in pixels. groupHeight() derives the height of a GroupComponent from the
 * number of label/control rows it contains, so panels and the editor stay consistent.
 */
struct LayoutMetrics {
    static constexpr int outerPadding      = 10;
    static constexpr int rowHeight         = 24;
    static constexpr int rowGap            = 4;
    static constexpr int labelWidth        = 110;
    static constexpr int groupPaddingV     = 18;
    static constexpr int groupPaddingH     = 10;
    static constexpr int groupGap          = 6;
    static constexpr int spectrumHeight    = 220;
    static constexpr int gainReductionHeight = 80;
    static constexpr int defaultEditorWidth  = 700;
    static constexpr int toggleColumnGap   = 10;
    static constexpr int levelRowHeight    = 44;  // taller rows give the meter bars visual space
    static constexpr int meterWidth        = 44;  // combined L+R bar strip width
    static constexpr int levelLabelHeight  = 14;  // "IN: -xx.x" text row below meters
    static constexpr int levelGroupHeight  = 2 * groupPaddingV + 2 * levelRowHeight + 2 * rowGap + levelLabelHeight;

    /** Height of a group box holding @p numRows rows of rowHeight, separated by rowGap. */
    static constexpr int groupHeight(int numRows) noexcept {
        return 2 * groupPaddingV + numRows * rowHeight + (numRows - 1) * rowGap;
    }
};

/**
 * LayoutSection — one vertically stacked entry of the editor layout.
 *
 * The editor stacks all visible sections top to bottom and sums their preferredHeight to
 * compute its own height. A section that is never registered takes no space.
 */
struct LayoutSection {
    juce::Component* component  = nullptr;
    int              preferredHeight = 0;
    bool             visible    = true;
};

/** Lay out a label + control row at the top of @p content and advance @p content past it. */
inline void layoutLabelRow(juce::Rectangle<int>& content, juce::Label& label,
                           juce::Component& control) {
    auto row = content.removeFromTop(LayoutMetrics::rowHeight);
    label.setBounds(row.removeFromLeft(LayoutMetrics::labelWidth));
    control.setBounds(row);
    content.removeFromTop(LayoutMetrics::rowGap);
}
