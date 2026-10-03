#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace onykey::ui::Theme
{
// ---------------------------------------------------------------------------
// Palette. The same theme palettes as ONY Verb (same names and colours) so
// the plugins match side by side; ONY Key defaults to "Violet Dusk".
//
// These are `inline` (not `const`) so applyPalette() can overwrite them in
// place. Components read them fresh in every paint(), so switching themes is
// just applyPalette() + repaint (plus OnyvaLookAndFeel::refreshColours()
// for the few colours JUCE bakes in with setColour()).
// ---------------------------------------------------------------------------
inline juce::Colour background    { 0xff0a0910 };
inline juce::Colour panel         { 0xff141220 };
inline juce::Colour panelRaised   { 0xff1b1829 };
inline juce::Colour hairline      { 0xff2e2946 };
inline juce::Colour textPrimary   { 0xfff2f0f8 };
inline juce::Colour textSecondary { 0xff938bab };
inline juce::Colour textDim       { 0xff5a5470 };
inline juce::Colour accent        { 0xffb980ff };
inline juce::Colour accentDim     { 0xff6b4a96 };
inline juce::Colour accentGlow    { 0x80b980ff };

// Derived by applyPalette() (and by Acid Trip's hue cycle) from the above,
// so every theme also tints the hardware: bezel metal, LEDs, glass, logo.
inline juce::Colour backgroundTop, accentBright, onAccent;
inline juce::Colour bezelTop, bezelBottom, wellTop, wellBottom, dotOff;
inline juce::Colour segmentInner, segmentOuter, segmentInnerMinor, segmentOuterMinor;
inline juce::Colour discTop, discBottom, ledOff, ledNeutralLow, ledNeutralHigh;
inline juce::Colour metalTop, metalMid, metalBottom;
inline float shadowStrength = 1.0f; // light themes cast much softer shadows

inline const juce::Colour listenRed { 0xffff5a6e }; // "listening" indicator only

inline bool currentThemeIsLight = false;
inline bool kushKomaActive = false;  // leaf emblem beside the logo
inline bool acidTripActive = false;  // accent hue cycles continuously
inline bool canadaActive = false;    // maple-leaf emblem beside the logo

struct ThemePalette
{
    const char* name;
    juce::Colour background, panel, panelRaised, hairline;
    juce::Colour textPrimary, textSecondary, textDim;
    juce::Colour accent, accentDim, accentGlow;
    bool isLight = false;
};

// juce::Colour's uint32 constructor is explicit, which aggregate init can't
// use via a bare `{ 0xff... }`.
inline juce::Colour C (juce::uint32 argb) { return juce::Colour (argb); }

inline const std::array<ThemePalette, 18>& getThemePalettes()
{
    static const std::array<ThemePalette, 18> palettes { {
        { "Electric Blue",    C (0xff0a0a0c), C (0xff121214), C (0xff17171a), C (0xff26262b), C (0xfff2f2f4), C (0xff8a8a90), C (0xff55555a), C (0xff35d4ff), C (0xff1c7a94), C (0x8035d4ff) },
        { "Amber Ember",      C (0xff0c0a09), C (0xff15100d), C (0xff1c1512), C (0xff2e2620), C (0xfff5f0ea), C (0xff96897b), C (0xff5c534a), C (0xffff9d42), C (0xff945a26), C (0x80ff9d42) },
        { "Emerald Noir",     C (0xff090c0a), C (0xff10150f), C (0xff171e16), C (0xff283128), C (0xfff0f5f1), C (0xff89968b), C (0xff535e54), C (0xff2fe6a0), C (0xff1a8a5f), C (0x802fe6a0) },
        { "Crimson Velvet",   C (0xff0c0909), C (0xff160f0f), C (0xff1d1414), C (0xff342222), C (0xfff5efef), C (0xff988686), C (0xff5e4f4f), C (0xffff4d6d), C (0xff992e42), C (0x80ff4d6d) },
        { "Violet Dusk",      C (0xff0a0910), C (0xff141220), C (0xff1b1829), C (0xff2e2946), C (0xfff2f0f8), C (0xff938bab), C (0xff5a5470), C (0xffb980ff), C (0xff6b4a96), C (0x80b980ff) },
        { "Mono Slate",       C (0xff0a0a0b), C (0xff141415), C (0xff1a1a1c), C (0xff2c2c2f), C (0xfff4f4f5), C (0xff929296), C (0xff5b5b5f), C (0xffe8e8ec), C (0xff87878d), C (0x80e8e8ec) },
        { "Kush Koma",        C (0xff0a0c08), C (0xff10160c), C (0xff161f10), C (0xff283420), C (0xfff1f6ea), C (0xff9bab8c), C (0xff5f6f52), C (0xff7ed321), C (0xff4a7d14), C (0x807ed321) },
        { "Acid Trip",        C (0xff0a0710), C (0xff130b1c), C (0xff1a1026), C (0xff33184a), C (0xfff8f0ff), C (0xffc9a8e8), C (0xff7a5a94), C (0xffff2fd6), C (0xff9c1c94), C (0x80ff2fd6) },
        { "Canada Eh?",       C (0xff0c0909), C (0xff160f0f), C (0xff1d1414), C (0xff342222), C (0xfff5efef), C (0xff988686), C (0xff5e4f4f), C (0xffe8112d), C (0xff8a0a1c), C (0x80e8112d) },
        { "Daylight",         C (0xfff4f5f7), C (0xffe9eaed), C (0xffffffff), C (0xffd5d7db), C (0xff16171a), C (0xff5c5f66), C (0xff8b8e94), C (0xff0a7cff), C (0xff0857b8), C (0x800a7cff), true },
        { "Ivory",            C (0xfff7f2e9), C (0xffefe7d8), C (0xfffffcf5), C (0xffddd0b8), C (0xff2a2116), C (0xff6e6045), C (0xffa0937a), C (0xffc1440e), C (0xff7a2c08), C (0x80c1440e), true },
        { "Mint Fog",         C (0xfff0f7f3), C (0xffe3f0e9), C (0xffffffff), C (0xffcde3d7), C (0xff12241c), C (0xff4f6b5c), C (0xff82998c), C (0xff0e9e6c), C (0xff076b48), C (0x800e9e6c), true },
        { "Rose Quartz",      C (0xfffbf1f3), C (0xfff5e3e7), C (0xfffffbfc), C (0xffe8cdd3), C (0xff2a1418), C (0xff6e4750), C (0xffa17e86), C (0xffd6336c), C (0xff99204c), C (0x80d6336c), true },
        { "Lilac Mist",       C (0xfff5f2fb), C (0xffece5f6), C (0xfffdfbff), C (0xffdcd0ee), C (0xff1e1830), C (0xff5c4f78), C (0xff8d80a8), C (0xff7c4dff), C (0xff5232a8), C (0x807c4dff), true },
        { "Graphite Light",   C (0xfff2f3f4), C (0xffe5e7e9), C (0xffffffff), C (0xffd2d5d8), C (0xff14171a), C (0xff52585e), C (0xff868c92), C (0xff4a5058), C (0xff2e3237), C (0x804a5058), true },
        { "Kush Koma Light",  C (0xfff4f8ea), C (0xffe7f0d8), C (0xfffdfff8), C (0xffd3e2bb), C (0xff1b2410), C (0xff576a41), C (0xff8b9c74), C (0xff5a9c1c), C (0xff3c6e11), C (0x805a9c1c), true },
        { "Acid Trip Light",  C (0xfff6f0fa), C (0xffece0f5), C (0xfffffbff), C (0xffe0cdf0), C (0xff20112f), C (0xff6e5490), C (0xffa08cc0), C (0xffff2fd6), C (0xff9c1c94), C (0x80ff2fd6), true },
        { "Canada Eh? Light", C (0xfffaf7f7), C (0xfff0e6e6), C (0xffffffff), C (0xffe0cccc), C (0xff241414), C (0xff6e4747), C (0xffa17e7e), C (0xffe8112d), C (0xff8a0a1c), C (0x80e8112d), true },
    } };
    return palettes;
}

constexpr int defaultThemeIndex = 4; // Violet Dusk

/** Kush Koma and Acid Trip (dark and light): hidden by "Hide NSFW themes". */
inline bool isNsfwTheme (const ThemePalette& p)
{
    const juce::String name (p.name);
    return name.startsWith ("Kush Koma") || name.startsWith ("Acid Trip");
}

/** Recomputes the accent-derived colours; Acid Trip calls this every frame
    as it cycles the accent hue. */
inline void deriveAccentColours()
{
    accentBright = accent.interpolatedWith (juce::Colours::white, 0.45f);
    onAccent = accent.getPerceivedBrightness() > 0.62f ? juce::Colour (0xff111114) : juce::Colours::white;
}

inline void applyPalette (const ThemePalette& p)
{
    background = p.background; panel = p.panel; panelRaised = p.panelRaised; hairline = p.hairline;
    textPrimary = p.textPrimary; textSecondary = p.textSecondary; textDim = p.textDim;
    accent = p.accent; accentDim = p.accentDim; accentGlow = p.accentGlow;

    const juce::String name (p.name);
    currentThemeIsLight = p.isLight;
    kushKomaActive = name.startsWith ("Kush Koma");
    acidTripActive = name.startsWith ("Acid Trip");
    canadaActive = name.startsWith ("Canada");

    deriveAccentColours();

    if (p.isLight)
    {
        // Light hardware: pale anodised metal, white glass, graphite logo.
        shadowStrength = 0.3f;
        backgroundTop = background.brighter (0.03f);
        bezelTop = juce::Colours::white;               bezelBottom = hairline.darker (0.12f);
        wellTop = background.darker (0.14f);           wellBottom = background.darker (0.04f);
        dotOff = hairline.darker (0.18f);
        segmentOuter = panelRaised;                    segmentInner = panel.darker (0.02f);
        segmentOuterMinor = panel.brighter (0.02f);    segmentInnerMinor = panel.darker (0.05f);
        discTop = juce::Colours::white;                discBottom = panel.darker (0.05f);
        ledOff = panel.darker (0.07f);
        ledNeutralLow = hairline.darker (0.15f);       ledNeutralHigh = textDim;
        metalTop = textPrimary.brighter (0.5f);        metalMid = textPrimary.brighter (0.12f);
        metalBottom = textPrimary;
    }
    else
    {
        // Dark hardware, every tone taken from the palette so tinted themes
        // (Amber, Emerald...) tint the metal and glass too.
        shadowStrength = 1.0f;
        backgroundTop = background.interpolatedWith (panelRaised, 0.6f);
        bezelTop = panelRaised.brighter (0.55f);       bezelBottom = background.darker (0.3f);
        wellTop = background.darker (0.6f);            wellBottom = background.brighter (0.05f);
        dotOff = hairline.brighter (0.2f);
        segmentOuter = panelRaised.brighter (0.25f);   segmentInner = panel.brighter (0.08f);
        segmentOuterMinor = panelRaised.brighter (0.12f); segmentInnerMinor = panel;
        discTop = panelRaised.brighter (0.2f);         discBottom = background.darker (0.3f);
        ledOff = panel.brighter (0.06f);
        ledNeutralLow = hairline.brighter (0.25f);     ledNeutralHigh = textDim.brighter (0.25f);
        metalTop = juce::Colours::white;               metalMid = textPrimary.darker (0.12f);
        metalBottom = textSecondary.brighter (0.1f);
    }
}

// Derived colours are valid from the start, before any editor applies the
// user's saved theme.
inline const bool defaultPaletteApplied = (applyPalette (getThemePalettes()[(size_t) defaultThemeIndex]), true);

inline int findThemeIndex (const juce::String& name)
{
    const auto& palettes = getThemePalettes();
    for (size_t i = 0; i < palettes.size(); ++i)
        if (name == palettes[i].name)
            return (int) i;
    return -1;
}

// ---------------------------------------------------------------------------
// Persistence: the chosen theme is a per-user preference (like ONY Verb),
// not saved per session. Stored by name, so reordering palettes is safe.
// ---------------------------------------------------------------------------
inline juce::File preferencesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("ONYVA").getChildFile ("ONY Key");
}

