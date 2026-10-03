#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "UI/OnyvaLookAndFeel.h"
#include "UI/KeyWheel.h"
#include "UI/ChromaBars.h"
#include "UI/StatCards.h"
#include "UI/IconButton.h"
#include "UI/Wordmark.h"
#include "UI/ThemePicker.h"
#include "UI/ParticleOverlay.h"

/** Everything you see, laid out at a fixed design size; the editor scales it. */
class ONYKeyMainPanel final : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              private juce::Timer
{
public:
    static constexpr int designWidth = 480, designHeight = 816;

    explicit ONYKeyMainPanel (ONYKeyAudioProcessor&);
    ~ONYKeyMainPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void refresh();
    void openFileChooser();
    void copyResult();
    void applyTheme (int index, bool save);
    void setHideNsfw (bool hide);
    void stepAcidTrip (float dt);
    juce::String statusText() const;
    juce::Colour statusColour() const;
    float listenLevel() const;

    ONYKeyAudioProcessor& keyProcessor;

    onykey::ui::Wordmark wordmark;
    onykey::ui::KeyWheel wheel;
    onykey::ui::StatCards stats;
    onykey::ui::ChromaBars chromaBars;
    onykey::ui::IconButton openButton { "OPEN FILE", onykey::ui::IconButton::Icon::Folder },
                           listenButton { "LISTEN", onykey::ui::IconButton::Icon::Listen, true },
                           clearButton { "CLEAR", onykey::ui::IconButton::Icon::Clear };
    onykey::ui::ThemePicker themePicker;
    onykey::ui::ParticleOverlay particles; // above everything else, click-through
    juce::TooltipWindow tooltips { this, 700 };

    // Particle triggers: a burst when a (new) key is found, and on
    // transients in the input while listening.
    int lastResultSignature = -1;
    bool burstsArmed = false; // not for a result restored when the editor opens
    double lastResultBurstMs = 0.0;
    float levelFollower = 0.0f, transientCooldown = 0.0f;

    int themeIndex = onykey::ui::Theme::defaultThemeIndex;
    bool hideNsfw = false;
    float acidPhase = 0.0f;
    int frameCounter = 0;

    std::unique_ptr<juce::FileChooser> chooser;
    ONYKeyAudioProcessor::Display display;

    // Background accent glow behind the wheel: fades up with a result.
    onykey::ui::Eased ambience;

    // Drag-and-drop overlay.
    onykey::ui::Eased dropOverlay;
    juce::Image frostedSnapshot;
    juce::String dragFileName;
    float dashPhase = 0.0f;

    juce::String lastStatus;
    juce::Rectangle<int> statusArea;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ONYKeyMainPanel)
};

/** Plugin editor: hosts the main panel and scales it to any size at the
    design's aspect ratio (like ONY Verb). The chosen size is remembered
    with the session. */
class ONYKeyAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ONYKeyAudioProcessorEditor (ONYKeyAudioProcessor&);
    ~ONYKeyAudioProcessorEditor() override;

    void resized() override;

private:
    ONYKeyAudioProcessor& keyProcessor;
    onykey::ui::OnyvaLookAndFeel lookAndFeel; // declared before the panel so it outlives it
    ONYKeyMainPanel panel;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ONYKeyAudioProcessorEditor)
};
