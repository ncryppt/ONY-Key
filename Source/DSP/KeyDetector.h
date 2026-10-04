#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#include "KeyModel.h"

namespace onykey
{

/** What the detector concluded about the audio it has heard so far. */
struct KeyResult
{
    enum class Kind
    {
        None,     // nothing tonal heard yet
        Key,      // a major/minor key
        RootNote  // essentially one pitch (one-shot, 808, drone): the note is
                  // reliable, major vs minor isn't — `key` still holds the
                  // best guess so the UI can offer it as a hint
    };

    Kind kind = Kind::None;
    int key = -1;              // 0-11 major, 12-23 minor (see KeyNames.h)
    int altKey = -1;           // runner-up key
    int rootNote = -1;         // pitch class 0-11 (the tonic, or the note for RootNote)
    float confidence = 0.0f;   // 0..1, how clearly `key` beat the rest
    float tuningCents = 0.0f;  // how far the material sits from A=440 (-50..+50)
    double secondsAnalysed = 0.0;

    std::array<float, 12> chroma {};     // pitch-class energy, max = 1
    std::array<float, 24> keyScores {};  // probability per key (sums to 1)

    bool isValid() const noexcept { return kind != Kind::None; }
};

/**
    Streaming key detector.

    Pipeline:
      source audio -> 4th-order low-pass -> resample to 11025 Hz
      -> 8192-point Blackman-Harris FFT every 2048 samples (~1.35 Hz bins)
      -> spectral peaks (parabolic interpolation), kept only if they stand
         clear of their surroundings, so drums and noise don't register
      -> overtones that aren't octaves (3rd, 5th, 7th harmonic...) of a louder
         lower peak are dropped: the 5th harmonic of a bass note is a major
         third, and it's the main reason naive detectors call minor loops major
      -> peaks folded into 10-cent pitch-class histograms for the whole
         range, the bass (< 200 Hz) and the middle register (200 Hz - 1 kHz)
      -> tuning offset estimated at the end and folded out, so detuned
         samples still land on the right notes
      -> a small learned model (KeyModel.h) scores all 24 keys from the
         per-register chroma, rotated to each candidate tonic, plus its
         correlation with Sha'ath's key profiles; a softmax turns the scores
         into probabilities, and the winner's probability is the confidence.

    The model was trained and evaluated with scripts/train_key_model.py on
    ~2,800 labelled files (sample-pack loops, the GiantSteps Key EDM
    benchmark, and rendered textbook progressions in all 24 keys), always
    scoring on packs / folds / progression types held out from training.
    Held out, it scores 74.7% on GiantSteps' weighted (MIREX) score vs 66.2%
    for the previous hand-tuned profile matcher, and reads 67% of chord
    progressions it never heard (90% as the key or its relative); its
    confidence is calibrated (an 85%+ answer was right 87% of the time).

    Pure juce_core/juce_dsp: no plugin or GUI dependency, so the plugin's
    live "listen" path, the file analyser and the unit tests all run the
    exact same code. Not thread-safe; one owner thread per instance.
*/
class KeyDetector
{
public:
    static constexpr double analysisRate = 11025.0;
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = 2048;
    static constexpr int binsPerSemitone = 10;
    static constexpr int numFineBins = 12 * binsPerSemitone;

    static constexpr double minFrequency = 40.0;   // ~E1, covers 808s/sub bass
    static constexpr double maxFrequency = 4200.0; // ~C8
    static constexpr double bassCutoffHz = 200.0;
    static constexpr double midCutoffHz = 1000.0;

    // Tunables, public for the evaluation tool. Defaults are the tuned values.
    double harmonicSuppression = 0.0;    // weight left on a non-octave overtone (0 = removed)
    double prominenceDb = 6.0;           // how far a peak must rise above its neighbourhood
    float rootNoteOtherThreshold = 0.22f;

    KeyDetector() : fft (fftOrder)
    {
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
        {
            // 4-term Blackman-Harris: -92 dB sidelobes. With Hann (-31 dB)
            // the sidelobes of a strong low note get picked as peaks of
            // their own, a fraction of a semitone away.
            const double x = juce::MathConstants<double>::twoPi * (double) i / (double) fftSize;
            window[(size_t) i] = (float) (0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x));
        }

