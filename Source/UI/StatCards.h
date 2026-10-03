#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Animation.h"
#include "DSP/KeyDetector.h"
#include "DSP/KeyNames.h"

namespace onykey::ui
{

/** The three raised cards under the wheel: CONFIDENCE (LED meter),
    ALSO FITS (runner-up key and how it relates), TUNING (cents strip with a
    needle). Values animate between results. */
class StatCards final : public juce::Component
{
public:
    StatCards()
    {
        confidence.timeConstant = 0.25f;
        tuning.timeConstant = 0.25f;
        presence.timeConstant = 0.15f;
        setInterceptsMouseClicks (false, false);
    }

    void setResult (const KeyResult& r)
    {
        result = r;
        const bool valid = r.isValid();
        confidence.set (valid && r.kind == KeyResult::Kind::Key ? r.confidence : 0.0f);
        tuning.set (valid ? juce::jlimit (-50.0f, 50.0f, r.tuningCents) : 0.0f);
        presence.set (valid ? 1.0f : 0.0f);
    }

    void tick (float dt)
    {
        bool moving = confidence.tick (dt);
        moving |= tuning.tick (dt);
        moving |= presence.tick (dt);
        if (moving)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f).withTrimmedBottom (6.0f);
        constexpr float gap = 12.0f;
        const float w = (area.getWidth() - 2.0f * gap) / 3.0f;

        auto c1 = area.removeFromLeft (w);
        area.removeFromLeft (gap);
        auto c2 = area.removeFromLeft (w);
        area.removeFromLeft (gap);
        auto c3 = area;

        for (auto c : { c1, c2, c3 })
            drawCard (g, c);

        drawConfidence (g, c1.reduced (14.0f, 11.0f));
        drawAltKey (g, c2.reduced (14.0f, 11.0f));
        drawTuning (g, c3.reduced (14.0f, 11.0f));
    }

private:
    static void drawCard (juce::Graphics& g, juce::Rectangle<float> r)
    {
        constexpr float radius = 11.0f;
        Theme::dropShadow (g, r, radius, 0.5f, 12, 4);
        Theme::fillRaised (g, r, radius, Theme::panel);
        juce::Path p;
        p.addRoundedRectangle (r, radius);
        Theme::drawGlassSheen (g, p, 0.035f);
    }