/** Lets tools (the snapshot renderer) force a theme without touching the
    user's saved preference. */
inline juce::String& sessionThemeOverride() { static juce::String name; return name; }

inline int loadSavedThemeIndex()
{
    if (sessionThemeOverride().isNotEmpty())
        return juce::jmax (0, findThemeIndex (sessionThemeOverride()));

    const auto index = findThemeIndex (preferencesFolder().getChildFile ("theme.txt").loadFileAsString().trim());
    return index >= 0 ? index : defaultThemeIndex;
}

inline void saveThemeIndex (int index)
{
    if (sessionThemeOverride().isNotEmpty() || ! juce::isPositiveAndBelow (index, (int) getThemePalettes().size()))
        return;
    preferencesFolder().createDirectory();
    preferencesFolder().getChildFile ("theme.txt").replaceWithText (getThemePalettes()[(size_t) index].name);
}

inline bool loadHideNsfw()
{
    return preferencesFolder().getChildFile ("hidensfw.txt").loadFileAsString().trim() == "1";
}

inline void saveHideNsfw (bool hide)
{
    if (sessionThemeOverride().isNotEmpty())
        return;
    preferencesFolder().createDirectory();
    preferencesFolder().getChildFile ("hidensfw.txt").replaceWithText (hide ? "1" : "0");
}

