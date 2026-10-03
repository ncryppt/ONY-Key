#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Animation.h"
#include "DSP/KeyDetector.h"
#include "DSP/KeyNames.h"
#include <functional>

namespace onykey::ui
{

/** Circle of fifths in Camelot layout: major keys on the outer ring, their
    relative minors on the inner ring, so harmonically compatible keys sit
    next to each other.

    Built like a piece of hardware: a machined bezel with a dotted indicator
    track (the same idea as ONY Verb's knobs), the two key rings, and a glass
    centre disc showing the result. Every key is tinted by how well it
    matched; the detected key is lit and glows, the runner-up is outlined.
    Changes animate rather than jump. Hovering a key outlines the keys it
    mixes with; clicking the centre copies the result. */
class KeyWheel final : public juce::Component
{
public:
    enum class Activity { Idle, Analysing, Listening };

    std::function<void()> onCentreClicked;

    KeyWheel()
    {
        for (auto& h : heat) h.timeConstant = 0.18f;
        for (auto& l : lit) l.timeConstant = 0.10f;
        hoverAmount.timeConstant = 0.08f;
        level.timeConstant = 0.07f;
    }

    void setState (const KeyResult& r, Activity a, float analysisProgress, float inputLevel)
    {
        const bool changed = r.kind != result.kind || r.key != result.key;
        result = r;
        activity = a;
        progress = analysisProgress;
        level.set (activity == Activity::Listening ? juce::jlimit (0.0f, 1.0f, inputLevel) : 0.0f);

        float lo = 0.0f, hi = 1.0f;
        if (r.isValid())
        {
            lo = *std::min_element (r.keyScores.begin(), r.keyScores.end());
            hi = *std::max_element (r.keyScores.begin(), r.keyScores.end());
        }

        for (int k = 0; k < 24; ++k)
        {
            const float h = r.isValid() && hi > lo ? std::pow ((r.keyScores[(size_t) k] - lo) / (hi - lo), 3.0f) : 0.0f;
            heat[(size_t) k].set (h);
            lit[(size_t) k].set (r.isValid() && k == r.key ? 1.0f : 0.0f);
        }

        if (changed && r.isValid())
            reveal = 0.0f;
        else if (! r.isValid())
            reveal = 0.0f;
    }

    /** Hovered key (0-23) or -1. */
    int getHoveredKey() const noexcept { return hoveredKey; }

    /** Briefly shows "COPIED" in the centre. */
    void flashCopied() { copiedTimer = 1.4f; }

    /** Animation step; call from the editor's timer. */
    void tick (float dt)
    {
        bool moving = false;
        for (auto& h : heat) moving |= h.tick (dt);
        for (auto& l : lit) moving |= l.tick (dt);
        moving |= hoverAmount.tick (dt);
        moving |= level.tick (dt);

        if (reveal < 1.0f && result.isValid())
        {
            reveal = juce::jmin (1.0f, reveal + dt / 0.55f);
            moving = true;
        }

        if (copiedTimer > 0.0f)
        {
            copiedTimer = juce::jmax (0.0f, copiedTimer - dt);
            moving = true;
        }

        phase += dt;
        if (moving || activity != Activity::Idle)
            repaint();
        else if (! result.isValid())
            repaint (centreBounds()); // empty-state glyph bobs
    }

    // --- Mouse ---------------------------------------------------------------

    void mouseMove (const juce::MouseEvent& e) override { updateHover (e.position); }
    void mouseExit (const juce::MouseEvent&) override   { updateHover ({ -1000.0f, -1000.0f }); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (overCentre && result.isValid() && e.mouseWasClicked() && onCentreClicked)
            onCentreClicked();
    }

    // --- Painting ------------------------------------------------------------

    void paint (juce::Graphics& g) override
    {
        const auto geo = geometry();

        drawBezel (g, geo);

        // Ring bases.
        for (int i = 0; i < 12; ++i)
        {
            const int majorKey = (i * 7) % 12, minorKey = 12 + (majorKey + 9) % 12;
            drawSegmentBase (g, geo, majorKey);
            drawSegmentBase (g, geo, minorKey);
        }

        // Lit key(s) on top, so their glow blooms over the neighbours.
        for (int k = 0; k < 24; ++k)
            if (lit[(size_t) k].value > 0.002f)
                drawSegmentLit (g, geo, k);

        for (int k = 0; k < 24; ++k)
            drawSegmentLabel (g, geo, k);

        drawCentre (g, geo);
    }

private:
    struct Geometry
    {
        juce::Point<float> centre;
        float bezelOuter, bezelInner, majorOuter, majorInner, minorOuter, minorInner, disc;
    };

