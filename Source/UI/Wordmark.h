#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Theme.h"
#include "Animation.h"
#include "Shapes.h"

namespace onykey::ui
{

/** The ONY KEY logo, painted in the UI's materials rather than as a flat
    bitmap: the wordmark is a mask filled with a brushed-metal gradient over
    a cast shadow, and the keyhole cut into the "O" is lit from behind in
    the accent, like light coming through a lock.

    Novelty themes add an emblem beside the logo, as in ONY Verb: a leaf for
    Kush Koma, the maple leaf for Canada Eh?.

    The keyhole doubles as a status light: a dim ember when idle, fully lit
    (with a flare) once a key is found, pulsing while analysing, and red
    with the input level while listening.

    Masks come from Resources/ (generated from the source logo by
    scripts/make_logo_assets.py). */
class Wordmark final : public juce::Component
{
public:
    enum class Mode { Idle, Found, Analysing, Listening };

    Wordmark()
    {
        setInterceptsMouseClicks (false, false);
        logo = juce::ImageCache::getFromMemory (BinaryData::ONYKey_LogoMask_png, BinaryData::ONYKey_LogoMask_pngSize);
        keyhole = juce::ImageCache::getFromMemory (BinaryData::ONYKey_KeyholeMask_png, BinaryData::ONYKey_KeyholeMask_pngSize);
        keyholeBounds = opaqueBounds (keyhole);
        mapleLeaf = juce::ImageCache::getFromMemory (BinaryData::CanadaMapleLeaf_png, BinaryData::CanadaMapleLeaf_pngSize);
        glow.timeConstant = 0.18f;
        red.timeConstant = 0.15f;
    }

    void setMode (Mode m, float inputLevel = 0.0f)
    {
        if (m == Mode::Found && mode != Mode::Found)
            flare = 1.0f;
        mode = m;
        level = inputLevel;
    }

    void tick (float dt)
    {
        phase += dt;
        flare = juce::jmax (0.0f, flare - dt / 0.7f);

        float target = 0.3f;
        switch (mode)
        {
            case Mode::Idle:      target = 0.3f + 0.06f * std::sin (phase * 1.3f); break; // slow breathing ember
            case Mode::Found:     target = 1.0f; break;
            case Mode::Analysing: target = 0.55f + 0.4f * (0.5f + 0.5f * std::sin (phase * 7.0f)); break;
            case Mode::Listening: target = 0.5f + 0.5f * level; break;
        }
        glow.set (target);
        red.set (mode == Mode::Listening ? 1.0f : 0.0f);

        bool moving = glow.tick (dt);
        moving |= red.tick (dt);
        if (moving || flare > 0.0f)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! logo.isValid())
            return;

        // Fit the logo, keeping its aspect ratio, left-aligned.
        const auto bounds = getLocalBounds().toFloat().reduced (2.0f, 4.0f);
        const float scale = juce::jmin (bounds.getWidth() / (float) logo.getWidth(), bounds.getHeight() / (float) logo.getHeight());
        const auto place = juce::AffineTransform::scale (scale).translated (bounds.getX(), bounds.getCentreY() - (float) logo.getHeight() * scale * 0.5f);
        const auto logoArea = logo.getBounds().toFloat().transformedBy (place);
        const auto holeArea = keyholeBounds.transformedBy (place);

        const auto light = Theme::accent.interpolatedWith (Theme::listenRed, red.value);
        const float intensity = juce::jlimit (0.0f, 1.0f, glow.value + 0.6f * flare);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        // Light spilling out of the keyhole onto the panel behind the logo.
        {
            const auto c = holeArea.getCentre();
            const float r = holeArea.getHeight() * (1.6f + 0.8f * flare);
            g.setGradientFill (juce::ColourGradient (light.withAlpha (0.22f * intensity), c.x, c.y,
                                                     light.withAlpha (0.0f), c.x + r, c.y, true));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        }

        // Cast shadow.
        g.setColour (juce::Colours::black.withAlpha (0.65f * Theme::shadowStrength));
        g.drawImageTransformed (logo, place.translated (0.0f, 1.6f), true);

