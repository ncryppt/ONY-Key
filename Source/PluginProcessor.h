#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AnalysisThreads.h"
#include <mutex>

/**
    ONY Key: tells you the key of a sample.

    Two ways in:
      - drop (or open) an audio file on the editor: analysed in the
        background, start to finish;
      - LISTEN: analyses whatever plays through the track the plugin sits on.

    Audio passes through untouched. The latest result is saved with the
    session, so reopening a project shows the key it found last time.
*/
class ONYKeyAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class Source { None, File, Live };

    /** Everything the editor needs to draw, in one snapshot. */
    struct Display
    {
        onykey::KeyResult result;
        Source source = Source::None;
        juce::String sourceName;   // file name, for Source::File
        bool analysing = false;    // a file is being decoded/analysed
        float progress = 0.0f;
        bool failed = false;       // last file couldn't be decoded
        bool listening = false;
    };

    ONYKeyAudioProcessor();
    ~ONYKeyAudioProcessor() override;

    // --- Editor actions (message thread) -----------------------------------
    void analyseFile (const juce::File& file);
    void setListening (bool shouldListen);
    void clearResult();
    Display getDisplay();

    /** Peak input level of recent blocks (linear), for the LISTEN meter. */
    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }

    /** Editor scale (1 = design size), saved with the session. 0 = never set. */
    float getUiScale() const noexcept { return uiScale.load(); }
    void setUiScale (float s) noexcept { uiScale.store (s); }

    // --- AudioProcessor ----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    /** Pulls finished work from the analysis threads into `shown`.
        Caller holds `stateLock`. */
    void syncFromAnalysersLocked();

    onykey::LiveAnalyser live;
    onykey::FileAnalyserThread fileAnalyser;

    std::atomic<float> inputLevel { 0.0f }, uiScale { 0.0f };

    std::mutex stateLock;
    Source source = Source::None;
    juce::String sourceName;
    onykey::KeyResult shown;
    int seenFileGeneration = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ONYKeyAudioProcessor)
};