    Geometry geometry() const
    {
        const auto b = getLocalBounds().toFloat();
        // Margin so the cast shadow and the lit key's bloom aren't clipped.
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 30.0f;
        return { b.getCentre(), r, r * 0.925f, r * 0.905f, r * 0.69f, r * 0.675f, r * 0.475f, r * 0.45f };
    }

    juce::Rectangle<int> centreBounds() const
    {
        const auto geo = geometry();
        return juce::Rectangle<float> (geo.disc * 2.0f, geo.disc * 2.0f).withCentre (geo.centre).getSmallestIntegerContainer().expanded (2);
    }

    static float angleForIndex (float fifthsIndex) { return fifthsIndex * juce::MathConstants<float>::twoPi / 12.0f; }
    static int fifthsIndexOf (int key)
    {
        const int majorTonic = isMinorKey (key) ? (tonicOf (key) + 3) % 12 : tonicOf (key);
        return (majorTonic * 7) % 12;
    }

    juce::Path segmentPath (const Geometry& geo, int key, float inset = 0.0f) const
    {
        const float idx = (float) fifthsIndexOf (key);
        const float r0 = (isMinorKey (key) ? geo.minorInner : geo.majorInner) + inset;
        const float r1 = (isMinorKey (key) ? geo.minorOuter : geo.majorOuter) - inset;
        const float gapAngle = 0.012f + inset / r1;
        juce::Path p;
        p.addPieSegment (juce::Rectangle<float> (r1 * 2.0f, r1 * 2.0f).withCentre (geo.centre),
                         angleForIndex (idx - 0.5f) + gapAngle, angleForIndex (idx + 0.5f) - gapAngle, r0 / r1);
        return p;
    }

    void drawBezel (juce::Graphics& g, const Geometry& geo)
    {
        const auto c = geo.centre;
        auto outer = juce::Rectangle<float> (geo.bezelOuter * 2.0f, geo.bezelOuter * 2.0f).withCentre (c);

        // Cast shadow, then a dark metal ring lit from above.
        juce::Path disc;
        disc.addEllipse (outer);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f * Theme::shadowStrength), 22, { 0, 8 }).drawForPath (g, disc);

        g.setGradientFill (juce::ColourGradient (Theme::bezelTop, c.x, outer.getY(),
                                                 Theme::bezelBottom, c.x, outer.getBottom(), false));
        g.fillEllipse (outer);

