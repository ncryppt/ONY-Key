#include "PluginEditor.h"
#include "FileAnalyser.h"
#include "Version.h"

using namespace onykey;
namespace Theme = onykey::ui::Theme;

namespace
{
// Fixed design layout (the editor scales the whole panel).
const juce::Rectangle<int> headerBounds  { 20, 8, 438, 64 };
const juce::Rectangle<int> taglineRow    { 236, 14, 222, 22 };  // tagline + version chip
const juce::Rectangle<int> themeRow      { 298, 42, 160, 26 };  // theme picker
const juce::Rectangle<int> wheelBounds   { 16, 64, 448, 448 };   // includes a 30 px shadow/bloom margin
const juce::Rectangle<int> statsBounds   { 20, 494, 440, 100 };
const juce::Rectangle<int> chromaBounds  { 22, 604, 436, 120 };
const juce::Rectangle<int> buttonsBounds { 19, 738, 442, 46 };
const juce::Rectangle<int> statusBounds  { 22, 788, 436, 20 };

juce::String middleDot() { return juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")); }
} // namespace

// =============================================================================
ONYKeyMainPanel::ONYKeyMainPanel (ONYKeyAudioProcessor& p) : keyProcessor (p)
{
    setOpaque (true);

    for (auto* c : std::initializer_list<juce::Component*> { &wordmark, &wheel, &stats, &chromaBars, &openButton, &listenButton, &clearButton, &themePicker, &particles })
        addAndMakeVisible (c);

    // Theme: a per-user preference shared by every ONY Key instance, like ONY Verb.
    hideNsfw = Theme::loadHideNsfw();
    themePicker.onThemeChosen = [this] (int index) { applyTheme (index, true); };
    themePicker.onHideNsfwChanged = [this] (bool hide) { setHideNsfw (hide); };
    const int saved = Theme::loadSavedThemeIndex();
    applyTheme (hideNsfw ? Theme::firstSafeThemeIndex (saved) : saved, false);

    openButton.setTooltip ("Analyse an audio file (or just drag one onto the window)");
    listenButton.setTooltip ("Analyse whatever plays through this track. Press LISTEN, then play.");
    clearButton.setTooltip ("Clear the result");

    openButton.onClick = [this] { openFileChooser(); };
    listenButton.onClick = [this] { keyProcessor.setListening (! display.listening); refresh(); };
    clearButton.onClick = [this] { keyProcessor.clearResult(); refresh(); };
    wheel.onCentreClicked = [this] { copyResult(); };

    // ONY Verb's particles, flowing from the wheel's centre disc.
    particles.setLivelinessSource ([this]
    {
        return display.listening ? listenLevel() * 0.9f : (display.analysing ? 0.35f : 0.0f);
    });
    for (auto* b : std::initializer_list<juce::Button*> { &openButton, &listenButton, &clearButton, &themePicker })
        onykey::ui::wireClickBurst (*b, particles);

    ambience.timeConstant = 0.5f;
    dropOverlay.timeConstant = 0.08f;

    setSize (designWidth, designHeight);
    refresh();
    ambience.snap (ambience.target);
    burstsArmed = true;
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

ONYKeyMainPanel::~ONYKeyMainPanel() = default;

void ONYKeyMainPanel::resized()
{
    wordmark.setBounds (headerBounds.withWidth (214)); // logo + room for a theme emblem
    themePicker.setBounds (themeRow);
    wheel.setBounds (wheelBounds);
    stats.setBounds (statsBounds);
    chromaBars.setBounds (chromaBounds);

    auto buttons = buttonsBounds;
    const int gap = 6, w = (buttons.getWidth() - 2 * gap) / 3;
    openButton.setBounds (buttons.removeFromLeft (w));
    buttons.removeFromLeft (gap);
    listenButton.setBounds (buttons.removeFromLeft (w));
    buttons.removeFromLeft (gap);
    clearButton.setBounds (buttons);

    statusArea = statusBounds;

    particles.setBounds (getLocalBounds());
    // Wheel disc radius: (wheel size / 2 - 30 px margin) * 0.45, see KeyWheel::geometry().
    const float discRadius = ((float) wheelBounds.getWidth() * 0.5f - 30.0f) * 0.45f;
    particles.setOrbGeometry (wheelBounds.toFloat().getCentre(), discRadius * 0.9f);
}

// --- Painting -------------------------------------------------------------------

void ONYKeyMainPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Ground: top-lit, with a soft accent light pooled behind the wheel and
    // the edges falling away, then grain so it reads as a material.
    g.setGradientFill (juce::ColourGradient (Theme::backgroundTop, 0.0f, 0.0f, Theme::background, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    if (Theme::acidTripActive)
    {
        // Slow two-colour wash that breathes with the hue cycle (as in ONY Verb).
        const auto washA = juce::Colour::fromHSV (acidPhase, 0.85f, 1.0f, Theme::currentThemeIsLight ? 0.06f : 0.07f);
        const auto washB = juce::Colour::fromHSV (std::fmod (acidPhase + 0.5f, 1.0f), 0.85f, 1.0f, 0.06f);
        g.setGradientFill (juce::ColourGradient (washA, 0.0f, 0.0f, washB, bounds.getRight(), bounds.getBottom(), false));
        g.fillRect (bounds);
    }

    const auto wc = wheelBounds.toFloat().getCentre();
    const float glow = 0.05f + 0.09f * ambience.value;
    g.setGradientFill (juce::ColourGradient (Theme::accent.withAlpha (glow), wc.x, wc.y,
                                             Theme::accent.withAlpha (0.0f), wc.x + 330.0f, wc.y, true));
    g.fillRect (bounds);

    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                             juce::Colours::black.withAlpha (Theme::currentThemeIsLight ? 0.05f : 0.45f), 0.0f, 0.0f, true));
    g.fillRect (bounds);

    Theme::fillGrain (g, getLocalBounds());

    // Header: product line + version chip (theme picker sits below them),
    // fading hairline underneath.
    auto header = taglineRow.toFloat();
    auto chip = header.removeFromRight (52.0f).withSizeKeepingCentre (52.0f, 20.0f);
    Theme::drawPill (g, chip, pluginVersion, Theme::panelRaised, Theme::textSecondary, 10.0f);
    header.removeFromRight (10.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::labelFont (10.0f));
    g.drawText ("KEY DETECTION SOFTWARE", header, juce::Justification::centredRight, false);

    const float lineY = (float) headerBounds.getBottom() + 5.0f;
    juce::ColourGradient line (Theme::hairline.withAlpha (0.0f), (float) headerBounds.getX(), lineY,
                               Theme::hairline.withAlpha (0.0f), (float) headerBounds.getRight(), lineY, false);
    line.addColour (0.5, Theme::hairline.brighter (0.4f));
    g.setGradientFill (line);
    g.fillRect (juce::Rectangle<float> ((float) headerBounds.getX(), lineY, (float) headerBounds.getWidth(), 1.0f));

    // Status line with an indicator dot.
    const auto text = statusText();
    const auto font = Theme::font (12.0f, Theme::Weight::Medium);
    const float textW = juce::GlyphArrangement::getStringWidth (font, text);
    const float groupW = juce::jmin ((float) statusArea.getWidth(), textW + 16.0f);
    auto row = statusArea.toFloat().withSizeKeepingCentre (groupW, (float) statusArea.getHeight());

    const auto dotColour = statusColour();
    const auto dotCentre = juce::Point<float> (row.getX() + 3.0f, row.getCentreY());
    g.setColour (dotColour.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (dotCentre));
    g.setColour (dotColour);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dotCentre));

    g.setColour (display.failed ? Theme::listenRed : Theme::textSecondary);
    g.setFont (font);
    g.drawFittedText (text, row.withTrimmedLeft (16.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

void ONYKeyMainPanel::paintOverChildren (juce::Graphics& g)
{
    const float a = dropOverlay.value;
    if (a <= 0.005f)
        return;

    const auto bounds = getLocalBounds().toFloat();

    // Frosted glass: a heavily downscaled snapshot drawn back up is a cheap,
    // convincing blur.
    if (frostedSnapshot.isValid())
    {
        g.setOpacity (a);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (frostedSnapshot, bounds);
        g.setOpacity (1.0f);
    }

    g.setColour (Theme::background.withAlpha (0.55f * a));
    g.fillRect (bounds);

    // Marching-ants outline.
    auto card = bounds.reduced (26.0f);
    juce::Path outline;
    outline.addRoundedRectangle (card, 20.0f);
    juce::Path dashed;
    const float offset = std::fmod (dashPhase * 30.0f, 16.0f);
    const float pattern[] = { offset + 0.001f, 7.0f, 9.0f, 0.001f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, outline, pattern, 4);
    Theme::drawGlow (g, outline, Theme::accent, 8.0f, a);
    g.setColour (Theme::accent.withAlpha (a));
    g.fillPath (dashed);

    // Big "drop" glyph and the file's name.
    const auto c = card.getCentre().translated (0.0f, -30.0f);
    const float bob = std::sin (dashPhase * 3.0f) * 4.0f;
    juce::Path glyph;
    glyph.startNewSubPath (c.x, c.y - 34.0f + bob);
    glyph.lineTo (c.x, c.y + 8.0f + bob);
    glyph.startNewSubPath (c.x - 16.0f, c.y - 8.0f + bob);
    glyph.lineTo (c.x, c.y + 9.0f + bob);
    glyph.lineTo (c.x + 16.0f, c.y - 8.0f + bob);
    glyph.startNewSubPath (c.x - 30.0f, c.y + 2.0f);
    glyph.lineTo (c.x - 30.0f, c.y + 24.0f);
    glyph.lineTo (c.x + 30.0f, c.y + 24.0f);
    glyph.lineTo (c.x + 30.0f, c.y + 2.0f);
    juce::Path stroked;
    juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroked, glyph);
    Theme::drawGlow (g, stroked, Theme::accent, 8.0f, a);
    g.setColour (Theme::accentBright.withAlpha (a));
    g.fillPath (stroked);

    g.setColour (Theme::textPrimary.withAlpha (a));
    g.setFont (Theme::font (24.0f, Theme::Weight::DemiBold));
    g.drawText ("Drop to analyse", card.withY (c.y + 46.0f).withHeight (30.0f), juce::Justification::centredTop, false);

    if (dragFileName.isNotEmpty())
    {
        g.setColour (Theme::textSecondary.withAlpha (a));
        g.setFont (Theme::font (13.0f, Theme::Weight::Medium));
        g.drawText (dragFileName, card.withY (c.y + 80.0f).withHeight (20.0f).reduced (20.0f, 0.0f),
                    juce::Justification::centredTop, true);
    }
}

// --- Status ------------------------------------------------------------------------

float ONYKeyMainPanel::listenLevel() const
{
    // Perceptual: -48 dBFS .. 0 dBFS -> 0..1.
    const float db = juce::Decibels::gainToDecibels (keyProcessor.getInputLevel(), -100.0f);
    return juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
}

juce::Colour ONYKeyMainPanel::statusColour() const
{
    if (display.failed || display.listening) return Theme::listenRed;
    if (display.analysing)                   return Theme::accent;
    return display.result.isValid() ? Theme::accent : Theme::textDim;
}

juce::String ONYKeyMainPanel::statusText() const
{
    const int hovered = wheel.getHoveredKey();
    if (hovered >= 0)
    {
        juce::StringArray mixes;
        for (int k = 0; k < 24; ++k)
            if (camelotCompatible (hovered, k))
                mixes.add (camelotCode (k));
        return keyName (hovered) + middleDot() + camelotCode (hovered) + "   mixes with " + mixes.joinIntoString (", ");
    }

    // (juce::String (x, 0) means "full precision", hence the int for >= 10 s.)
    const auto seconds = [] (double s) { return (s < 10.0 ? juce::String (s, 1) : juce::String (juce::roundToInt (s))) + " s"; };

    switch (display.source)
    {
        case ONYKeyAudioProcessor::Source::File:
            if (display.analysing)
                return "Analysing " + display.sourceName;
            if (display.failed)
                return "Couldn't read " + display.sourceName;
            if (display.result.isValid())
                return display.sourceName;
            return display.sourceName + middleDot() + "no pitched content found";

        case ONYKeyAudioProcessor::Source::Live:
            if (display.listening)
                return display.result.secondsAnalysed > 0.0
                           ? "Listening" + middleDot() + seconds (display.result.secondsAnalysed) + " heard"
                           : juce::String ("Listening for audio on this track");
            return display.result.isValid()
                       ? "From playback" + middleDot() + seconds (display.result.secondsAnalysed)
                       : juce::String ("Nothing pitched heard yet");

        case ONYKeyAudioProcessor::Source::None:
        default:
            return "Drag a sample here, open a file, or press LISTEN";
    }
}

// --- Updates ---------------------------------------------------------------------

void ONYKeyMainPanel::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    refresh();

    wheel.tick (dt);
    stats.tick (dt);
    chromaBars.tick (dt);
    for (auto* b : { &openButton, &listenButton, &clearButton })
        b->tick (dt);
    wordmark.tick (dt);
    themePicker.tick (dt);

    if (Theme::acidTripActive)
        stepAcidTrip (dt);

    // Transients while listening: the level jumping well above its recent
    // average fires a particle burst (Verb does the same off its orb).
    transientCooldown = juce::jmax (0.0f, transientCooldown - dt);
    const float level = display.listening ? listenLevel() : 0.0f;
    if (display.listening && level > 0.45f && level - levelFollower > 0.18f && transientCooldown <= 0.0f)
    {
        particles.spawnBurst (12);
        transientCooldown = 0.22f;
    }
    levelFollower += (level - levelFollower) * (1.0f - std::exp (-dt / 0.25f));

    if (ambience.tick (dt))
        repaint (wheelBounds.expanded (40));

    if (dropOverlay.tick (dt) || dropOverlay.value > 0.005f)
    {
        dashPhase += dt;
        repaint();
    }

    const auto status = statusText();
    if (status != lastStatus)
    {
        lastStatus = status;
        repaint (statusArea);
    }
}

