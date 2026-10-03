#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "FileAnalyser.h"
#include <atomic>
#include <mutex>

namespace onykey
{

/**
    "Listen" mode: analyses the audio flowing through the plugin.

    The audio thread only mixes to mono and pushes into a lock-free FIFO
    (no allocation, no locks); this background thread drains it into a
    KeyDetector and publishes a fresh result a few times a second.
*/
class LiveAnalyser final : private juce::Thread
{
public:
    LiveAnalyser() : juce::Thread ("ONY Key live analyser") {}
    ~LiveAnalyser() override { stopThread (2000); }

    /** Message thread, from prepareToPlay(). Allocates. */
    void prepare (double newSampleRate)
    {
        sampleRate.store (newSampleRate);
        resetRequested.store (true);
        if (! isThreadRunning())
            startThread (juce::Thread::Priority::low);
    }

    /** Audio thread. Real-time safe. */
    void push (const juce::AudioBuffer<float>& buffer, int numChannels)
    {
        if (! listening.load (std::memory_order_relaxed) || numChannels <= 0)
            return;

        const int n = buffer.getNumSamples();
        const auto scope = fifo.write (juce::jmin (n, fifo.getFreeSpace()));
        const float gain = 1.0f / (float) numChannels;

        auto mixInto = [&] (int fifoStart, int count, int sourceStart)
        {
            for (int i = 0; i < count; ++i)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    sum += buffer.getSample (ch, sourceStart + i);
                fifoData[(size_t) (fifoStart + i)] = sum * gain;
            }
        };

        mixInto (scope.startIndex1, scope.blockSize1, 0);
        mixInto (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    /** Starts listening from scratch (or pauses, keeping what was heard). */
    void setListening (bool shouldListen)
    {
        if (shouldListen && ! listening.load())
        {
            resetRequested.store (true);
            std::lock_guard<std::mutex> lock (resultLock);
            latest = {};
        }
        listening.store (shouldListen);
        notify();
    }

    bool isListening() const noexcept { return listening.load(); }

    KeyResult getLatestResult() const
    {
        std::lock_guard<std::mutex> lock (resultLock);
        return latest;
    }

private:
    static constexpr int fifoSize = 1 << 18; // ~2.7 s at 96 kHz between drains

    void run() override
    {
        std::vector<float> scratch ((size_t) fifoSize);
        auto lastPublish = juce::Time::getMillisecondCounter();

        while (! threadShouldExit())
        {
            wait (40);

            if (resetRequested.exchange (false))
            {
                // Discard (rather than AbstractFifo::reset(), which isn't
                // safe while the audio thread may be writing).
                fifo.read (fifo.getNumReady());
                detector.prepare (sampleRate.load());
            }

            const int ready = fifo.getNumReady();
            if (ready > 0)
            {
                const auto scope = fifo.read (ready);
                std::copy_n (fifoData.begin() + scope.startIndex1, scope.blockSize1, scratch.begin());
                std::copy_n (fifoData.begin() + scope.startIndex2, scope.blockSize2, scratch.begin() + scope.blockSize1);
                detector.process (scratch.data(), ready);
            }

            const auto now = juce::Time::getMillisecondCounter();
            if (listening.load() && now - lastPublish > 400)
            {
                lastPublish = now;
                auto result = detector.computeResult();
                std::lock_guard<std::mutex> lock (resultLock);
                latest = result;
            }
        }
    }

    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoData = std::vector<float> ((size_t) fifoSize);

    KeyDetector detector;
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<bool> listening { false }, resetRequested { true };

    mutable std::mutex resultLock;
    KeyResult latest;
};

/**
    Decodes and analyses dropped / chosen files on a background thread.
    A new request cancels one still in progress.
*/
class FileAnalyserThread final : private juce::Thread
{
public:
    struct Status
    {
        bool busy = false;
        float progress = 0.0f;
        juce::File file;
        std::optional<KeyResult> result; // empty while busy, or if decoding failed
        bool failed = false;
        int generation = 0;             // bumps on every finished analysis
    };

    FileAnalyserThread() : juce::Thread ("ONY Key file analyser")
    {
        formats.registerBasicFormats();
    }

    ~FileAnalyserThread() override { stopThread (4000); }

    void analyse (const juce::File& file)
    {
        {
            std::lock_guard<std::mutex> lock (statusLock);
            pending = file;
            status.busy = true;
            status.progress = 0.0f;
            status.file = file;
            status.result.reset();
            status.failed = false;
            requestId.fetch_add (1); // under the lock, so run() sees file and id together
        }

        if (! isThreadRunning())
            startThread (juce::Thread::Priority::normal);
        notify();
    }

    Status getStatus() const
    {
        std::lock_guard<std::mutex> lock (statusLock);
        return status;
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            juce::File file;
            int id = 0;
            {
                std::lock_guard<std::mutex> lock (statusLock);
                file = pending;
                pending = juce::File();
                id = requestId.load();
            }

            if (file == juce::File())
            {
                wait (-1);
                continue;
            }

            auto result = analyseAudioFile (
                file, formats,
                [this, id] { return threadShouldExit() || requestId.load() != id; },
                [this, id] (float p)
                {
                    std::lock_guard<std::mutex> lock (statusLock);
                    if (requestId.load() == id)
                        status.progress = p;
                });

            std::lock_guard<std::mutex> lock (statusLock);
            if (requestId.load() != id || threadShouldExit())
                continue; // superseded by a newer drop

            status.busy = false;
            status.progress = 1.0f;
            status.result = result;
            status.failed = ! result.has_value();
            ++status.generation;
        }
    }

    juce::AudioFormatManager formats;
    std::atomic<int> requestId { 0 };

    mutable std::mutex statusLock;
    juce::File pending;
    Status status;
};

} // namespace onykey
