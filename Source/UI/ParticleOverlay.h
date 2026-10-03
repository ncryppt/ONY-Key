#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Shapes.h"
#include <array>
#include <cmath>
#include <functional>

namespace onykey::ui
{

/** ONY Verb's particle overlay, ported: a click-through layer over the whole
    panel that sends small glowing particles out from the wheel's centre
    disc (ONY Key's equivalent of Verb's orb).

      - Ambient trickle follows a "liveliness" source (the input level while
        LISTENing, a gentle trickle while analysing a file, nothing at rest).
      - spawnBurst() fires a burst from the disc: on transients while
        listening, and when a key is found.
      - spawnBurstAt() is a small pop for button clicks (wireClickBurst()).

    Themed variants, as in Verb:
      - Kush Koma: the dots become tumbling leaves, a curling wisp of smoke
        drifts up from the disc, and a room haze slowly builds the longer the
        theme stays on (clearing faster when you switch away).
      - Canada Eh?: tumbling beavers instead of dots.
      - Acid Trip: particles shift hue as they age, and a hypnotic spiral
        slowly rotates and breathes over the whole window.

    Unlike Verb's, this repaints only the area the particles cover unless the
    haze or spiral is showing, since every overlay repaint also repaints the
    UI underneath it. */
class ParticleOverlay final : public juce::Component, private juce::Timer
{
public:
    ParticleOverlay()
    {
        setInterceptsMouseClicks (false, false);
        leafTemplate = makeLeafPath (kTemplateSize);
        beaverTemplate = makeBeaverPath (kTemplateSize);
        smokeTemplate.addEllipse (-1.0f, -0.65f, 2.0f, 1.3f); // unit streak, stretched along its direction of travel
        startTimerHz (kFrameRate);
    }

    /** Where particles come from, in this overlay's coordinates. */
    void setOrbGeometry (juce::Point<float> centre, float radius)
    {
        orbCentre = centre;
        orbRadius = radius;
    }

    void setLivelinessSource (std::function<float()> source) { livelinessSource = std::move (source); }

    void spawnBurst (int count = 16)
    {
        for (int i = 0; i < count; ++i)
            spawnParticle (true, orbCentre, orbRadius);
    }

    void spawnBurstAt (juce::Point<float> origin)
    {
        for (int i = 0; i < 8; ++i)
            spawnParticle (true, origin, 4.0f);
    }

    void paint (juce::Graphics& g) override
    {
        const auto hue = Theme::accent.getHue();
        const auto sat = juce::jlimit (0.0f, 1.0f, Theme::accent.getSaturation());
        // Full brightness glows on dark grounds; on light ones that would be
        // a pale smudge, so keep the accent's own (darker) brightness there.
        const auto bri = Theme::currentThemeIsLight ? Theme::accent.getBrightness() : 1.0f;

        for (const auto& p : particles)
        {
            if (! p.active)
                continue;

            const auto t = juce::jlimit (0.0f, 1.0f, p.age / p.life);

            if (p.kind == Particle::Kind::Smoke)
            {
                const auto alpha = std::sin (t * juce::MathConstants<float>::pi) * p.baseAlpha;
                const auto radius = p.size * (0.6f + t * 1.5f);
                // Oriented along its wobbling velocity: a curling streak, not a dot.
                const auto angle = std::atan2 (p.velocity.y, p.velocity.x);
                g.setColour (Theme::textSecondary.withAlpha (alpha));
                g.fillPath (smokeTemplate, juce::AffineTransform::scale (radius).rotated (angle).translated (p.pos));
                continue;
            }

            const auto alpha = (1.0f - t) * p.baseAlpha;
            // Acid Trip: each particle keeps shifting hue as it ages.
            const auto particleHue = Theme::acidTripActive ? std::fmod (hue + p.hueOffset + t * 1.3f + 1.0f, 1.0f)
                                                           : std::fmod (hue + p.hueOffset + 1.0f, 1.0f);
            const auto colour = juce::Colour::fromHSV (particleHue, Theme::acidTripActive ? 1.0f : sat, bri, 1.0f);

            if (p.kind == Particle::Kind::Leaf || p.kind == Particle::Kind::Beaver)
            {
                const auto& shape = p.kind == Particle::Kind::Leaf ? leafTemplate : beaverTemplate;
                g.setColour ((p.kind == Particle::Kind::Leaf ? colour : Theme::accent).withAlpha (alpha));
                const auto scale = (p.size / kTemplateSize) * (1.0f - t * 0.25f);
                g.fillPath (shape, juce::AffineTransform::scale (scale).rotated (p.rotation).translated (p.pos));
                continue;
            }

            const auto radius = p.size * (1.0f - t * 0.35f);
            g.setColour (colour.withAlpha (alpha * 0.3f));
            g.fillEllipse (juce::Rectangle<float> (radius * 4.0f, radius * 4.0f).withCentre (p.pos));
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p.pos));
        }