void ONYKeyMainPanel::refresh()
{
    display = keyProcessor.getDisplay();

    using Activity = onykey::ui::KeyWheel::Activity;
    const auto activity = display.analysing ? Activity::Analysing
                        : display.listening ? Activity::Listening
                                            : Activity::Idle;
    const float level = display.listening ? listenLevel() : 0.0f;

    wheel.setState (display.result, activity, display.progress, level);

    using LogoMode = onykey::ui::Wordmark::Mode;
    wordmark.setMode (display.analysing ? LogoMode::Analysing
                    : display.listening ? LogoMode::Listening
                    : display.result.isValid() ? LogoMode::Found
                                               : LogoMode::Idle,
                      level);
    stats.setResult (display.result);
    chromaBars.setResult (display.result);

    // A celebratory burst when a key is found (or changes), at most once a second.
    const int signature = display.result.isValid() ? display.result.key * 2 + (int) display.result.kind : -1;
    if (signature != lastResultSignature)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (burstsArmed && signature >= 0 && now - lastResultBurstMs > 1000.0)
        {
            particles.spawnBurst (26);
            lastResultBurstMs = now;
        }
        lastResultSignature = signature;
    }

    ambience.set (display.result.isValid() ? 1.0f : 0.0f);

    listenButton.setToggleState (display.listening, juce::dontSendNotification);
    listenButton.setButtonText (display.listening ? "STOP" : "LISTEN");
    listenButton.setLedLevel (display.listening ? 0.55f + 0.45f * level : 0.0f);
    clearButton.setEnabled (display.source != ONYKeyAudioProcessor::Source::None);
}

