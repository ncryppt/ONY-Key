#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace onykey::ui
{

/** Themed popups/tooltips and the resize grip in the ONYVA style. (ONY Key's
    buttons draw themselves, see IconButton.h.) */
class OnyvaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OnyvaLookAndFeel() { refreshColours(); }

    /** Re-applies the colours JUCE bakes in via setColour(); called again
        whenever the theme changes. */
    void refreshColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Theme::background);
        setColour (juce::PopupMenu::backgroundColourId, Theme::panelRaised);
        setColour (juce::PopupMenu::textColourId, Theme::textPrimary);
        setColour (juce::PopupMenu::headerTextColourId, Theme::textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha (Theme::currentThemeIsLight ? 0.16f : 0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, Theme::textPrimary);
        setColour (juce::TooltipWindow::backgroundColourId, Theme::panelRaised);
        setColour (juce::TooltipWindow::textColourId, Theme::textPrimary);
        setColour (juce::TooltipWindow::outlineColourId, Theme::hairline);
    }

    juce::Font getPopupMenuFont() override { return Theme::font (14.0f, Theme::Weight::Medium); }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (Theme::panelRaised);
        g.setColour (Theme::hairline);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) override
    {
        g.setFont (Theme::labelFont (10.0f));
        g.setColour (Theme::textDim);
        g.drawText (sectionName, area.withTrimmedLeft (12).withTrimmedTop (4), juce::Justification::centredLeft, false);
    }

    /** Three short diagonal grooves, like a machined grip, instead of the
        stock resize corner. */
    void drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override
    {
        const auto colour = (isMouseOver || isMouseDragging) ? Theme::accent : Theme::textDim;
        for (int i = 0; i < 3; ++i)
        {
            const float inset = 3.0f + (float) i * 4.0f;
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawLine ((float) w - inset + 0.8f, (float) h - 2.2f, (float) w - 2.2f, (float) h - inset + 0.8f, 1.4f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawLine ((float) w - inset, (float) h - 3.0f, (float) w - 3.0f, (float) h - inset, 1.2f);
        }
    }
};

} // namespace onykey::ui
