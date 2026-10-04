// Offline accuracy check (not part of ctest): runs the detector over folders
// of samples whose filenames state their key ("..._Gmin.wav", "Loop 124 F#m.wav")
// and reports how often it agrees, MIREX-style:
//   exact = 1, fifth above/below = 0.5, relative major/minor = 0.3, parallel = 0.2
//
//   ONYKeyEval [--verbose] [--dump] [--harm 0] [--prom 6] [--other 0.22] <folder or file>...
//
// Prints major- and minor-labelled accuracy separately: sample packs are
// overwhelmingly minor, so overall accuracy alone would reward a detector that
// just leans minor.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "FileAnalyser.h"
#include "DSP/KeyNames.h"
#include <map>
#include <regex>

namespace
{
using namespace onykey;

/** Returns 0-23, or -1 if the filename doesn't clearly state a key. */
int keyFromFilename (const juce::String& name)
{
    static const std::regex token (R"(^([A-G])(#|b|s|sharp|flat)?(maj|major|min|minor|m)$)", std::regex::icase);
    static const juce::String letters ("C D EF G A B");

    int found = -1;
    juce::StringArray tokens;
    tokens.addTokens (name.upToLastOccurrenceOf (".", false, false), " _-.()[]", "");

    for (auto& t : tokens)
    {
        std::smatch m;
        const auto s = t.toStdString();
        if (! std::regex_match (s, m, token))
            continue;

        // The tonic letter must be upper case ("am", "em" are usually words).
        if (! std::isupper ((unsigned char) s[0]))
            continue;

        int pc = letters.indexOfChar ((juce::juce_wchar) s[0]);
        const auto acc = juce::String (m[2].str()).toLowerCase();
        if (acc == "#" || acc == "s" || acc == "sharp") ++pc;
        if (acc == "b" || acc == "flat") --pc;
        pc = (pc + 12) % 12;

        const auto quality = juce::String (m[3].str());
        const bool minor = quality == "m" || quality.startsWithIgnoreCase ("min");
        found = minor ? 12 + pc : pc;
    }

    return found;
}

double mirexScore (int truth, int guess)
{
    if (guess < 0) return 0.0;
    if (truth == guess) return 1.0;
    const bool sameMode = isMinorKey (truth) == isMinorKey (guess);
    const int diff = (tonicOf (guess) - tonicOf (truth) + 12) % 12;
    if (sameMode && (diff == 7 || diff == 5)) return 0.5;
    if (relativeKeyOf (truth) == guess) return 0.3;
    if (! sameMode && tonicOf (truth) == tonicOf (guess)) return 0.2;
    return 0.0;
}

juce::String percent (int count, int total)
{
    return total > 0 ? juce::String (100.0 * count / total, 1) + "%" : juce::String ("-");
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init; // CoreAudio's reader wants a message manager on macOS
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    bool verbose = false, dump = false;
    std::map<juce::String, double> overrides;
    juce::Array<juce::File> files;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--verbose") { verbose = true; continue; }
        if (arg == "--dump") { dump = true; continue; }
        if (arg.startsWith ("--") && i + 1 < argc) { overrides[arg.substring (2)] = juce::String (argv[++i]).getDoubleValue(); continue; }

        const auto f = juce::File::getCurrentWorkingDirectory().getChildFile (arg);
        if (f.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (f, true, "*", juce::File::findFiles))
                files.add (entry.getFile());
        else
            files.add (f);
    }

    auto configure = [&overrides] (KeyDetector& d)
    {
        for (const auto& [name, value] : overrides)
        {
            if      (name == "harm")  d.harmonicSuppression = value;
            else if (name == "prom")  d.prominenceDb = value;
            else if (name == "other") d.rootNoteOtherThreshold = (float) value;
            else std::cerr << "Unknown option --" << name << "\n";
        }
    };

    int n = 0, exact = 0, rootNotes = 0;
    double score = 0.0;
    std::array<int, 2> modeCount {}, modeExact {};
    std::array<int, 5> confCount {}, confExact {};

    for (const auto& file : files)
    {
        if (! isSupportedAudioFile (file))
            continue;

        const int truth = keyFromFilename (file.getFileName());
        if (truth < 0)
            continue;

        const auto result = analyseAudioFile (file, formats, {}, {}, configure);
        if (! result)
            continue;

        const auto& r = *result;
        const double s = mirexScore (truth, r.key);
        const bool hit = s == 1.0;

        ++n;
        score += s;
        exact += hit ? 1 : 0;
        rootNotes += r.kind == KeyResult::Kind::RootNote ? 1 : 0;
        ++modeCount[isMinorKey (truth) ? 1 : 0];
        modeExact[isMinorKey (truth) ? 1 : 0] += hit ? 1 : 0;

        const auto bucket = (size_t) juce::jlimit (0, 4, (int) (r.confidence * 5.0f));
        ++confCount[bucket];
        confExact[bucket] += hit ? 1 : 0;

        if (dump)
        {
            std::cout << file.getFileName() << "  truth " << keyName (truth) << "  got " << keyName (r.key)
                      << "  tuning " << juce::String (r.tuningCents, 1) << "\n   chroma:";
            for (int pc = 0; pc < 12; ++pc)
                std::cout << " " << noteName (pc) << "=" << juce::String (r.chroma[(size_t) pc], 2);
            std::cout << "\n";
        }

        if (verbose && ! hit)
            std::cout << (s > 0.0 ? "  near  " : "  MISS  ") << keyName (truth) << " -> " << keyName (r.key)
                      << (r.kind == KeyResult::Kind::RootNote ? " [root]" : "")
                      << "  conf " << juce::String (r.confidence, 2) << "  " << file.getFileName() << "\n";
    }

    if (n == 0)
    {
        std::cout << "No key-labelled audio files found.\n";
        return 1;
    }

    std::cout << "\nFiles: " << n
              << "\nExact: " << percent (exact, n)
              << "\nMIREX: " << juce::String (100.0 * score / n, 1) << "%"
              << "\nMajor-labelled: " << modeCount[0] << " files, " << percent (modeExact[0], modeCount[0]) << " exact"
              << "\nMinor-labelled: " << modeCount[1] << " files, " << percent (modeExact[1], modeCount[1]) << " exact"
              << "\nRoot-note results: " << rootNotes
              << "\n\nConfidence calibration (exact-match rate per bucket):\n";

    for (size_t b = 0; b < 5; ++b)
        std::cout << "  " << (int) b * 20 << "-" << (int) b * 20 + 20 << "%: "
                  << confCount[b] << " files, " << percent (confExact[b], confCount[b]) << " exact\n";

    return 0;
}
