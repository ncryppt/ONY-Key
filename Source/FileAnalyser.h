#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/KeyDetector.h"
#include <functional>
#include <optional>

namespace onykey
{

/** Wildcard of the formats the plugin advertises in its file chooser. */
inline juce::String supportedAudioWildcard()
{
    return "*.wav;*.wave;*.aif;*.aiff;*.aifc;*.flac;*.ogg;*.mp3;*.m4a;*.caf";
}

inline bool isSupportedAudioFile (const juce::File& f)
{
    return f.existsAsFile()
        && f.hasFileExtension ("wav;wave;aif;aiff;aifc;flac;ogg;mp3;m4a;caf");
}

/** Decodes an audio file (mixed down to mono) and runs it through a
    KeyDetector. Blocking; call it from a background thread.

    `shouldStop` is polled between chunks so a newer drop can cancel an
    older analysis. `onProgress` gets 0..1. `configure` can adjust the
    detector's tunables (used by the evaluation tool). Returns nullopt if
    the file can't be decoded or the analysis was cancelled. */
inline std::optional<KeyResult> analyseAudioFile (const juce::File& file,
                                                  juce::AudioFormatManager& formats,
                                                  const std::function<bool()>& shouldStop = {},
                                                  const std::function<void (float)>& onProgress = {},
                                                  const std::function<void (KeyDetector&)>& configure = {})
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return std::nullopt;

    // Twenty minutes is far beyond any sample or song; this just stops a
    // mis-dropped multi-hour recording from tying up the analyser.
    const auto maxSamples = (juce::int64) (reader->sampleRate * 60.0 * 20.0);
    const auto totalSamples = juce::jmin (reader->lengthInSamples, maxSamples);
    const int numChannels = (int) juce::jlimit (1u, 8u, reader->numChannels);

    KeyDetector detector;
    if (configure)
        configure (detector);
    detector.prepare (reader->sampleRate);

    constexpr int chunkSize = 1 << 16;
    juce::AudioBuffer<float> buffer (numChannels, chunkSize);
    std::vector<float> mono ((size_t) chunkSize);

    for (juce::int64 pos = 0; pos < totalSamples; pos += chunkSize)
    {
        if (shouldStop && shouldStop())
            return std::nullopt;

        const int n = (int) juce::jmin ((juce::int64) chunkSize, totalSamples - pos);
        if (! reader->read (&buffer, 0, n, pos, true, numChannels > 1))
            return std::nullopt;

        const float gain = 1.0f / (float) numChannels;
        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += buffer.getSample (ch, i);
            mono[(size_t) i] = sum * gain;
        }

        detector.process (mono.data(), n);

        if (onProgress)
            onProgress ((float) ((double) (pos + n) / (double) totalSamples));
    }

    detector.flush();
    return detector.computeResult();
}

} // namespace onykey