        fftData.resize ((size_t) fftSize * 2);
        frameBuffer.reserve ((size_t) fftSize * 2);
        peaks.reserve (1024);
        prepare (44100.0);
    }

    /** Sets the incoming sample rate and clears everything heard so far. */
    void prepare (double sourceSampleRate)
    {
        sourceRate = sourceSampleRate > 0.0 ? sourceSampleRate : 44100.0;
        resampleStep = sourceRate / analysisRate;
        useLowPass = sourceRate > analysisRate * 1.05;

        if (useLowPass)
        {
            const auto cutoff = (float) juce::jmin (0.42 * analysisRate, 0.45 * sourceRate);
            // Butterworth 4th order as two biquads.
            lowPass[0].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sourceRate, cutoff, 0.5412f);
            lowPass[1].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sourceRate, cutoff, 1.3066f);
        }

        reset();
    }

    void reset()
    {
        for (auto& f : lowPass)
            f.reset();

        resamplePos = 1.0;
        previousSample = 0.0f;
        frameBuffer.clear();
        samplesSinceLastFrame = 0;
        fineHistogram.fill (0.0);
        bassHistogram.fill (0.0);
        midHistogram.fill (0.0);
        analysedSamples = 0;
        tonalFrames = 0;
    }

    /** Feed mono audio at the rate given to prepare(). */
    void process (const float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            auto x = samples[i];
            if (! std::isfinite (x))
                x = 0.0f;

            if (useLowPass)
                x = lowPass[1].processSample (lowPass[0].processSample (x));

            // Linear-interpolating resampler: the low-pass above has already
            // removed everything the 11025 Hz analysis rate can't represent,
            // and pitch-class analysis is very forgiving of interpolation error.
            while (resamplePos <= 1.0)
            {
                pushAnalysisSample (previousSample + (float) resamplePos * (x - previousSample));
                resamplePos += resampleStep;
            }

            resamplePos -= 1.0;
            previousSample = x;
        }
    }

    /** Analyse whatever is still buffered (zero-padded). Call once at the end
        of a file so short one-shots — shorter than a single FFT frame — still
        produce a result. */
    void flush()
    {
        const bool neverAnalysed = analysedSamples == 0 && ! frameBuffer.empty();
        if (neverAnalysed || samplesSinceLastFrame > hopSize / 2)
        {
            frameBuffer.resize ((size_t) fftSize, 0.0f);
            analyseFrame();
        }
    }

    double getSecondsAnalysed() const noexcept { return (double) analysedSamples / analysisRate; }

    /** Raw evidence, for the evaluation/training tools: 10-cent pitch-class
        histograms for the whole range, the bass (< 200 Hz) and the middle
        register (200 Hz - 1 kHz). */
    struct Evidence
    {
        std::array<double, numFineBins> all {}, bass {}, mid {};
        int tonalFrames = 0;
    };

    Evidence getEvidence() const
    {
        return { fineHistogram, bassHistogram, midHistogram, tonalFrames };
    }

    /** Where, within a semitone, the energy sits: -0.5..0.5 semitones. */
    double estimateTuning() const
    {
        std::complex<double> phasor;
        for (int b = 0; b < numFineBins; ++b)
            phasor += fineHistogram[(size_t) b]
                      * std::polar (1.0, juce::MathConstants<double>::twoPi * (double) b / (double) binsPerSemitone);
        return std::arg (phasor) / juce::MathConstants<double>::twoPi;
    }

    KeyResult computeResult() const
    {
        KeyResult result;
        result.secondsAnalysed = getSecondsAnalysed();

        double total = 0.0;
        for (auto v : fineHistogram)
            total += v;

        if (total <= 1.0e-9 || tonalFrames == 0)
            return result;

        const double tuningSemitones = estimateTuning();
        result.tuningCents = (float) (tuningSemitones * 100.0);

        // --- Fold to 12 pitch classes per register, tuning removed ----------
        std::array<double, 12> chroma {}, bass {}, mid {};
        for (int b = 0; b < numFineBins; ++b)
        {
            const double semis = (double) b / (double) binsPerSemitone - tuningSemitones;
            const auto pc = (size_t) (((int) std::lround (semis) % 12 + 12) % 12);
            chroma[pc] += fineHistogram[(size_t) b];
            bass[pc] += bassHistogram[(size_t) b];
            mid[pc] += midHistogram[(size_t) b];
        }

        const double chromaMax = *std::max_element (chroma.begin(), chroma.end());
        for (size_t i = 0; i < 12; ++i)
            result.chroma[i] = (float) (chroma[i] / chromaMax);

        // --- Score every key with the learned model -------------------------
        result.keyScores = scoreKeys (chroma, bass, mid);

        int best = 0, second = 1;
        if (result.keyScores[1] > result.keyScores[0])
            std::swap (best, second);

        for (int key = 2; key < 24; ++key)
        {
            if (result.keyScores[(size_t) key] > result.keyScores[(size_t) best])
            {
                second = best;
                best = key;
            }
            else if (result.keyScores[(size_t) key] > result.keyScores[(size_t) second])
            {
                second = key;
            }
        }

        result.key = best;
        result.altKey = second;
        result.rootNote = best % 12;
        // The model's own probability, which is calibrated on held-out packs.
        result.confidence = result.keyScores[(size_t) best];

        // --- One note (plus its own overtones) only? ------------------------
        // A single pitched sound puts its energy on its root and, through any
        // overtones that survived, the fifth above. If nothing else carries
        // real weight there is no third to decide major vs minor from.
        const auto root = (int) std::distance (result.chroma.begin(),
                                               std::max_element (result.chroma.begin(), result.chroma.end()));
        bool onlyRootAndFifth = true;
        for (int pc = 0; pc < 12; ++pc)
            if (pc != root && pc != (root + 7) % 12 && result.chroma[(size_t) pc] > rootNoteOtherThreshold)
                onlyRootAndFifth = false;

        if (onlyRootAndFifth)
        {
            result.kind = KeyResult::Kind::RootNote;
            result.rootNote = root;
        }
        else
        {
            result.kind = KeyResult::Kind::Key;
        }

        return result;
    }