        // Brushed metal: bright top edge, cooler towards the bottom, with a
        // faint highlight band across the upper letters.
        // (Graphite on light themes, so it never washes out.)
        juce::ColourGradient metal (Theme::metalTop, 0.0f, logoArea.getY(),
                                    Theme::metalBottom, 0.0f, logoArea.getBottom(), false);
        metal.addColour (0.18, Theme::metalTop.interpolatedWith (Theme::metalMid, 0.2f));
        metal.addColour (0.42, Theme::metalMid);
        metal.addColour (0.52, Theme::metalTop.interpolatedWith (Theme::metalMid, 0.45f));
        metal.addColour (0.62, Theme::metalMid.interpolatedWith (Theme::metalBottom, 0.4f));
        g.setGradientFill (metal);
        g.drawImageTransformed (logo, place, true);

        // The keyhole, lit from behind: hot at the top of the bowl, deeper
        // towards the foot.
        juce::ColourGradient lit (light.interpolatedWith (juce::Colours::white, 0.55f), 0.0f, holeArea.getY(),
                                  light.darker (0.6f), 0.0f, holeArea.getBottom(), false);
        lit.addColour (0.35, light.brighter (0.15f));
        g.setGradientFill (lit);
        g.setOpacity (0.25f + 0.75f * intensity);
        g.drawImageTransformed (keyhole, place, true);
        g.setOpacity (1.0f);

        // A touch of the light on the metal around the hole.
        {
            const auto c = holeArea.getCentre();
            const float r = holeArea.getHeight() * 1.1f;
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (logo, place);
            g.setGradientFill (juce::ColourGradient (light.withAlpha (0.28f * intensity), c.x, c.y,
                                                     light.withAlpha (0.0f), c.x + r, c.y, true));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        }

        drawEmblem (g, logoArea);
    }

private:
    void drawEmblem (juce::Graphics& g, juce::Rectangle<float> logoArea)
    {
        if (! Theme::kushKomaActive && ! Theme::canadaActive)
            return;

        const float h = logoArea.getHeight() * 0.62f;
        auto area = juce::Rectangle<float> (h, h).withCentre ({ logoArea.getRight() + h * 0.72f, logoArea.getCentreY() });

        juce::Path shape;
        if (Theme::kushKomaActive)
        {
            shape = makeLeafPath (h * 0.62f);
            shape.applyTransform (juce::AffineTransform::translation (area.getCentreX(), area.getBottom() - h * 0.12f));
        }

        juce::Graphics::ScopedSaveState state (g);
        if (Theme::canadaActive && mapleLeaf.isValid())
        {
            const auto fit = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                                 .getTransformToFit (mapleLeaf.getBounds().toFloat(), area);
            g.setColour (Theme::accent.withAlpha (0.25f));
            g.drawImageTransformed (mapleLeaf, fit.translated (0.0f, 1.5f), true);
            g.setColour (Theme::accent);
            g.drawImageTransformed (mapleLeaf, fit, true);
            return;
        }

        Theme::drawGlow (g, shape, Theme::accent, 5.0f, 0.8f);
        g.setGradientFill (juce::ColourGradient (Theme::accentBright, 0.0f, area.getY(), Theme::accent, 0.0f, area.getBottom(), false));
        g.fillPath (shape);
    }

    static juce::Rectangle<float> opaqueBounds (const juce::Image& image)
    {
        if (! image.isValid())
            return {};

        int x0 = image.getWidth(), y0 = image.getHeight(), x1 = -1, y1 = -1;
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (data.getPixelColour (x, y).getAlpha() > 64)
                {
                    x0 = juce::jmin (x0, x); x1 = juce::jmax (x1, x);
                    y0 = juce::jmin (y0, y); y1 = juce::jmax (y1, y);
                }

        return x1 < 0 ? juce::Rectangle<float>() : juce::Rectangle<float> ((float) x0, (float) y0, (float) (x1 - x0 + 1), (float) (y1 - y0 + 1));
    }

    juce::Image logo, keyhole, mapleLeaf;
    juce::Rectangle<float> keyholeBounds;
    Mode mode = Mode::Idle;
    float level = 0.0f, phase = 0.0f, flare = 0.0f;
    Eased glow, red;
};

} // namespace onykey::ui