/** The first non-NSFW theme of the same light/dark kind as `index`. */
inline int firstSafeThemeIndex (int index)
{
    const auto& palettes = getThemePalettes();
    if (! juce::isPositiveAndBelow (index, (int) palettes.size()) || ! isNsfwTheme (palettes[(size_t) index]))
        return index;
    for (size_t i = 0; i < palettes.size(); ++i)
        if (palettes[i].isLight == palettes[(size_t) index].isLight && ! isNsfwTheme (palettes[i]))
            return (int) i;
    return defaultThemeIndex;
}

// ---------------------------------------------------------------------------
// Type. "Avenir Next" (ships with macOS) like ONY Verb; elsewhere the
// platform sans with JUCE's bold flag, since the named weights won't exist.
// ---------------------------------------------------------------------------
enum class Weight { Regular, Medium, DemiBold, Bold };

inline bool hasAvenir()
{
    static const bool available = juce::Font::findAllTypefaceNames().contains ("Avenir Next");
    return available;
}

inline juce::Font font (float height, Weight weight = Weight::Regular)
{
    if (hasAvenir())
    {
        static const char* const styles[] = { "Regular", "Medium", "Demi Bold", "Bold" };
        return juce::Font (juce::FontOptions().withName ("Avenir Next").withStyle (styles[(int) weight]).withHeight (height));
    }

    return juce::Font (juce::FontOptions (height, weight >= Weight::DemiBold ? juce::Font::bold : juce::Font::plain));
}