private:
    /** Per-register chroma -> the model's input for each candidate tonic,
        and the softmax over all 24 keys. Must match features() / rotations()
        in scripts/train_key_model.py exactly. */
    static std::array<float, 24> scoreKeys (const std::array<double, 12>& all, const std::array<double, 12>& bass,
                                            const std::array<double, 12>& mid)
    {
        // Bands in model order: bass, mid, high, all. Each max-normalised,
        // square-root compressed and mean-centred.
        std::array<std::array<double, 12>, 4> bands {};
        for (size_t i = 0; i < 12; ++i)
        {
            bands[0][i] = bass[i];
            bands[1][i] = mid[i];
            bands[2][i] = juce::jmax (0.0, all[i] - bass[i] - mid[i]);
            bands[3][i] = all[i];
        }
        for (auto& band : bands)
        {
            const double peak = *std::max_element (band.begin(), band.end());
            double mean = 0.0;
            for (auto& v : band)
            {
                v = peak > 0.0 ? std::sqrt (v / peak) : 0.0;
                mean += v;
            }
            mean /= 12.0;
            for (auto& v : band)
                v -= mean;
        }

        std::array<double, 12> mix {};
        for (size_t i = 0; i < 12; ++i)
            mix[i] = bands[3][i] + 0.35 * bands[0][i];

        std::array<double, 24> logits {};
        std::array<float, (size_t) model::inputs> x {};

        for (int tonic = 0; tonic < 12; ++tonic)
        {
            // Rotate so the candidate tonic sits at index 0.
            for (size_t band = 0; band < 4; ++band)
                for (int i = 0; i < 12; ++i)
                    x[band * 12 + (size_t) i] = (float) bands[band][(size_t) ((i + tonic) % 12)];

            if constexpr (model::usesCorrelations)
            {
                x[48] = (float) correlate (bands[3], tonic);
                x[49] = (float) correlate (bands[3], 12 + tonic);
                x[50] = (float) correlate (mix, tonic);
                x[51] = (float) correlate (mix, 12 + tonic);
            }

            for (int mode = 0; mode < 2; ++mode)
            {
                double score = model::b2[mode];
                for (int h = 0; h < model::hidden; ++h)
                {
                    double z = model::b1[mode][h];
                    for (int d = 0; d < model::inputs; ++d)
                        z += (double) model::W1[mode][d][h] * x[(size_t) d];
                    score += (double) model::w2[mode][h] * (model::linear ? z : juce::jmax (0.0, z));
                }
                logits[(size_t) (mode * 12 + tonic)] = score;
            }
        }

        const double top = *std::max_element (logits.begin(), logits.end());
        double sum = 0.0;
        std::array<double, 24> e {};
        for (size_t k = 0; k < 24; ++k)
        {
            e[k] = std::exp (logits[k] - top);
            sum += e[k];
        }
        std::array<float, 24> p {};
        for (size_t k = 0; k < 24; ++k)
            p[k] = (float) (e[k] / sum);
        return p;
    }

    /** Sha'ath's profiles (from libKeyFinder), which scored best on
        electronic loops among Krumhansl-Kessler, Temperley, Albrecht-Shanahan
        and Faraldo's EDMA. */
    static const std::array<double, 12>& profileFor (bool minor)
    {
        static const std::array<double, 12> majorProfile { 6.6, 2.0, 3.5, 2.3, 4.6, 4.0, 2.5, 5.2, 2.4, 3.7, 2.3, 3.4 };
        static const std::array<double, 12> minorProfile { 6.5, 2.7, 3.5, 5.4, 2.6, 3.5, 2.5, 5.2, 4.0, 2.7, 4.3, 3.2 };
        return minor ? minorProfile : majorProfile;
    }

    /** Pearson correlation of the chroma against a key's (rotated) profile. */
    static double correlate (const std::array<double, 12>& chroma, int key)
    {
        const auto& profile = profileFor (key >= 12);
        const int tonic = key % 12;

        double meanC = 0.0, meanP = 0.0;
        for (size_t i = 0; i < 12; ++i)
        {
            meanC += chroma[i];
            meanP += profile[i];
        }
        meanC /= 12.0;
        meanP /= 12.0;

        double num = 0.0, denC = 0.0, denP = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double c = chroma[(size_t) ((i + tonic) % 12)] - meanC;
            const double p = profile[(size_t) i] - meanP;
            num += c * p;
            denC += c * c;
            denP += p * p;
        }

        return denC > 0.0 && denP > 0.0 ? num / std::sqrt (denC * denP) : 0.0;
    }

    void pushAnalysisSample (float s)
    {
        frameBuffer.push_back (s);
        ++samplesSinceLastFrame;

        if ((int) frameBuffer.size() >= fftSize)
        {
            analyseFrame();
            frameBuffer.erase (frameBuffer.begin(), frameBuffer.begin() + hopSize);
        }
    }

    void analyseFrame()
    {
        analysedSamples += samplesSinceLastFrame;
        samplesSinceLastFrame = 0;

        double sumSquares = 0.0;
        for (size_t i = 0; i < (size_t) fftSize; ++i)
        {
            const auto s = frameBuffer[i];
            sumSquares += (double) s * s;
            fftData[i] = s * window[i];
        }
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        // Silence gate (-70 dBFS RMS): pauses in a live performance or a
        // sample's leading/trailing silence add nothing but noise.
        if (std::sqrt (sumSquares / fftSize) < 3.0e-4)
            return;

        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
        const float* mag = fftData.data();

        constexpr double binHz = analysisRate / fftSize;
        const int kMin = juce::jmax (2, (int) std::floor (minFrequency / binHz));
        const int kMax = juce::jmin (fftSize / 2 - 2, (int) std::ceil (maxFrequency / binHz));

        float frameMax = 0.0f;
        for (int k = kMin; k <= kMax; ++k)
            frameMax = juce::jmax (frameMax, mag[k]);

        if (frameMax <= 0.0f)
            return;

        const float threshold = frameMax * juce::Decibels::decibelsToGain (-30.0f);
        const float prominence = juce::Decibels::decibelsToGain ((float) prominenceDb);
        constexpr int neighbourhood = 20; // bins either side, ~±27 Hz

        peaks.clear();
        for (int k = kMin; k <= kMax; ++k)
        {
            const float m = mag[k];
            if (m < threshold || m <= mag[k - 1] || m < mag[k + 1])
                continue;

            // Must stand clear of its surroundings: broadband noise (hats,
            // snares, risers) is full of tiny local maxima.
            double local = 0.0;
            const int j0 = juce::jmax (1, k - neighbourhood), j1 = juce::jmin (fftSize / 2, k + neighbourhood);
            for (int j = j0; j <= j1; ++j)
                local += mag[j];
            if (m < (float) (local / (j1 - j0 + 1)) * prominence)
                continue;

            // Parabolic interpolation on log magnitude.
            const double a = std::log (mag[k - 1] + 1.0e-12), b = std::log (m + 1.0e-12), c = std::log (mag[k + 1] + 1.0e-12);
            const double denom = a - 2.0 * b + c;
            const double offset = std::abs (denom) > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom) : 0.0;
            const double freq = ((double) k + offset) * binHz;
            if (freq < minFrequency || freq > maxFrequency)
                continue;

            // Each frame normalised to its own loudest peak, so a quiet
            // breakdown counts as much as a loud drop.
            peaks.push_back ({ freq, std::exp (b - 0.25 * (a - c) * offset) / frameMax });
        }

        bool anyPeak = false;
        for (const auto& peak : peaks)
        {
            // Square-root compression so one dominant partial doesn't drown
            // the harmony around it.
            double weight = std::sqrt (peak.mag);

            if (isOvertoneOfLouderPeak (peak))
                weight *= harmonicSuppression;

            if (weight <= 0.0)
                continue;

            const double midi = 69.0 + 12.0 * std::log2 (peak.freq / 440.0);
            addToHistogram (fineHistogram, midi, weight);

            if (peak.freq < bassCutoffHz)
                addToHistogram (bassHistogram, midi, weight);
            else if (peak.freq < midCutoffHz)
                addToHistogram (midHistogram, midi, weight);

            anyPeak = true;
        }

        if (anyPeak)
            ++tonalFrames;
    }


    struct Peak
    {
        double freq, mag;
    };

    /** True if `peak` sits on the 3rd, 5th, 6th or 7th harmonic of a louder
        peak below it — overtones that add a pitch class that wasn't played.
        (Octave harmonics are left alone: they land on the same note.) */
    bool isOvertoneOfLouderPeak (const Peak& peak) const
    {
        for (int h : { 3, 5, 6, 7 })
        {
            const double fundamental = peak.freq / h;
            if (fundamental < minFrequency * 0.97)
                return false;

            for (const auto& other : peaks) // sorted by frequency
            {
                if (other.freq > fundamental * 1.021)
                    break;
                if (other.mag > peak.mag && std::abs (std::log2 (other.freq / fundamental)) < 0.03)
                    return true;
            }
        }

        return false;
    }

    static void addToHistogram (std::array<double, numFineBins>& hist, double midi, double weight)
    {
        // Linear split across the two nearest 10-cent bins.
        double pos = std::fmod (midi * binsPerSemitone, (double) numFineBins);
        if (pos < 0.0)
            pos += numFineBins;

        const int lo = (int) pos % numFineBins;
        const int hi = (lo + 1) % numFineBins;
        const double frac = pos - std::floor (pos);
        hist[(size_t) lo] += weight * (1.0 - frac);
        hist[(size_t) hi] += weight * frac;
    }

    juce::dsp::FFT fft;
    std::vector<float> window, fftData, frameBuffer;
    std::vector<Peak> peaks;
    std::array<juce::dsp::IIR::Filter<float>, 2> lowPass;

    double sourceRate = 44100.0, resampleStep = 4.0, resamplePos = 1.0;
    bool useLowPass = true;
    float previousSample = 0.0f;
    int samplesSinceLastFrame = 0;
    juce::int64 analysedSamples = 0;
    int tonalFrames = 0;

    std::array<double, numFineBins> fineHistogram {};
    std::array<double, numFineBins> bassHistogram {};
    std::array<double, numFineBins> midHistogram {};  // 200 Hz - 1 kHz
};

} // namespace onykey