void ONYKeyMainPanel::applyTheme (int index, bool save)
{
    const auto& palettes = Theme::getThemePalettes();
    if (! juce::isPositiveAndBelow (index, (int) palettes.size()))
        return;

    themeIndex = index;
    Theme::applyPalette (palettes[(size_t) index]);
    themePicker.setCurrent (index, hideNsfw);

    if (save)
        Theme::saveThemeIndex (index);

    // Everything reads Theme:: fresh when it paints; only the LookAndFeel's
    // baked-in colours (popups, tooltips) need re-applying.
    if (auto* lf = dynamic_cast<onykey::ui::OnyvaLookAndFeel*> (&getLookAndFeel()))
        lf->refreshColours();
    sendLookAndFeelChange();
    repaint();
}

void ONYKeyMainPanel::setHideNsfw (bool hide)
{
    hideNsfw = hide;
    Theme::saveHideNsfw (hide);

    // If the theme in use just got hidden, move to the first remaining
    // theme of the same light/dark kind.
    const int safe = hide ? Theme::firstSafeThemeIndex (themeIndex) : themeIndex;
    if (safe != themeIndex)
        applyTheme (safe, true);
    else
        themePicker.setCurrent (themeIndex, hideNsfw);
}

/** Acid Trip: the accent cycles through the rainbow (~6 s per lap), and the
    whole panel repaints at 30 Hz to follow it. */
