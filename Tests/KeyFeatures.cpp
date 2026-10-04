// Feature dump for training/evaluating the key model (not part of ctest).
//
//   ONYKeyFeatures <manifest.csv> <out.csv> [--harm 0] [--prom 6] ...
//
// Reads a manifest from scripts/build_eval_manifest.py (path,label,group,kind),
// runs every file through the plugin's own analyseAudioFile(), and writes one
// row per file: the manifest columns, the current detector's answer, then the
// raw evidence (10-cent pitch-class histograms: all, bass, mid; 120 each).
// scripts/train_key_model.py learns the key model from this.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "FileAnalyser.h"
#include "DSP/KeyNames.h"
#include <map>

namespace
{
using namespace onykey;

juce::StringArray parseCsvLine (const juce::String& line)
{
    juce::StringArray out;
    juce::String field;
    bool quoted = false;
    for (int i = 0; i < line.length(); ++i)
    {
        const auto c = line[i];
        if (quoted)
        {
            if (c == '"' && i + 1 < line.length() && line[i + 1] == '"') { field += '"'; ++i; }
            else if (c == '"') quoted = false;
            else field += c;
        }
        else if (c == '"') quoted = true;
        else if (c == ',') { out.add (field); field = {}; }
        else field += c;
    }
    out.add (field);
    return out;
}

juce::String csvQuote (const juce::String& s)
{
    return "\"" + s.replace ("\"", "\"\"") + "\"";
}

struct Row
{
    juce::String path, label, group, kind, output;
};
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc < 3)
    {
        std::cerr << "usage: ONYKeyFeatures <manifest.csv> <out.csv> [--option value]...\n";
        return 1;
    }

    std::map<juce::String, double> overrides;
    for (int i = 3; i + 1 < argc; i += 2)
        overrides[juce::String (argv[i]).trimCharactersAtStart ("-")] = juce::String (argv[i + 1]).getDoubleValue();

    auto configure = [overrides] (KeyDetector& d)
    {
        for (const auto& [name, value] : overrides)
        {
            if      (name == "harm") d.harmonicSuppression = value;
            else if (name == "prom") d.prominenceDb = value;
        }
    };

    juce::StringArray lines;
    lines.addLines (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]).loadFileAsString());

    std::vector<Row> rows;
    for (int i = 1; i < lines.size(); ++i)
    {
        const auto f = parseCsvLine (lines[i]);
        if (f.size() >= 4)
            rows.push_back ({ f[0], f[1], f[2], f[3], {} });
    }

    // Parallel: each worker has its own format manager and detector.
    juce::ThreadPool pool (juce::jmax (2, juce::SystemStats::getNumCpus() - 1));
    std::atomic<int> done { 0 };

    for (auto& row : rows)
    {
        pool.addJob ([&row, &configure, &done, total = (int) rows.size()]
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();

            KeyDetector::Evidence evidence;
            const auto result = analyseAudioFile (juce::File (row.path), formats, {}, {}, configure, &evidence);
            if (result)
            {
                juce::StringArray cols { csvQuote (row.path), csvQuote (row.label), csvQuote (row.group), row.kind,
                                         juce::String (result->key), juce::String ((int) result->kind), juce::String (result->rootNote),
                                         juce::String (result->confidence, 4), juce::String (evidence.tonalFrames) };
                for (const auto* hist : { &evidence.all, &evidence.bass, &evidence.mid })
                    for (auto v : *hist)
                        cols.add (juce::String (v, 5));
                row.output = cols.joinIntoString (",");
            }

            const int n = ++done;
            if (n % 500 == 0)
                std::cerr << n << " / " << total << "\n";
        });
    }

    while (pool.getNumJobs() > 0)
        juce::Thread::sleep (100);

    juce::StringArray header { "path", "label", "group", "kind", "pred_key", "pred_kind", "pred_root", "pred_conf", "tonal_frames" };
    for (const char* band : { "all", "bass", "mid" })
        for (int b = 0; b < KeyDetector::numFineBins; ++b)
            header.add (juce::String (band) + juce::String (b));

    juce::StringArray out { header.joinIntoString (",") };
    int written = 0;
    for (const auto& row : rows)
        if (row.output.isNotEmpty())
        {
            out.add (row.output);
            ++written;
        }

    juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]).replaceWithText (out.joinIntoString ("\n") + "\n");
    std::cerr << "wrote " << written << " of " << rows.size() << " rows\n";
    return 0;
}
