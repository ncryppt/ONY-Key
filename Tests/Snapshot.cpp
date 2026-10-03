// Renders the plugin editor to PNGs without a host (not part of ctest):
//   ONYKeySnapshot <out-dir> [--theme <name>] [audio files...] [--listen <audio file>] [--drop <audio file>]
// One image for the empty state, then one per file after it's analysed.
// --listen plays a file through processBlock() with LISTEN on, as a host
// would, and snapshots the live result (exercises the real-time path).
// --drop shows the drag-and-drop overlay as if that file were being dragged in.
// --watch analyses a file with the editor open, snapshotting just after the
// result lands (catches the result's particle burst mid-flight).
// --idle <seconds> leaves the editor open that long before snapshotting.
// --delay <ms> sets how long after the result --watch snapshots (default 330).
// --play <file> <seconds> plays the file through processBlock in real time
// with the editor open and LISTEN on, then snapshots (live particles).
// --theme renders with a theme without touching the saved preference (must
// come before the files); image names get the theme as a prefix.
// Handy for checking UI changes and for README screenshots.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Theme.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc < 2)
    {
        std::cout << "usage: ONYKeySnapshot <out-dir> [audio files...]\n";
        return 1;
    }

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    outDir.createDirectory();

    auto save = [&] (ONYKeyAudioProcessor& processor, const juce::String& name)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        // Let the editor's timer run so animations settle.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = outDir.getChildFile (name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "wrote " << file.getFullPathName() << "\n";
    };

    juce::String prefix;
    int firstArg = 2;
    if (argc > 3 && juce::String (argv[2]) == "--theme")
    {
        onykey::ui::Theme::sessionThemeOverride() = argv[3];
        prefix = juce::String (argv[3]).replaceCharacters (" ?", "_-") + "-";
        firstArg = 4;
    }

    const auto baseSave = save;
    auto saveNamed = [&] (ONYKeyAudioProcessor& p, const juce::String& name) { baseSave (p, prefix + name); };

    int watchDelayMs = 330;
    ONYKeyAudioProcessor processor;
    saveNamed (processor, "00-empty");

    for (int i = firstArg; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--delay" && i + 1 < argc)
        {
            watchDelayMs = juce::String (argv[++i]).getIntValue();
            continue;
        }

        if (juce::String (argv[i]) == "--play" && i + 2 < argc)
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]);
            const double seconds = juce::String (argv[++i]).getDoubleValue();
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr)
                continue;

            constexpr int blockSize = 512;
            processor.setPlayConfigDetails (2, 2, reader->sampleRate, blockSize);
            processor.prepareToPlay (reader->sampleRate, blockSize);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            processor.setListening (true);

            juce::AudioBuffer<float> block (2, blockSize);
            juce::MidiBuffer midi;
            const auto start = juce::Time::getMillisecondCounterHiRes();
            juce::int64 pos = 0;
            while (juce::Time::getMillisecondCounterHiRes() - start < seconds * 1000.0)
            {
                // Keep pace with real time so the UI sees a realistic level.
                const auto due = (juce::int64) ((juce::Time::getMillisecondCounterHiRes() - start) * 0.001 * reader->sampleRate);
                while (pos < due)
                {
                    block.clear();
                    reader->read (&block, 0, blockSize, pos % juce::jmax ((juce::int64) 1, reader->lengthInSamples), true, true);
                    processor.processBlock (block, midi);
                    pos += blockSize;
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            }

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto out = outDir.getChildFile (prefix + "play-" + file.getFileNameWithoutExtension() + ".png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << "wrote " << out.getFullPathName() << "\n";
            processor.setListening (false);
            continue;
        }

        if (juce::String (argv[i]) == "--idle" && i + 1 < argc)
        {
            // Leave the editor open for a while (slow theme effects: Acid
            // Trip's spiral fade-in, Kush Koma's building haze and smoke).
            const int seconds = juce::String (argv[++i]).getIntValue();
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (seconds * 1000);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto out = outDir.getChildFile (prefix + "idle-" + juce::String (seconds) + "s.png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << "wrote " << out.getFullPathName() << "\n";
            continue;
        }

        if (juce::String (argv[i]) == "--watch" && i + 1 < argc)
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            processor.analyseFile (file);
            for (int waited = 0; processor.getDisplay().analysing && waited < 30000; waited += 20)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (watchDelayMs);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto out = outDir.getChildFile (prefix + "watch-" + file.getFileNameWithoutExtension() + ".png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << "wrote " << out.getFullPathName() << "\n";
            continue;
        }

        if (juce::String (argv[i]) == "--drop" && i + 1 < argc)
        {
            const juce::String path (argv[++i]);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            if (auto* target = dynamic_cast<juce::FileDragAndDropTarget*> (editor->getChildComponent (0)))
                target->fileDragEnter ({ path }, 100, 100);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (600);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto file = outDir.getChildFile (prefix + "drop-overlay.png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (image, out);
            std::cout << "wrote " << file.getFullPathName() << "\n";
            continue;
        }

        if (juce::String (argv[i]) == "--listen" && i + 1 < argc)
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]);
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr)
                continue;

            constexpr int blockSize = 512;
            processor.setPlayConfigDetails (2, 2, reader->sampleRate, blockSize);
            processor.prepareToPlay (reader->sampleRate, blockSize);
            processor.setListening (true);
            juce::Thread::sleep (100); // let the analyser thread pick up the reset

            juce::AudioBuffer<float> block (2, blockSize);
            juce::MidiBuffer midi;
            for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += blockSize)
            {
                block.clear();
                reader->read (&block, 0, blockSize, pos, true, true);
                const auto before = block.getRMSLevel (0, 0, blockSize);
                processor.processBlock (block, midi);
                jassertquiet (juce::exactlyEqual (block.getRMSLevel (0, 0, blockSize), before)); // pass-through untouched
                juce::Thread::sleep (1); // ~10x real time; the FIFO drains every 40 ms
            }

            juce::Thread::sleep (600); // final publish
            const auto d = processor.getDisplay();
            std::cout << "LISTEN " << file.getFileName() << " -> " << onykey::keyName (d.result.key)
                      << " after " << d.result.secondsAnalysed << " s\n";
            saveNamed (processor, "listen-" + file.getFileNameWithoutExtension());
            processor.setListening (false);
            continue;
        }

        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i]);
        processor.analyseFile (file);

        for (int waited = 0; processor.getDisplay().analysing && waited < 30000; waited += 50)
            juce::Thread::sleep (50);

        const auto d = processor.getDisplay();
        std::cout << file.getFileName() << " -> " << onykey::keyName (d.result.key) << "\n";
        saveNamed (processor, juce::String (i - firstArg + 1).paddedLeft ('0', 2) + "-" + file.getFileNameWithoutExtension());
    }

    return 0;
}
