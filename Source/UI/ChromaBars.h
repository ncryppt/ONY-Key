#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Animation.h"
#include "DSP/KeyDetector.h"
#include "DSP/KeyNames.h"

namespace onykey::ui
{

/** Twelve segmented LED columns in a recessed well, one per pitch class,
    showing how much of each note the detector heard. Notes in the detected
    scale light in the accent and the rest stay grey, so you can see at a
    glance why it chose that key. Note names are spelled for the key (Bb in
    G minor, A# in F# minor) and the tonic is marked. */
class ChromaBars final : public juce::Component
{
public:
    ChromaBars()
    {
        for (auto& l : levels)
            l.timeConstant = 0.14f;
    }

    void setResult (const KeyResult& r)
    {
        result = r;
        scale.fill (false);

        for (size_t i = 0; i < 12; ++i)
            levels[i].set (r.isValid() ? r.chroma[i] : 0.0f);

        if (r.isValid())
        {
            static constexpr int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
            static constexpr int minorSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
            const auto& steps = isMinorKey (r.key) ? minorSteps : majorSteps;

            if (r.kind == KeyResult::Kind::RootNote)
                scale[(size_t) r.rootNote] = true;
            else
                for (int s : steps)
                    scale[(size_t) ((tonicOf (r.key) + s) % 12)] = true;
        }
    }

    void tick (float dt)
    {
        bool moving = false;
        for (auto& l : levels)
            moving |= l.tick (dt);
        if (moving)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        Theme::fillRecessed (g, bounds, 12.0f);

        auto area = bounds.reduced (14.0f, 12.0f);
        auto labels = area.removeFromBottom (18.0f);
        area.removeFromBottom (6.0f);

        constexpr int segments = 14;
        const float slot = area.getWidth() / 12.0f;
        const float colWidth = juce::jmin (20.0f, slot * 0.58f);
        const float segGap = 2.0f;
        const float segHeight = (area.getHeight() - segGap * (segments - 1)) / (float) segments;
        const bool valid = result.isValid();
        const int tonic = valid ? result.rootNote : -1;

        for (int i = 0; i < 12; ++i)
        {
            const float cx = area.getX() + slot * ((float) i + 0.5f);
            const bool inScale = scale[(size_t) i];
            const float v = juce::jlimit (0.0f, 1.0f, levels[(size_t) i].value);
            const float litSegments = v * (float) segments;

            for (int s = 0; s < segments; ++s)
            {
                auto seg = juce::Rectangle<float> (colWidth, segHeight)
                               .withCentre ({ cx, area.getBottom() - segHeight * 0.5f - (float) s * (segHeight + segGap) });
                const float on = juce::jlimit (0.0f, 1.0f, litSegments - (float) s);
                const float heightT = (float) s / (float) (segments - 1);

                // Unlit LED.
                g.setColour (Theme::ledOff);
                g.fillRoundedRectangle (seg, 1.5f);

                if (on > 0.0f)
                {
                    const auto colour = inScale ? Theme::accentDim.interpolatedWith (Theme::accentBright, 0.25f + 0.75f * heightT)
                                                : Theme::ledNeutralLow.interpolatedWith (Theme::ledNeutralHigh, heightT);
                    if (inScale)
                    {
                        g.setColour (Theme::accent.withAlpha (0.16f * on));
                        g.fillRoundedRectangle (seg.expanded (2.5f, 2.0f), 3.0f);
                    }
                    g.setColour (colour.withMultipliedAlpha (on));
                    g.fillRoundedRectangle (seg, 1.5f);
                    // Specular line along each lit LED's top edge.
                    g.setColour (juce::Colours::white.withAlpha (0.18f * on));
                    g.fillRect (seg.withHeight (0.8f).reduced (1.5f, 0.0f));
                }
            }

            // Note name, spelled for the key; the tonic gets a chip.
            const auto name = valid ? noteNameInKey (i, result.key) : noteName (i);
            const auto labelArea = juce::Rectangle<float> (slot, labels.getHeight()).withCentre ({ cx, labels.getCentreY() });

            if (i == tonic)
            {
                Theme::drawPill (g, labelArea.withSizeKeepingCentre (juce::jmin (slot - 2.0f, 28.0f), 16.0f),
                                 name, Theme::accent, Theme::onAccent, 10.5f);
                continue;
            }

            g.setFont (Theme::font (10.5f, inScale ? Theme::Weight::DemiBold : Theme::Weight::Medium));
            g.setColour (inScale ? Theme::textPrimary : Theme::textDim);
            g.drawText (name, labelArea, juce::Justification::centred, false);
        }
    }

private:
    KeyResult result;
    std::array<Eased, 12> levels;
    std::array<bool, 12> scale {};
};

} // namespace onykey::ui