void ONYKeyMainPanel::stepAcidTrip (float dt)
{
    acidPhase = std::fmod (acidPhase + dt * 0.17f, 1.0f);
    Theme::accent = juce::Colour::fromHSV (acidPhase, 0.9f, Theme::currentThemeIsLight ? 0.85f : 1.0f, 1.0f);
    Theme::accentDim = Theme::accent.darker (0.55f);
    Theme::accentGlow = Theme::accent.withAlpha (0.5f);
    Theme::deriveAccentColours();

    if ((++frameCounter & 1) == 0)
        repaint();
}

void ONYKeyMainPanel::copyResult()
{
    const auto& r = display.result;
    if (! r.isValid())
        return;

    const auto text = r.kind == KeyResult::Kind::RootNote
                          ? noteName (r.rootNote) + " (root note)"
                          : keyName (r.key) + " (" + camelotCode (r.key) + ")";
    juce::SystemClipboard::copyTextToClipboard (text);
    wheel.flashCopied();
    particles.spawnBurst (10);
}

// --- Files -------------------------------------------------------------------------

void ONYKeyMainPanel::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample to analyse",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   supportedAudioWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr)
                                  return;

                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                              {
                                  safeThis->keyProcessor.analyseFile (file);
                                  safeThis->refresh();
                              }
                          });
}