    static void drawLabel (juce::Graphics& g, juce::Rectangle<float>& inner, const juce::String& text)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::labelFont (9.5f));
        g.drawText (text, inner.removeFromTop (12.0f), juce::Justification::centredLeft, false);
        inner.removeFromTop (3.0f);
    }

    void drawConfidence (juce::Graphics& g, juce::Rectangle<float> inner)
    {
        const bool valid = result.isValid();
        const bool rootOnly = result.kind == KeyResult::Kind::RootNote;
        drawLabel (g, inner, rootOnly ? "SOUND" : "CONFIDENCE");

        auto valueRow = inner.removeFromTop (28.0f);
        const float c = confidence.value;
        const float a = 0.35f + 0.65f * presence.value;

        g.setColour (Theme::textPrimary.withAlpha (a));
        g.setFont (Theme::font (rootOnly ? 17.0f : 21.0f, Theme::Weight::DemiBold));
        g.drawText (! valid ? juce::String ("-") : rootOnly ? juce::String ("One note") : confidenceWord (result.confidence),
                    valueRow, juce::Justification::centredLeft, false);

        if (valid && ! rootOnly)
        {
            g.setColour (Theme::textSecondary);
            g.setFont (Theme::font (12.0f, Theme::Weight::Medium));
            g.drawText (juce::String (juce::roundToInt (c * 100.0f)) + "%", valueRow, juce::Justification::centredRight, false);
        }

        // 12-segment LED meter.
        auto meter = inner.removeFromBottom (7.0f);
        constexpr int leds = 12;
        const float ledGap = 2.5f, ledW = (meter.getWidth() - ledGap * (leds - 1)) / (float) leds;
        for (int i = 0; i < leds; ++i)
        {
            auto led = juce::Rectangle<float> (meter.getX() + (float) i * (ledW + ledGap), meter.getY(), ledW, meter.getHeight());
            const float on = juce::jlimit (0.0f, 1.0f, c * leds - (float) i);
            g.setColour (Theme::ledOff);
            g.fillRoundedRectangle (led, 1.5f);
            if (on > 0.0f)
            {
                const auto colour = Theme::accentDim.interpolatedWith (Theme::accentBright, (float) i / (leds - 1));
                g.setColour (Theme::accent.withAlpha (0.18f * on));
                g.fillRoundedRectangle (led.expanded (1.5f), 2.5f);
                g.setColour (colour.withMultipliedAlpha (on));
                g.fillRoundedRectangle (led, 1.5f);
            }
        }

        if (rootOnly)
        {
            g.setColour (Theme::textSecondary);
            g.setFont (Theme::font (10.5f, Theme::Weight::Medium));
            g.drawText ("major/minor unclear", inner, juce::Justification::centredLeft, false);
        }
    }

    void drawAltKey (juce::Graphics& g, juce::Rectangle<float> inner)
    {
        const bool valid = result.isValid();
        const bool rootOnly = result.kind == KeyResult::Kind::RootNote;
        drawLabel (g, inner, rootOnly ? "BEST GUESS" : "ALSO FITS");

        const int key = rootOnly ? result.key : result.altKey;
        auto valueRow = inner.removeFromTop (28.0f);

        g.setColour (Theme::textPrimary.withAlpha (0.35f + 0.65f * presence.value));
        if (valid)
            Theme::drawKeyText (g, shortKeyName (key), valueRow, Theme::font (21.0f, Theme::Weight::DemiBold), juce::Justification::centredLeft);
        else
        {
            g.setFont (Theme::font (21.0f, Theme::Weight::DemiBold));
            g.drawText ("-", valueRow, juce::Justification::centredLeft, false);
        }

        if (valid)
        {
            Theme::drawPill (g, valueRow.removeFromRight (34.0f).withSizeKeepingCentre (34.0f, 17.0f),
                             camelotCode (key), Theme::accentDim.withAlpha (0.55f), Theme::textPrimary, 10.0f);

            const auto relation = rootOnly ? juce::String ("unconfirmed") : keyRelation (result.key, key);
            g.setColour (Theme::textSecondary);
            g.setFont (Theme::font (10.5f, Theme::Weight::Medium));
            g.drawText (relation.isNotEmpty() ? relation : juce::String ("next best match"), inner, juce::Justification::centredLeft, true);
        }
    }

    void drawTuning (juce::Graphics& g, juce::Rectangle<float> inner)
    {
        const bool valid = result.isValid();
        drawLabel (g, inner, "TUNING");

        auto valueRow = inner.removeFromTop (28.0f);
        const int cents = juce::roundToInt (result.tuningCents);

        g.setColour (Theme::textPrimary.withAlpha (0.35f + 0.65f * presence.value));
        g.setFont (Theme::font (21.0f, Theme::Weight::DemiBold));
        g.drawText (valid ? (cents > 0 ? "+" : "") + juce::String (cents) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2")) : juce::String ("-"),
                    valueRow, juce::Justification::centredLeft, false);

        if (valid)
        {
            g.setColour (Theme::textSecondary);
            g.setFont (Theme::font (10.5f, Theme::Weight::Medium));
            g.drawText (std::abs (cents) <= 5 ? "in tune" : (cents > 0 ? "sharp" : "flat"), valueRow, juce::Justification::centredRight, false);
        }

        // -50..+50 cent strip with ticks and a needle.
        auto strip = inner.removeFromBottom (12.0f);
        const float y = strip.getCentreY();
        g.setColour (Theme::ledOff);
        g.fillRoundedRectangle (strip.withSizeKeepingCentre (strip.getWidth(), 3.0f), 1.5f);

        for (int t = -2; t <= 2; ++t)
        {
            const float x = strip.getCentreX() + (float) t / 2.0f * strip.getWidth() * 0.5f;
            const float h = t == 0 ? 10.0f : 6.0f;
            g.setColour (t == 0 ? Theme::textSecondary : Theme::textDim);
            g.fillRect (juce::Rectangle<float> (1.0f, h).withCentre ({ x, y }));
        }

        if (valid)
        {
            const float x = strip.getCentreX() + tuning.value / 50.0f * strip.getWidth() * 0.5f;
            const auto zero = strip.getCentreX();
            g.setColour (Theme::accent.withAlpha (0.6f));
            g.fillRect (juce::Rectangle<float> (juce::jmin (x, zero), y - 1.0f, std::abs (x - zero), 2.0f));

            juce::Path needle;
            needle.addTriangle (x - 4.5f, strip.getY() - 1.0f, x + 4.5f, strip.getY() - 1.0f, x, y + 1.0f);
            Theme::drawGlow (g, needle, Theme::accent, 4.0f, presence.value);
            g.setColour (Theme::accentBright.withAlpha (presence.value));
            g.fillPath (needle);
        }
    }

    static juce::String confidenceWord (float c)
    {
        if (c >= 0.8f) return "High";
        if (c >= 0.6f) return "Good";
        if (c >= 0.4f) return "Fair";
        return "Low";
    }

    KeyResult result;
    Eased confidence, tuning, presence;
};

} // namespace onykey::ui
