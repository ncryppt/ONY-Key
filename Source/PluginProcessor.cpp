#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String floatsToString (const float* values, size_t count)
{
    juce::StringArray parts;
    for (size_t i = 0; i < count; ++i)
        parts.add (juce::String (values[i], 4));
    return parts.joinIntoString (" ");
}

template <size_t N>
void stringToFloats (const juce::String& text, std::array<float, N>& out)
{
    juce::StringArray parts;
    parts.addTokens (text, " ", "");
    for (size_t i = 0; i < N && (int) i < parts.size(); ++i)
        out[i] = parts[(int) i].getFloatValue();
}
} // namespace

ONYKeyAudioProcessor::ONYKeyAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

ONYKeyAudioProcessor::~ONYKeyAudioProcessor() = default;

// --- Editor actions ---------------------------------------------------------

void ONYKeyAudioProcessor::analyseFile (const juce::File& file)
{
    live.setListening (false);
    fileAnalyser.analyse (file);

    std::lock_guard<std::mutex> lock (stateLock);
    source = Source::File;
    sourceName = file.getFileName();
    shown = {};
}

void ONYKeyAudioProcessor::setListening (bool shouldListen)
{
    live.setListening (shouldListen);

    if (shouldListen)
    {
        std::lock_guard<std::mutex> lock (stateLock);
        source = Source::Live;
        sourceName = {};
        shown = {};
    }
}

void ONYKeyAudioProcessor::clearResult()
{
    live.setListening (false);

    std::lock_guard<std::mutex> lock (stateLock);
    source = Source::None;
    sourceName = {};
    shown = {};
    seenFileGeneration = fileAnalyser.getStatus().generation; // forget an analysis still in flight
}

void ONYKeyAudioProcessor::syncFromAnalysersLocked()
{
    if (source == Source::Live && live.isListening())
    {
        shown = live.getLatestResult();
    }
    else if (source == Source::File)
    {
        const auto status = fileAnalyser.getStatus();
        if (status.generation != seenFileGeneration && ! status.busy)
        {
            seenFileGeneration = status.generation;
            shown = status.result.value_or (onykey::KeyResult {});
        }
    }
}

ONYKeyAudioProcessor::Display ONYKeyAudioProcessor::getDisplay()
{
    std::lock_guard<std::mutex> lock (stateLock);
    syncFromAnalysersLocked();

    Display d;
    d.result = shown;
    d.source = source;
    d.sourceName = sourceName;
    d.listening = live.isListening();

    if (source == Source::File)
    {
        const auto status = fileAnalyser.getStatus();
        d.analysing = status.busy;
        d.progress = status.progress;
        d.failed = ! status.busy && status.failed && status.generation == seenFileGeneration;
    }

    return d;
}

// --- Audio ------------------------------------------------------------------

void ONYKeyAudioProcessor::prepareToPlay (double sampleRate, int)
{
    live.prepare (sampleRate);
}

bool ONYKeyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void ONYKeyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pass-through: the plugin only listens. Anything beyond the input
    // channels is cleared so hosts don't get garbage on unused outputs.
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int channels = juce::jmin (numIn, buffer.getNumChannels());
    live.push (buffer, channels);

    if (live.isListening())
    {
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        inputLevel.store (peak, std::memory_order_relaxed);
    }
    else
    {
        inputLevel.store (0.0f, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* ONYKeyAudioProcessor::createEditor()
{
    return new ONYKeyAudioProcessorEditor (*this);
}

// --- State ------------------------------------------------------------------

void ONYKeyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::lock_guard<std::mutex> lock (stateLock);
    syncFromAnalysersLocked();

    juce::ValueTree state ("ONYKey");
    state.setProperty ("version", 1, nullptr);
    state.setProperty ("source", source == Source::File ? "file" : source == Source::Live ? "live" : "none", nullptr);
    state.setProperty ("name", sourceName, nullptr);
    if (uiScale.load() > 0.0f)
        state.setProperty ("uiScale", uiScale.load(), nullptr);

    if (shown.isValid())
    {
        state.setProperty ("kind", shown.kind == onykey::KeyResult::Kind::RootNote ? "root" : "key", nullptr);
        state.setProperty ("key", shown.key, nullptr);
        state.setProperty ("alt", shown.altKey, nullptr);
        state.setProperty ("root", shown.rootNote, nullptr);
        state.setProperty ("confidence", shown.confidence, nullptr);
        state.setProperty ("tuning", shown.tuningCents, nullptr);
        state.setProperty ("seconds", shown.secondsAnalysed, nullptr);
        state.setProperty ("chroma", floatsToString (shown.chroma.data(), shown.chroma.size()), nullptr);
        state.setProperty ("scores", floatsToString (shown.keyScores.data(), shown.keyScores.size()), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ONYKeyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("ONYKey"))
        return;

    onykey::KeyResult r;
    const auto kind = state.getProperty ("kind").toString();
    const int key = state.getProperty ("key", -1);

    if ((kind == "key" || kind == "root") && key >= 0 && key < 24)
    {
        r.kind = kind == "root" ? onykey::KeyResult::Kind::RootNote : onykey::KeyResult::Kind::Key;
        r.key = key;
        r.altKey = juce::jlimit (-1, 23, (int) state.getProperty ("alt", -1));
        r.rootNote = juce::jlimit (0, 11, (int) state.getProperty ("root", key % 12));
        r.confidence = (float) state.getProperty ("confidence", 0.0);
        r.tuningCents = (float) state.getProperty ("tuning", 0.0);
        r.secondsAnalysed = (double) state.getProperty ("seconds", 0.0);
        stringToFloats (state.getProperty ("chroma").toString(), r.chroma);
        stringToFloats (state.getProperty ("scores").toString(), r.keyScores);
    }

    live.setListening (false);
    uiScale.store ((float) state.getProperty ("uiScale", 0.0));

    std::lock_guard<std::mutex> lock (stateLock);
    const auto src = state.getProperty ("source").toString();
    // A restored result is a finished one: show it as-is, whatever produced it.
    source = r.isValid() ? (src == "file" ? Source::File : Source::Live) : Source::None;
    sourceName = state.getProperty ("name").toString();
    shown = r;
    seenFileGeneration = fileAnalyser.getStatus().generation;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ONYKeyAudioProcessor();
}