bool ONYKeyMainPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isSupportedAudioFile (juce::File (f)))
            return true;
    return false;
}

void ONYKeyMainPanel::fileDragEnter (const juce::StringArray& files, int, int)
{
    dragFileName = {};
    for (const auto& f : files)
    {
        if (isSupportedAudioFile (juce::File (f)))
        {
            dragFileName = juce::File (f).getFileName();
            break;
        }
    }

    frostedSnapshot = createComponentSnapshot (getLocalBounds(), true, 0.08f);
    dropOverlay.set (1.0f);
}

void ONYKeyMainPanel::fileDragExit (const juce::StringArray&)
{
    dropOverlay.set (0.0f);
}

void ONYKeyMainPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dropOverlay.set (0.0f);

    for (const auto& f : files)
    {
        const juce::File file (f);
        if (isSupportedAudioFile (file))
        {
            keyProcessor.analyseFile (file);
            break;
        }
    }

    refresh();
}

// =============================================================================
ONYKeyAudioProcessorEditor::ONYKeyAudioProcessorEditor (ONYKeyAudioProcessor& p)
    : AudioProcessorEditor (&p), keyProcessor (p), panel (p)
{
    setLookAndFeel (&lookAndFeel); // also styles the resize corner
    lookAndFeel.refreshColours();  // the panel has applied the saved theme by now
    addAndMakeVisible (panel);

    constexpr double minScale = 0.7, maxScale = 1.6;
    constrainer.setFixedAspectRatio ((double) ONYKeyMainPanel::designWidth / (double) ONYKeyMainPanel::designHeight);
    constrainer.setSizeLimits ((int) (ONYKeyMainPanel::designWidth * minScale), (int) (ONYKeyMainPanel::designHeight * minScale),
                               (int) (ONYKeyMainPanel::designWidth * maxScale), (int) (ONYKeyMainPanel::designHeight * maxScale));
    setConstrainer (&constrainer);
    setResizable (true, true);

    // First open: fit comfortably on screen (the design is tall); after
    // that, whatever size the user left it at.
    double scale = keyProcessor.getUiScale();
    if (scale <= 0.0)
    {
        scale = 1.0;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = juce::jlimit (0.8, 1.0, d->userBounds.getHeight() * 0.8 / (double) ONYKeyMainPanel::designHeight);
    }

    scale = juce::jlimit (minScale, maxScale, scale);
    setSize (juce::roundToInt (ONYKeyMainPanel::designWidth * scale), juce::roundToInt (ONYKeyMainPanel::designHeight * scale));
}

ONYKeyAudioProcessorEditor::~ONYKeyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ONYKeyAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) ONYKeyMainPanel::designWidth;
    panel.setBounds (0, 0, ONYKeyMainPanel::designWidth, ONYKeyMainPanel::designHeight);
    panel.setTransform (juce::AffineTransform::scale (scale, (float) getHeight() / (float) ONYKeyMainPanel::designHeight));
    keyProcessor.setUiScale (scale);
}
