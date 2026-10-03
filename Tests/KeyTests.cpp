// Headless unit tests for the key detector (JUCE UnitTest, no extra framework).

#include <juce_core/juce_core.h>
#include "DSP/KeyDetector.h"
#include "DSP/KeyNames.h"

namespace
{
using namespace onykey;

/** Harmonically rich tone (8 partials, 1/n rolloff) with a short fade so
    chord changes don't click. */
void addTone (std::vector<float>& out, double sampleRate, double startSec, double lengthSec,
              double midiNote, float gain, double centsOffset = 0.0)
{
    const double freq = 440.0 * std::pow (2.0, (midiNote - 69.0 + centsOffset / 100.0) / 12.0);
    const auto start = (size_t) (startSec * sampleRate);
    const auto len = (size_t) (lengthSec * sampleRate);
    const auto fade = (size_t) (0.01 * sampleRate);

    for (size_t i = 0; i < len && start + i < out.size(); ++i)
    {
        const double t = (double) i / sampleRate;
        double s = 0.0;
        for (int h = 1; h <= 8; ++h)
            if (freq * h < sampleRate * 0.45)
                s += std::sin (juce::MathConstants<double>::twoPi * freq * h * t) / h;

        float env = 1.0f;
        if (i < fade) env = (float) i / (float) fade;
        if (len - i < fade) env = (float) (len - i) / (float) fade;

        out[start + i] += (float) s * gain * env;
    }
}

/** I-IV-V-I (major) or i-iv-V-i (minor) with a bass note under each chord. */
std::vector<float> makeProgression (int key, double sampleRate, double cents = 0.0)
{
    const bool minor = isMinorKey (key);
    const int tonic = 48 + tonicOf (key); // C3..B3

    const int third = minor ? 3 : 4;
    const std::vector<std::vector<int>> chords = minor
        ? std::vector<std::vector<int>> { { 0, 3, 7 }, { 5, 8, 12 }, { 7, 11, 14 }, { 0, 3, 7 } }
        : std::vector<std::vector<int>> { { 0, 4, 7 }, { 5, 9, 12 }, { 7, 11, 14 }, { 0, third, 7 } };

    const double chordLen = 2.0;
    std::vector<float> out ((size_t) (sampleRate * chordLen * (double) chords.size()), 0.0f);

    for (size_t c = 0; c < chords.size(); ++c)
    {
        for (auto interval : chords[c])
            addTone (out, sampleRate, (double) c * chordLen, chordLen, tonic + interval, 0.08f, cents);

        addTone (out, sampleRate, (double) c * chordLen, chordLen, tonic - 12 + chords[c][0], 0.12f, cents);
    }

    return out;
}

KeyResult detect (const std::vector<float>& audio, double sampleRate)
{
    KeyDetector detector;
    detector.prepare (sampleRate);
    // Feed in odd-sized blocks, the way a host would.
    for (size_t pos = 0; pos < audio.size(); pos += 517)
        detector.process (audio.data() + pos, (int) juce::jmin ((size_t) 517, audio.size() - pos));
    detector.flush();
    return detector.computeResult();
}

class KeyDetectorTests final : public juce::UnitTest
{
public:
    KeyDetectorTests() : juce::UnitTest ("KeyDetector", "ONY Key") {}