        if (spiralLevel > 0.0f)
            paintSpiralOverlay (g);

        if (hotboxLevel > 0.0f)
            paintHotboxHaze (g);
    }

private:
    /** Acid Trip: alternating spiral arms sweeping out from the centre over
        a few turns, slowly rotating, with opacity breathing on a 10 s cycle.
        spiralLevel is the separate fade in/out when the theme changes. */
    void paintSpiralOverlay (juce::Graphics& g) const
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = spiralCentre();
        const auto rMax = juce::jmax (centre.getDistanceFrom (bounds.getTopLeft()), centre.getDistanceFrom (bounds.getBottomRight()),
                                      centre.getDistanceFrom (bounds.getTopRight()), centre.getDistanceFrom (bounds.getBottomLeft())) + 20.0f;
        constexpr float rMin = 6.0f;
        constexpr int totalSlots = 24, steps = 48;
        constexpr float totalTurns = 2.75f;

        const auto slotAngle = juce::MathConstants<float>::twoPi / (float) totalSlots;
        const auto halfWidth = slotAngle * 0.42f;

        const auto breathing = std::pow (0.5f + 0.5f * std::sin (spiralFadePhase), 1.5f);
        const auto alpha = spiralLevel * breathing * (Theme::currentThemeIsLight ? 0.12f : 0.22f);
        if (alpha < 0.004f)
            return;

        g.setColour (Theme::accent.withAlpha (alpha));

        for (int slot = 0; slot < totalSlots; slot += 2)
        {
            juce::Path arm;
            for (int step = 0; step <= steps; ++step)
            {
                const auto t = (float) step / (float) steps;
                const auto r = rMin + (rMax - rMin) * t;
                const auto a = (float) slot * slotAngle + totalTurns * juce::MathConstants<float>::twoPi * t + spiralPhase;
                const juce::Point<float> p (centre.x + r * std::cos (a - halfWidth), centre.y + r * std::sin (a - halfWidth));
                if (step == 0) arm.startNewSubPath (p); else arm.lineTo (p);
            }
            for (int step = steps; step >= 0; --step)
            {
                const auto t = (float) step / (float) steps;
                const auto r = rMin + (rMax - rMin) * t;
                const auto a = (float) slot * slotAngle + totalTurns * juce::MathConstants<float>::twoPi * t + spiralPhase;
                arm.lineTo (centre.x + r * std::cos (a + halfWidth), centre.y + r * std::sin (a + halfWidth));
            }
            arm.closeSubPath();
            g.fillPath (arm);
        }
    }

    /** Kush Koma's slowly-building room haze: a faint overall tint plus a few
        soft blobs drifting lazily (fixed seeds, animated phases), so it reads
        as uneven smoke hanging in the air rather than a screen filter. */
    void paintHotboxHaze (juce::Graphics& g) const
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto diag = bounds.getWidth() + bounds.getHeight();

        g.setColour (Theme::textSecondary.withAlpha (hotboxLevel * 0.05f));
        g.fillRect (bounds);

        juce::Random blobRng { 0x1105 };
        for (int i = 0; i < 5; ++i)
        {
            const auto seedX = blobRng.nextFloat(), seedY = blobRng.nextFloat();
            const auto freq = 0.05f + blobRng.nextFloat() * 0.05f;
            const auto phase = blobRng.nextFloat() * juce::MathConstants<float>::twoPi;

            const auto cx = bounds.getX() + bounds.getWidth()  * (seedX + 0.15f * std::sin (hotboxTime * freq + phase));
            const auto cy = bounds.getY() + bounds.getHeight() * (seedY + 0.15f * std::cos (hotboxTime * freq * 0.8f + phase));
            const auto radius = diag * (0.22f + 0.06f * std::sin (hotboxTime * freq * 1.3f + phase));

            g.setGradientFill (juce::ColourGradient (Theme::textSecondary.withAlpha (hotboxLevel * 0.10f), cx, cy,
                                                     Theme::textSecondary.withAlpha (0.0f), cx + radius, cy, true));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ cx, cy }));
        }
    }

    juce::Point<float> spiralCentre() const
    {
        return orbCentre.isOrigin() ? getLocalBounds().toFloat().getCentre() : orbCentre;
    }

    struct Particle
    {
        enum class Kind { Glow, Leaf, Smoke, Beaver };

        juce::Point<float> pos, velocity;
        float age = 0.0f, life = 1.0f, size = 2.0f, hueOffset = 0.0f, baseAlpha = 0.8f;
        float rotation = 0.0f, rotationSpin = 0.0f;
        float wobblePhase = 0.0f, wobbleFreq = 1.0f, wobbleAmp = 0.0f; // smoke's side-to-side curl
        Kind kind = Kind::Glow;
        bool active = false;
    };

    void spawnParticle (bool burst, juce::Point<float> origin, float radius)
    {
        for (auto& p : particles)
        {
            if (p.active)
                continue;

            const auto angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const juce::Point<float> direction (std::cos (angle), std::sin (angle));
            const auto speed = burst ? (150.0f + rng.nextFloat() * 190.0f) : (25.0f + rng.nextFloat() * 55.0f);

            p.pos = origin + direction * (radius * (0.55f + rng.nextFloat() * 0.5f));
            p.velocity = direction * speed;
            p.age = 0.0f;
            p.life = burst ? (1.0f + rng.nextFloat() * 0.7f) : (1.6f + rng.nextFloat() * 1.6f);
            p.size = burst ? (1.8f + rng.nextFloat() * 1.8f) : (1.1f + rng.nextFloat() * 1.3f);
            p.hueOffset = Theme::acidTripActive ? rng.nextFloat() : (rng.nextFloat() * 2.0f - 1.0f) * 0.05f;
            p.baseAlpha = burst ? 0.9f : 0.6f;
            p.rotation = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.rotationSpin = (rng.nextFloat() * 2.0f - 1.0f) * 5.0f;

            if (Theme::kushKomaActive)
            {
                p.kind = Particle::Kind::Leaf;
                p.size *= 3.5f; // big enough to read as a leaf
            }
            else if (Theme::canadaActive)
            {
                p.kind = Particle::Kind::Beaver;
                p.size *= 3.2f;
            }
            else
            {
                p.kind = Particle::Kind::Glow;
            }

            p.active = true;
            return;
        }
    }

    void spawnSmoke()
    {
        for (auto& p : particles)
        {
            if (p.active)
                continue;

            // A narrow, mostly-upward plume that curls side to side as it rises.
            const auto driftAngle = -juce::MathConstants<float>::halfPi + (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;
            const juce::Point<float> dir (std::cos (driftAngle), std::sin (driftAngle));

            p.pos = orbCentre + juce::Point<float> ((rng.nextFloat() * 2.0f - 1.0f) * orbRadius * 0.3f, 0.0f);
            p.velocity = dir * (14.0f + rng.nextFloat() * 14.0f);
            p.age = 0.0f;
            p.life = 3.8f + rng.nextFloat() * 3.0f;
            p.size = 4.0f + rng.nextFloat() * 5.0f;
            p.hueOffset = 0.0f;
            p.baseAlpha = 0.15f + rng.nextFloat() * 0.07f;
            p.rotation = p.rotationSpin = 0.0f;
            p.wobblePhase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.wobbleFreq = 0.9f + rng.nextFloat() * 1.4f;
            p.wobbleAmp = 9.0f + rng.nextFloat() * 12.0f;
            p.kind = Particle::Kind::Smoke;
            p.active = true;
            return;
        }
    }

    void timerCallback() override
    {
        constexpr float dt = 1.0f / (float) kFrameRate;
        const auto liveliness = livelinessSource ? juce::jlimit (0.0f, 1.0f, livelinessSource()) : 0.0f;

        // Ambient trickle follows the liveliness; silence emits nothing.
        ambientAccumulator += liveliness * 2.4f * dt;
        while (ambientAccumulator >= 1.0f)
        {
            spawnParticle (false, orbCentre, orbRadius);
            ambientAccumulator -= 1.0f;
        }

        // Kush Koma's smoke wafts regardless of loudness: decoration, not metering.
        if (Theme::kushKomaActive)
        {
            smokeAccumulator += 1.1f * dt;
            while (smokeAccumulator >= 1.0f)
            {
                spawnSmoke();
                smokeAccumulator -= 1.0f;
            }
        }

        // Haze builds slowly while Kush Koma is on, clears faster once it isn't.
        hotboxLevel = Theme::kushKomaActive ? juce::jmin (1.0f, hotboxLevel + dt / 55.0f)
                                            : juce::jmax (0.0f, hotboxLevel - dt / 8.0f);
        if (hotboxLevel > 0.0f)
            hotboxTime += dt;

        // Acid Trip's spiral fades in over 3 s, out over 1.5 s.
        spiralLevel = Theme::acidTripActive ? juce::jmin (1.0f, spiralLevel + dt / 3.0f)
                                            : juce::jmax (0.0f, spiralLevel - dt / 1.5f);
        if (spiralLevel > 0.0f)
        {
            spiralPhase += dt * 0.05f;
            spiralFadePhase = std::fmod (spiralFadePhase + dt * (juce::MathConstants<float>::twoPi / 10.0f), juce::MathConstants<float>::twoPi);
        }

        const auto cullBounds = getLocalBounds().toFloat().expanded (30.0f);
        juce::Rectangle<float> covered;

        for (auto& p : particles)
        {
            if (! p.active)
                continue;

            if (p.kind == Particle::Kind::Smoke)
                p.velocity.x = std::sin (p.age * p.wobbleFreq + p.wobblePhase) * p.wobbleAmp;

            p.pos += p.velocity * dt;
            p.velocity *= 0.99f;
            p.rotation += p.rotationSpin * dt;
            p.age += dt;

            if (p.age >= p.life || ! cullBounds.contains (p.pos))
            {
                p.active = false;
                continue;
            }

            // Generous extent: glows draw at 2x size, smoke grows to 2.1x and stretches.
            const auto extent = p.kind == Particle::Kind::Smoke ? p.size * 2.6f : p.size * 2.2f;
            const auto area = juce::Rectangle<float> (extent * 2.0f, extent * 2.0f).withCentre (p.pos);
            covered = covered.isEmpty() ? area : covered.getUnion (area);
        }

        if (hotboxLevel > 0.0f || spiralLevel > 0.0f)
        {
            repaint();
        }
        else
        {
            // This frame's particles plus last frame's (to erase them).
            const auto dirty = lastCovered.isEmpty() ? covered : (covered.isEmpty() ? lastCovered : covered.getUnion (lastCovered));
            if (! dirty.isEmpty())
                repaint (dirty.getSmallestIntegerContainer().expanded (2));
        }

        lastCovered = covered;
    }

    static constexpr int kFrameRate = 45;
    static constexpr int kMaxParticles = 240;
    static constexpr float kTemplateSize = 10.0f;

    std::array<Particle, kMaxParticles> particles;
    juce::Path leafTemplate, beaverTemplate, smokeTemplate;
    juce::Point<float> orbCentre;
    float orbRadius = 40.0f;
    float ambientAccumulator = 0.0f, smokeAccumulator = 0.0f;
    float hotboxLevel = 0.0f, hotboxTime = 0.0f;
    float spiralLevel = 0.0f, spiralPhase = 0.0f, spiralFadePhase = 0.0f;
    juce::Rectangle<float> lastCovered;
    juce::Random rng { 0x9a11e };
    std::function<float()> livelinessSource;
};

/** Adds a small particle pop at `button`'s position to its click, keeping
    whatever onClick it already has (wrapped, not replaced). */
inline void wireClickBurst (juce::Button& button, ParticleOverlay& overlay)
{
    auto previousOnClick = button.onClick;
    button.onClick = [&button, &overlay, previousOnClick]
    {
        overlay.spawnBurstAt (overlay.getLocalPoint (&button, button.getLocalBounds().getCentre().toFloat()));
        if (previousOnClick)
            previousOnClick();
    };
}

} // namespace onykey::ui