/** Small spaced-out caps used for section labels ("CONFIDENCE"). */
inline juce::Font labelFont (float height = 10.5f)
{
    return font (height, Weight::DemiBold).withExtraKerningFactor (0.14f);
}

// ---------------------------------------------------------------------------
// Depth cues, shared so every surface gets the same light: from above, with
// shadow pooling below. (Same recipe as ONY Verb's Theme helpers.)
// ---------------------------------------------------------------------------

/** Soft shadow cast by a raised rounded shape onto the surface behind it. */
inline void dropShadow (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float alpha = 0.45f, int blur = 10, int offsetY = 3)
{
    juce::Path p;
    p.addRoundedRectangle (bounds, radius);
    juce::DropShadow (juce::Colours::black.withAlpha (alpha * shadowStrength), blur, { 0, offsetY }).drawForPath (g, p);
}

/** Raised panel: top-lit gradient surface, and an edge that catches light on
    top and falls into shadow at the bottom. */
inline void fillRaised (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, juce::Colour base = panelRaised)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.08f), bounds.getX(), bounds.getY(),
                                             base.darker (0.12f), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, radius);

    juce::Path outline;
    outline.addRoundedRectangle (bounds.reduced (0.5f), radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (currentThemeIsLight ? 0.9f : 0.16f), bounds.getX(), bounds.getY(),
                                             juce::Colours::black.withAlpha (currentThemeIsLight ? 0.14f : 0.35f), bounds.getX(), bounds.getBottom(), false));
    g.strokePath (outline, juce::PathStrokeType (1.0f));
}