    void runTest() override
    {
        beginTest ("All 24 keys are detected from a cadence (44.1 kHz and 48 kHz)");
        for (double sr : { 44100.0, 48000.0 })
        {
            for (int key = 0; key < 24; ++key)
            {
                const auto r = detect (makeProgression (key, sr), sr);
                expect (r.kind == KeyResult::Kind::Key, "not detected as a key: " + keyName (key));
                expectEquals (keyName (r.key), keyName (key));
            }
        }

        beginTest ("Detuned material: key still found, tuning offset reported");
        for (double cents : { -35.0, 28.0 })
        {
            const auto r = detect (makeProgression (9 + 12, 44100.0, cents), 44100.0); // A minor
            expectEquals (keyName (r.key), keyName (21));
            expectWithinAbsoluteError (r.tuningCents, (float) cents, 6.0f);
        }

        beginTest ("96 kHz input");
        {
            const auto r = detect (makeProgression (7, 96000.0), 96000.0); // G major
            expectEquals (keyName (r.key), keyName (7));
        }

        beginTest ("Single note is reported as a root note");
        for (int midi : { 29, 41, 58, 66 }) // F1 (808 territory), F2, A#3, F#4
        {
            std::vector<float> audio ((size_t) (44100 * 1.5), 0.0f);
            addTone (audio, 44100.0, 0.0, 1.5, midi, 0.5f);
            const auto r = detect (audio, 44100.0);
            expect (r.kind == KeyResult::Kind::RootNote, "single note " + juce::String (midi) + " not flagged as root note");
            expectEquals (r.rootNote, midi % 12);
        }

        beginTest ("Very short one-shot (< 1 FFT frame) still gives a result");
        {
            std::vector<float> audio ((size_t) (44100 * 0.25), 0.0f);
            for (int n : { 60, 63, 67 }) // C minor triad
                addTone (audio, 44100.0, 0.0, 0.25, n, 0.2f);
            const auto r = detect (audio, 44100.0);
            expect (r.isValid());
            expectEquals (tonicOf (r.key), 0);
        }

        beginTest ("Silence gives no result");
        {
            std::vector<float> audio (44100 * 3, 0.0f);
            expect (! detect (audio, 44100.0).isValid());
        }

        beginTest ("NaN/inf input is ignored rather than poisoning the result");
        {
            auto audio = makeProgression (2, 44100.0); // D major
            audio[1000] = std::numeric_limits<float>::quiet_NaN();
            audio[5000] = std::numeric_limits<float>::infinity();
            const auto r = detect (audio, 44100.0);
            expectEquals (keyName (r.key), keyName (2));
        }

        beginTest ("Key naming and Camelot codes");
        expectEquals (camelotCode (0), juce::String ("8B"));   // C major
        expectEquals (camelotCode (21), juce::String ("8A"));  // A minor
        expectEquals (camelotCode (18), juce::String ("11A")); // F# minor
        expectEquals (camelotCode (1), juce::String ("3B"));   // Db major
        expectEquals (camelotCode (5), juce::String ("7B"));   // F major
        expectEquals (camelotCode (20), juce::String ("1A"));  // G# minor
        expectEquals (relativeKeyOf (21), 0);
        expectEquals (relativeKeyOf (0), 21);
        expectEquals (shortKeyName (13), juce::String ("C#m"));
        expectEquals (keyName (10), juce::String ("Bb major"));
        expectEquals (withMusicSymbols ("Bbm"), juce::String (juce::CharPointer_UTF8 ("B\xe2\x99\xadm")));
        expectEquals (withMusicSymbols ("F#"), juce::String (juce::CharPointer_UTF8 ("F\xe2\x99\xaf")));
        expectEquals (withMusicSymbols ("B"), juce::String ("B"));
        expectEquals (noteNameInKey (10, 7 + 12), juce::String ("Bb")); // G minor
        expectEquals (noteNameInKey (10, 6 + 12), juce::String ("A#")); // F# minor
        expectEquals (noteNameInKey (6, 3 + 12), juce::String ("Gb"));  // Eb minor
        expectEquals (keyRelation (21, 0), juce::String ("relative major"));
        expect (camelotCompatible (21, 16) && camelotCompatible (21, 14) && camelotCompatible (21, 0)); // 8A: 9A, 7A, 8B
        expect (! camelotCompatible (21, 7));
    }
};

static KeyDetectorTests keyDetectorTests;
} // namespace

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (runner.getResult (i)->failures > 0)
            return 1;

    return 0;
}
