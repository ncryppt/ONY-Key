#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Animation.h"

namespace onykey::ui
{

/** Raised hardware-style button: drawn icon + spaced caps label, a hover
    rim, a pressed-in state, and (for LISTEN) an LED that glows red while
    active and flickers with the incoming level. */
class IconButton final : public juce::Button
{
public:
    enum class Icon { Folder, Listen, Clear };

    IconButton (const juce::String& name, Icon iconType, bool hasLed = false)
        : juce::Button (name), icon (iconType), led (hasLed)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        hover.timeConstant = 0.08f;
        ledGlow.timeConstant = 0.06f;
    }

    /** LED brightness 0..1 (only drawn when constructed with hasLed). */
    void setLedLevel (float level) { ledGlow.set (juce::jlimit (0.0f, 1.0f, level)); }

    void tick (float dt)
    {
        bool moving = hover.tick (dt);
        moving |= ledGlow.tick (dt);
        if (moving)
            repaint();
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        hover.set (isHighlighted && isEnabled() ? 1.0f : 0.0f);

        auto r = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
        const float radius = r.getHeight() * 0.5f;
        const bool on = getToggleState();
        const float enabledAlpha = isEnabled() ? 1.0f : 0.35f;

        if (! isDown)
            Theme::dropShadow (g, r, radius, 0.55f, 8, 3);

        if (isDown)
        {
            Theme::fillRecessed (g, r, radius);
        }
        else
        {
            Theme::fillRaised (g, r, radius, on ? Theme::accent.interpolatedWith (Theme::panelRaised, Theme::currentThemeIsLight ? 0.82f : 0.72f) : Theme::panelRaised);
            if (hover.value > 0.01f || on)
            {
                g.setColour (Theme::accent.withAlpha (on ? 0.55f : 0.35f * hover.value));
                g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            }
        }

        // Icon + label, centred as a group.
        const juce::String text = getButtonText();
        const auto labelFont = Theme::labelFont (11.0f);
        const float textW = juce::GlyphArrangement::getStringWidth (labelFont, text);
        const float iconSize = 14.0f, spacing = 9.0f;
        const float ledSpace = led ? 14.0f : 0.0f;
        const float groupW = ledSpace + iconSize + spacing + textW;
        float x = r.getCentreX() - groupW * 0.5f;
        const float cy = r.getCentreY() + (isDown ? 1.0f : 0.0f);

        const auto ink = (on ? Theme::textPrimary : Theme::textSecondary.interpolatedWith (Theme::textPrimary, hover.value))
                             .withMultipliedAlpha (enabledAlpha);

        if (led)
        {
            const auto centre = juce::Point<float> (x + 3.5f, cy);
            const float lv = ledGlow.value;
            g.setColour (Theme::listenRed.withAlpha (0.25f * lv));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (centre));
            g.setColour (Theme::listenRed.interpolatedWith (Theme::ledOff, 0.75f).interpolatedWith (Theme::listenRed, lv));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre));
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.4f * lv));
            g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (centre.translated (-1.0f, -1.2f)));
            x += ledSpace;
        }

        drawIcon (g, juce::Rectangle<float> (x, cy - iconSize * 0.5f, iconSize, iconSize), ink);
        x += iconSize + spacing;

        g.setColour (ink);
        g.setFont (labelFont);
        g.drawText (text, juce::Rectangle<float> (x, cy - 8.0f, textW + 4.0f, 16.0f), juce::Justification::centredLeft, false);
    }

private:
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour colour) const
    {
        juce::Path p;
        const juce::PathStrokeType stroke (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        switch (icon)
        {
            case Icon::Folder:
            {
                const auto f = b.reduced (0.5f, 2.0f);
                p.startNewSubPath (f.getX(), f.getY() + 1.5f);
                p.lineTo (f.getX() + f.getWidth() * 0.38f, f.getY() + 1.5f);
                p.lineTo (f.getX() + f.getWidth() * 0.5f, f.getY() + 3.5f);
                p.lineTo (f.getRight(), f.getY() + 3.5f);
                p.lineTo (f.getRight(), f.getBottom());
                p.lineTo (f.getX(), f.getBottom());
                p.closeSubPath();
                g.setColour (colour);
                g.strokePath (p, stroke);
                break;
            }

            case Icon::Listen:
            {
                // Waveform bars.
                const float heights[] = { 0.35f, 0.75f, 1.0f, 0.6f, 0.3f };
                for (int i = 0; i < 5; ++i)
                {
                    const float x = b.getX() + 1.0f + (float) i * (b.getWidth() - 2.0f) / 4.0f;
                    const float h = b.getHeight() * heights[i] * (getToggleState() ? 0.6f + 0.4f * ledGlow.value : 1.0f);
                    p.startNewSubPath (x, b.getCentreY() - h * 0.5f);
                    p.lineTo (x, b.getCentreY() + h * 0.5f);
                }
                g.setColour (colour);
                g.strokePath (p, stroke);
                break;
            }

            case Icon::Clear:
            {
                // Counter-clockwise "reset" arrow.
                const auto c = b.getCentre();
                const float rr = b.getWidth() * 0.4f;
                p.addCentredArc (c.x, c.y, rr, rr, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
                g.setColour (colour);
                g.strokePath (p, stroke);
                const auto tip = c.getPointOnCircumference (rr, 0.6f);
                juce::Path head;
                head.addTriangle (tip.translated (-3.5f, -2.0f), tip.translated (2.0f, -3.5f), tip.translated (1.0f, 2.5f));
                g.fillPath (head);
                break;
            }
        }
    }

    Icon icon;
    bool led;
    Eased hover, ledGlow;
};

} // namespace onykey::ui