/** Recessed well: darker, with shadow falling in from the top edge. */
inline void fillRecessed (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setGradientFill (juce::ColourGradient (background.darker (currentThemeIsLight ? 0.07f : 0.35f), bounds.getX(), bounds.getY(),
                                             background.darker (currentThemeIsLight ? 0.02f : 0.05f), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, radius);

    {
        juce::Path clip;
        clip.addRoundedRectangle (bounds, radius);
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (clip);

        juce::Path caster;
        caster.addRectangle (bounds.getX() - 10.0f, bounds.getY() - 14.0f, bounds.getWidth() + 20.0f, 14.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f * shadowStrength), 12, { 0, 5 }).drawForPath (g, caster);
    }

    juce::Path outline;
    outline.addRoundedRectangle (bounds.reduced (0.5f), radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (currentThemeIsLight ? 0.12f : 0.4f), bounds.getX(), bounds.getY(),
                                             juce::Colours::white.withAlpha (currentThemeIsLight ? 0.8f : 0.07f), bounds.getX(), bounds.getBottom(), false));
    g.strokePath (outline, juce::PathStrokeType (1.0f));
}

/** Diagonal gloss streak, as if a pane of glass sits over the shape. Draw last. */
inline void drawGlassSheen (juce::Graphics& g, const juce::Path& shape, float strength = 0.07f)
{
    const auto bounds = shape.getBounds();
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (shape);

    const auto angle = juce::degreesToRadians (-24.0f);
    const juce::Point<float> dir (std::cos (angle), std::sin (angle)), perp (-dir.y, dir.x);
    const auto centre = bounds.getCentre().translated (0.0f, -bounds.getHeight() * 0.22f);
    const float halfLen = bounds.getWidth() + bounds.getHeight(), halfWidth = bounds.getHeight() * 0.3f;

    juce::Path band;
    band.startNewSubPath (centre - dir * halfLen - perp * halfWidth);
    band.lineTo (centre + dir * halfLen - perp * halfWidth);
    band.lineTo (centre + dir * halfLen + perp * halfWidth);
    band.lineTo (centre - dir * halfLen + perp * halfWidth);
    band.closeSubPath();

    const auto e1 = centre - perp * halfWidth, e2 = centre + perp * halfWidth;
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), e1.x, e1.y, juce::Colours::white.withAlpha (0.0f), e2.x, e2.y, false);
    sheen.addColour (0.5, juce::Colours::white.withAlpha (strength));
    g.setGradientFill (sheen);
    g.fillPath (band);
}

/** Cheap bloom: the path stroked outward a few times at falling alpha.
    (A real blur per frame would be too costly for something that animates.) */