        // Faint machining rings.
        for (int i = 1; i <= 4; ++i)
        {
            const float rr = geo.bezelInner + (geo.bezelOuter - geo.bezelInner) * (float) i / 5.0f;
            g.setColour (juce::Colours::white.withAlpha (0.018f));
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.6f);
        }

        // Rim highlights: light catches the top outer edge.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), c.x, outer.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), c.x, c.y, false));
        g.drawEllipse (outer.reduced (0.6f), 1.2f);

        // Well that the rings sit in.
        auto well = juce::Rectangle<float> (geo.bezelInner * 2.0f, geo.bezelInner * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (Theme::wellTop, c.x, well.getY(), Theme::wellBottom, c.x, well.getBottom(), false));
        g.fillEllipse (well);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f * Theme::shadowStrength), c.x, well.getY(),
                                                 juce::Colours::white.withAlpha (0.08f), c.x, well.getBottom(), false));
        g.drawEllipse (well, 1.2f);

        // Dotted indicator track around the bezel.
        constexpr int numDots = 72;
        const float dotR = (geo.bezelOuter + geo.bezelInner) * 0.5f;
        const float dotSize = juce::jmax (1.2f, (geo.bezelOuter - geo.bezelInner) * 0.16f);

        const bool valid = result.isValid();
        const float keyAngle = valid ? angleForIndex ((float) fifthsIndexOf (result.key)) : 0.0f;
        const float head = phase * 2.6f; // analysing comet position (radians)

        for (int i = 0; i < numDots; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) numDots;
            const auto p = c.getPointOnCircumference (dotR, a);
            float intensity = 0.0f;

            if (activity == Activity::Analysing)
            {
                const float behind = std::fmod (head - a + juce::MathConstants<float>::twoPi * 4.0f, juce::MathConstants<float>::twoPi);
                intensity = std::pow (juce::jmax (0.0f, 1.0f - behind / 2.2f), 2.0f);
            }
            else if (activity == Activity::Listening)
            {
                const float shimmer = 0.5f + 0.5f * std::sin (a * 3.0f + phase * 4.0f);
                intensity = level.value * (0.35f + 0.65f * shimmer);
            }
            else if (valid)
            {
                const float d = std::abs (std::remainder (a - keyAngle, juce::MathConstants<float>::twoPi));
                intensity = reveal > 0.0f ? juce::jmax (0.0f, 1.0f - d / 0.42f) * easeOutCubic (reveal) : 0.0f;
                intensity *= result.kind == KeyResult::Kind::RootNote ? 0.6f : 1.0f;
            }

            if (intensity > 0.02f)
            {
                g.setColour (Theme::accent.withAlpha (0.25f * intensity));
                g.fillEllipse (juce::Rectangle<float> (dotSize * 4.0f, dotSize * 4.0f).withCentre (p));
                g.setColour (Theme::accent.interpolatedWith (Theme::accentBright, intensity).withAlpha (0.35f + 0.65f * intensity));
                g.fillEllipse (juce::Rectangle<float> (dotSize * 2.0f, dotSize * 2.0f).withCentre (p));
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.6f * Theme::shadowStrength));
                g.fillEllipse (juce::Rectangle<float> (dotSize * 1.9f, dotSize * 1.9f).withCentre (p.translated (0.0f, 0.5f)));
                g.setColour (Theme::dotOff);
                g.fillEllipse (juce::Rectangle<float> (dotSize * 1.5f, dotSize * 1.5f).withCentre (p));
            }
        }
    }

    void drawSegmentBase (juce::Graphics& g, const Geometry& geo, int key)
    {
        const auto seg = segmentPath (geo, key);
        const bool minor = isMinorKey (key);
        const float r0 = minor ? geo.minorInner : geo.majorInner, r1 = minor ? geo.minorOuter : geo.majorOuter;

        // Radial gradient: darker towards the centre, so the rings read as a
        // shallow dish rather than flat tiles.
        const auto inner = minor ? Theme::segmentInnerMinor : Theme::segmentInner;
        juce::ColourGradient base (inner, geo.centre.x, geo.centre.y,
                                   minor ? Theme::segmentOuterMinor : Theme::segmentOuter, geo.centre.x + r1, geo.centre.y, true);
        base.addColour (r0 / r1, inner);
        g.setGradientFill (base);
        g.fillPath (seg);

        const float h = heat[(size_t) key].value;
        if (h > 0.003f)
        {
            g.setColour (Theme::accent.withAlpha (0.03f + 0.2f * h));
            g.fillPath (seg);
        }

        if (key == hoveredKey)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f * hoverAmount.value));
            g.fillPath (seg);
        }

        // Top edge catches light; bottom edge falls into shadow.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), geo.centre.x, geo.centre.y - r1,
                                                 juce::Colours::black.withAlpha (0.3f * Theme::shadowStrength), geo.centre.x, geo.centre.y + r1, false));
        g.strokePath (seg, juce::PathStrokeType (0.8f));

        const bool isAlt = result.kind == KeyResult::Kind::Key && key == result.altKey;
        const bool mixes = hoveredKey >= 0 && camelotCompatible (hoveredKey, key);
        if (isAlt || mixes)
        {
            const auto inset = segmentPath (geo, key, 2.5f);
            g.setColour ((isAlt ? Theme::accent : Theme::textSecondary).withAlpha (isAlt ? 0.75f : 0.55f * hoverAmount.value));
            g.strokePath (inset, juce::PathStrokeType (isAlt ? 1.3f : 1.0f));
        }
    }

    void drawSegmentLit (juce::Graphics& g, const Geometry& geo, int key)
    {
        const float s = lit[(size_t) key].value;
        const bool tentative = result.kind == KeyResult::Kind::RootNote;
        const float strength = s * (tentative ? 0.55f : 1.0f);
        const auto seg = segmentPath (geo, key);
        const bool minor = isMinorKey (key);
        const float r0 = minor ? geo.minorInner : geo.majorInner, r1 = minor ? geo.minorOuter : geo.majorOuter;

        // Bloom, flaring briefly as the result arrives.
        const float flare = 1.0f + 0.8f * (1.0f - easeOutCubic (reveal));
        Theme::drawGlow (g, seg, Theme::accent, 12.0f * flare, strength);

        juce::ColourGradient fill (Theme::accentDim, geo.centre.x, geo.centre.y,
                                   Theme::accentBright, geo.centre.x + r1, geo.centre.y, true);
        fill.addColour (r0 / r1, Theme::accentDim.brighter (0.2f));
        fill.addColour (0.5 * (r0 / r1) + 0.5, Theme::accent);
        g.setGradientFill (fill);
        g.setOpacity (strength);
        g.fillPath (seg);
        g.setOpacity (1.0f);

        g.setColour (Theme::accentBright.withAlpha (0.8f * strength));
        g.strokePath (seg, juce::PathStrokeType (1.0f));
    }

    void drawSegmentLabel (juce::Graphics& g, const Geometry& geo, int key)
    {
        const bool minor = isMinorKey (key);
        const float r0 = minor ? geo.minorInner : geo.majorInner, r1 = minor ? geo.minorOuter : geo.majorOuter;
        const float mid = angleForIndex ((float) fifthsIndexOf (key));
        const auto pos = geo.centre.getPointOnCircumference ((r0 + r1) * 0.5f, mid);
        const float depth = r1 - r0;
        const float nameSize = juce::jlimit (10.0f, 17.0f, depth * 0.31f);

        const float s = lit[(size_t) key].value * (result.kind == KeyResult::Kind::RootNote ? 0.55f : 1.0f);
        const float h = heat[(size_t) key].value;
        const auto idle = Theme::textSecondary.interpolatedWith (Theme::textPrimary, juce::jmin (1.0f, h * 1.6f + (key == hoveredKey ? hoverAmount.value * 0.5f : 0.0f)));
        const auto colour = idle.interpolatedWith (Theme::onAccent, s > 0.7f ? (s - 0.7f) / 0.3f : 0.0f);

        g.setColour (colour);
        g.setFont (Theme::font (nameSize, s > 0.5f ? Theme::Weight::Bold : Theme::Weight::DemiBold));
        g.drawText (shortKeyName (key), juce::Rectangle<float> (depth * 1.5f, nameSize + 2.0f).withCentre (pos.translated (0.0f, -nameSize * 0.3f)),
                    juce::Justification::centred, false);

        g.setColour (colour.withMultipliedAlpha (0.6f));
        g.setFont (Theme::font (juce::jmax (8.5f, nameSize * 0.66f), Theme::Weight::Medium));
        g.drawText (camelotCode (key), juce::Rectangle<float> (depth * 1.5f, nameSize).withCentre (pos.translated (0.0f, nameSize * 0.62f)),
                    juce::Justification::centred, false);
    }

    void drawCentre (juce::Graphics& g, const Geometry& geo)
    {
        const auto c = geo.centre;
        const float r = geo.disc;
        auto disc = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
        juce::Path discPath;
        discPath.addEllipse (disc);

        // Raised glass disc.
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f * Theme::shadowStrength), 16, { 0, 5 }).drawForPath (g, discPath);
        g.setGradientFill (juce::ColourGradient (Theme::discTop, c.x, disc.getY(),
                                                 Theme::discBottom, c.x, disc.getBottom(), false));
        g.fillPath (discPath);

        const float rv = easeOutCubic (reveal);
        const bool valid = result.isValid();

        // Accent light pooled behind the text once there's a result.
        if (valid)
        {
            juce::ColourGradient pool (Theme::accent.withAlpha (0.16f * rv), c.x, c.y,
                                       Theme::accent.withAlpha (0.0f), c.x + r, c.y, true);
            g.setGradientFill (pool);
            g.fillPath (discPath);
        }

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x, disc.getY(),
                                                 juce::Colours::black.withAlpha (0.5f * Theme::shadowStrength), c.x, disc.getBottom(), false));
        g.strokePath (discPath, juce::PathStrokeType (1.0f));

        // Status ring just inside the disc edge.
        const float ringR = r - 5.0f;
        if (activity == Activity::Analysing)
        {
            juce::Path track, arc;
            track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.strokePath (track, juce::PathStrokeType (2.5f));
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * juce::jmax (0.02f, progress), true);
            Theme::drawGlow (g, arc, Theme::accent, 6.0f);
            g.setColour (Theme::accent);
            g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (activity == Activity::Listening)
        {
            juce::Path ring;
            ring.addEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c));
            const float breathe = 0.5f + 0.5f * std::sin (phase * 3.0f);
            const float lv = level.value;
            Theme::drawGlow (g, ring, Theme::listenRed, 4.0f + 10.0f * lv, 0.5f + lv);
            g.setColour (Theme::listenRed.withAlpha (0.35f + 0.3f * breathe + 0.35f * lv));
            g.strokePath (ring, juce::PathStrokeType (1.5f + 1.5f * lv));
        }
        else if (valid)
        {
            juce::Path ring;
            ring.addEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c));
            g.setColour (Theme::accent.withAlpha (0.45f * rv));
            g.strokePath (ring, juce::PathStrokeType (1.0f));
        }

        auto text = disc.reduced (r * 0.16f);

        if (activity == Activity::Analysing && ! valid)
        {
            g.setColour (Theme::textSecondary);
            g.setFont (Theme::labelFont (juce::jmax (10.5f, r * 0.1f)));
            g.drawText ("ANALYSING", text.withTrimmedTop (r * 0.1f), juce::Justification::centredTop, false);
            g.setColour (Theme::textPrimary);
            g.setFont (Theme::font (r * 0.3f, Theme::Weight::Medium));
            g.drawText (juce::String (juce::roundToInt (progress * 100.0f)) + "%", text, juce::Justification::centred, false);
        }
        else if (! valid)
        {
            drawEmptyState (g, c, r);
        }
        else
        {
            drawResultText (g, c, r, rv);
        }

        // Click-to-copy hint / confirmation.
        if (valid && activity != Activity::Analysing)
        {
            const float hint = copiedTimer > 0.0f ? juce::jmin (1.0f, copiedTimer / 0.3f) : (overCentre ? hoverAmount.value : 0.0f);
            if (hint > 0.01f)
            {
                g.setColour ((copiedTimer > 0.0f ? Theme::accentBright : Theme::textSecondary).withAlpha (hint));
                g.setFont (Theme::labelFont (juce::jmax (9.0f, r * 0.09f)));
                g.drawText (copiedTimer > 0.0f ? "COPIED" : "CLICK TO COPY",
                            juce::Rectangle<float> (r * 1.4f, r * 0.14f).withCentre ({ c.x, c.y + r * 0.8f }),
                            juce::Justification::centred, false);
            }
        }

        Theme::drawGlassSheen (g, discPath, 0.05f);
    }

    void drawEmptyState (juce::Graphics& g, juce::Point<float> c, float r)
    {
        // "Drop into tray" glyph.
        const float s = r * 0.32f;
        const auto ic = c.translated (0.0f, -r * 0.2f);
        juce::Path arrow;
        arrow.startNewSubPath (ic.x, ic.y - s * 0.55f);
        arrow.lineTo (ic.x, ic.y + s * 0.15f);
        arrow.startNewSubPath (ic.x - s * 0.28f, ic.y - s * 0.12f);
        arrow.lineTo (ic.x, ic.y + s * 0.16f);
        arrow.lineTo (ic.x + s * 0.28f, ic.y - s * 0.12f);
        juce::Path tray;
        tray.startNewSubPath (ic.x - s * 0.55f, ic.y + s * 0.05f);
        tray.lineTo (ic.x - s * 0.55f, ic.y + s * 0.42f);
        tray.lineTo (ic.x + s * 0.55f, ic.y + s * 0.42f);
        tray.lineTo (ic.x + s * 0.55f, ic.y + s * 0.05f);

        // Gentle bob invites a drop; still while listening.
        const float bob = activity == Activity::Listening ? 0.0f : std::sin (phase * 2.2f) * r * 0.025f;
        g.setColour (activity == Activity::Listening ? Theme::listenRed.withAlpha (0.8f) : Theme::textDim);
        const juce::PathStrokeType stroke (r * 0.028f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.strokePath (arrow, stroke, juce::AffineTransform::translation (0.0f, bob));
        g.strokePath (tray, stroke);

        g.setColour (Theme::textSecondary);
        g.setFont (Theme::labelFont (juce::jmax (10.5f, r * 0.1f)));
        g.drawText (activity == Activity::Listening ? "LISTENING" : "DROP A SAMPLE",
                    juce::Rectangle<float> (r * 1.6f, r * 0.14f).withCentre ({ c.x, c.y + r * 0.3f }), juce::Justification::centred, false);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (juce::jmax (11.0f, r * 0.11f), Theme::Weight::Medium));
        g.drawText (activity == Activity::Listening ? "play audio through this track" : "or press LISTEN",
                    juce::Rectangle<float> (r * 1.6f, r * 0.16f).withCentre ({ c.x, c.y + r * 0.47f }), juce::Justification::centred, false);
    }

    void drawResultText (juce::Graphics& g, juce::Point<float> c, float r, float rv)
    {
        const bool rootOnly = result.kind == KeyResult::Kind::RootNote;
        const auto big = rootOnly ? noteName (result.rootNote) : tonicName (result.key);
        const auto small = rootOnly ? juce::String ("ROOT NOTE") : juce::String (isMinorKey (result.key) ? "MINOR" : "MAJOR");

        // Ease in: fade up and settle from slightly larger.
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::scale (1.0f + 0.06f * (1.0f - rv), 1.0f + 0.06f * (1.0f - rv), c.x, c.y));

        const auto bigArea = juce::Rectangle<float> (r * 1.7f, r * 0.72f).withCentre ({ c.x, c.y - r * 0.1f });
        const auto bigFont = Theme::font (r * 0.7f, Theme::Weight::DemiBold);
        // Soft halo behind the letters.
        g.setColour (Theme::accent.withAlpha (0.16f * rv));
        for (auto off : { juce::Point<float> (0.0f, 1.5f), juce::Point<float> (0.0f, -1.5f), juce::Point<float> (1.5f, 0.0f), juce::Point<float> (-1.5f, 0.0f) })
            Theme::drawKeyText (g, big, bigArea.translated (off.x, off.y), bigFont);
        g.setColour (Theme::textPrimary.withAlpha (rv));
        Theme::drawKeyText (g, big, bigArea, bigFont);

        g.setColour (Theme::accent.withAlpha (rv));
        g.setFont (Theme::labelFont (juce::jmax (10.5f, r * 0.12f)).withExtraKerningFactor (0.3f));
        g.drawText (small, juce::Rectangle<float> (r * 1.6f, r * 0.16f).withCentre ({ c.x, c.y + r * 0.3f }), juce::Justification::centred, false);

        if (! rootOnly)
            Theme::drawPill (g, juce::Rectangle<float> (r * 0.42f, r * 0.17f).withCentre ({ c.x, c.y + r * 0.53f }),
                             camelotCode (result.key), Theme::accentDim.withAlpha (0.7f * rv), Theme::textPrimary.withAlpha (rv), r * 0.095f);
    }

    void updateHover (juce::Point<float> pos)
    {
        const auto geo = geometry();
        const float dist = pos.getDistanceFrom (geo.centre);
        int key = -1;

        if (dist >= geo.minorInner && dist <= geo.majorOuter)
        {
            float angle = std::atan2 (pos.x - geo.centre.x, geo.centre.y - pos.y); // 0 = up, clockwise
            if (angle < 0.0f)
                angle += juce::MathConstants<float>::twoPi;
            const int idx = juce::roundToInt (angle / juce::MathConstants<float>::twoPi * 12.0f) % 12;
            const int majorTonic = (idx * 7) % 12;
            key = dist >= geo.majorInner ? majorTonic : 12 + (majorTonic + 9) % 12;
        }

        const bool centreNow = dist < geo.disc;
        if (key != hoveredKey || centreNow != overCentre)
        {
            hoveredKey = key;
            overCentre = centreNow;
            hoverAmount.snap (0.0f);
            hoverAmount.set (key >= 0 || overCentre ? 1.0f : 0.0f);
            setMouseCursor (overCentre && result.isValid() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    KeyResult result;
    Activity activity = Activity::Idle;
    float progress = 0.0f, phase = 0.0f, reveal = 1.0f, copiedTimer = 0.0f;
    std::array<Eased, 24> heat, lit;
    Eased hoverAmount, level;
    int hoveredKey = -1;
    bool overCentre = false;
};

} // namespace onykey::ui