inline void drawGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float size = 10.0f, float strength = 1.0f)
{
    constexpr int layers = 5;
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        g.setColour (colour.withMultipliedAlpha (strength * 0.10f * (1.0f - t * 0.6f)));
        g.strokePath (path, juce::PathStrokeType (size * t * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

/** Sparse salt-and-pepper speckle, tiled over large surfaces so they read as
    a matte material rather than flat digital black. */
inline const juce::Image& grainTile()
{
    static const juce::Image tile = []
    {
        constexpr int size = 128;
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Random rng (7);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (rng.nextFloat() < 0.35f)
                    image.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black)
                                                .withAlpha (0.015f + rng.nextFloat() * 0.035f));
        return image;
    }();
    return tile;
}

inline void fillGrain (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setTiledImageFill (grainTile(), 0, 0, 1.0f);
    g.fillRect (area);
}

/** Small rounded "chip" with text, e.g. the Camelot code. */
inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour fill, juce::Colour textColour, float fontHeight)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour ((currentThemeIsLight ? juce::Colours::black : juce::Colours::white).withAlpha (0.1f));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    g.setColour (textColour);
    g.setFont (font (fontHeight, Weight::DemiBold).withExtraKerningFactor (0.06f));
    g.drawText (text, r, juce::Justification::centred, false);
}

/** Draws a key/note name written in ASCII ("Ab", "F#m", "Bbm") with real
    flat/sharp signs. They're drawn as vector shapes sized to the font's
    caps, because Avenir Next has no ♭/♯ glyphs and the fallback font's
    look completely out of place next to it. */
inline void drawKeyText (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area,
                         const juce::Font& f, juce::Justification justification = juce::Justification::centred)
{
    const bool hasAccidental = name.length() >= 2 && (name[1] == '#' || name[1] == 'b');
    const auto letter = name.substring (0, 1);
    const auto suffix = name.substring (hasAccidental ? 2 : 1);
    const juce::juce_wchar accidental = hasAccidental ? name[1] : 0;

    const float capH = f.getAscent() * 0.74f;
    const float letterW = juce::GlyphArrangement::getStringWidth (f, letter);
    const float accW = accidental == '#' ? capH * 0.52f : accidental == 'b' ? capH * 0.42f : 0.0f;
    const float accGap = hasAccidental ? capH * 0.08f : 0.0f;
    const float suffixW = juce::GlyphArrangement::getStringWidth (f, suffix);
    const float totalW = letterW + accGap + accW + accGap + suffixW;

    float x = area.getX();
    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - totalW * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = area.getRight() - totalW;

    const float baseline = area.getCentreY() + capH * 0.5f;

    g.setFont (f);
    g.drawSingleLineText (letter, juce::roundToInt (x), juce::roundToInt (baseline));
    x += letterW + accGap;

    if (accidental != 0)
    {
        const float stroke = juce::jmax (1.0f, capH * 0.085f);
        juce::Path p;

        if (accidental == 'b')
        {
            // Stem rising above the cap height, and a teardrop bowl at its foot.
            const float sx = x + stroke * 0.5f, top = baseline - capH * 1.02f;
            p.startNewSubPath (sx, top);
            p.lineTo (sx, baseline);
            p.startNewSubPath (sx, baseline - capH * 0.38f);
            p.cubicTo (sx + accW * 0.75f, baseline - capH * 0.62f,
                       sx + accW * 1.05f, baseline - capH * 0.22f,
                       sx, baseline);
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            // Two upright strokes, two heavier rising bars.
            const float top = baseline - capH * 1.0f, bottom = baseline + capH * 0.08f;
            p.startNewSubPath (x + accW * 0.33f, top + capH * 0.06f);
            p.lineTo (x + accW * 0.33f, bottom);
            p.startNewSubPath (x + accW * 0.67f, top);
            p.lineTo (x + accW * 0.67f, bottom - capH * 0.06f);
            g.strokePath (p, juce::PathStrokeType (stroke * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path bars;
            for (float yy : { baseline - capH * 0.66f, baseline - capH * 0.3f })
            {
                bars.startNewSubPath (x, yy + capH * 0.07f);
                bars.lineTo (x + accW, yy - capH * 0.07f);
            }
            g.strokePath (bars, juce::PathStrokeType (stroke * 1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        x += accW + accGap;
    }

    if (suffix.isNotEmpty())
        g.drawSingleLineText (suffix, juce::roundToInt (x), juce::roundToInt (baseline));
}

} // namespace onykey::ui::Theme
