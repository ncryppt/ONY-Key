cmake_minimum_required(VERSION 3.22)

project(ONYKey VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Universal binary architectures" FORCE)
endif()

# Statically link the MSVC runtime on Windows so the plugin loads without the
# Visual C++ Redistributable (same lesson as ONY Verb's beta: without this the
# DLL fails to load silently and never shows up in the host's plugin list).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE 9.0.2's CoreMIDI UMP-endpoint code relies on class template argument
# deduction from a unique_ptr into std::shared_ptr, which current Apple
# libc++ rejects. Spell the types out (same fix ONY Verb carries) — applied
# here so a fresh clone builds without hand-patching the submodule. A no-op
# once applied, or once JUCE fixes it upstream.
set(_juce_coremidi "${CMAKE_CURRENT_SOURCE_DIR}/modules/JUCE/modules/juce_audio_devices/native/juce_CoreMidi_mac.mm")
if(APPLE AND EXISTS "${_juce_coremidi}")
    file(STRINGS "${_juce_coremidi}" _needs_patch REGEX "std::shared_ptr receiver = ")
    if(_needs_patch)
        # perl -pi edits in place and keeps the file's CRLF line endings.
        execute_process(COMMAND perl -pi -e
                            "s/std::shared_ptr receiver = /std::shared_ptr<ConnectionToSrc> receiver = /; s/std::weak_ptr weak = receiver;/std::weak_ptr<ConnectionToSrc> weak = receiver;/"
                            "${_juce_coremidi}"
                        RESULT_VARIABLE _patch_result)
        if(_patch_result EQUAL 0)
            message(STATUS "Patched JUCE CoreMIDI shared_ptr deduction for current libc++")
        else()
            message(WARNING "Couldn't patch ${_juce_coremidi}; the AU/Standalone build may fail on current Xcode")
        endif()
    endif()
endif()

add_subdirectory(modules/JUCE)

# ---------------------------------------------------------------------------
# Formats: VST3 + Standalone everywhere; AU on macOS. AAX only when the
# (NDA'd, not vendored) AAX SDK path is supplied.
# ---------------------------------------------------------------------------
set(ONYKEY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ONYKEY_FORMATS AU)
endif()

set(AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK, enables the AAX target when set")
if(AAX_SDK_PATH)
    list(APPEND ONYKEY_FORMATS AAX)
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
endif()

# Logo masks, generated from Resources/brand/ONY-Key-logo-source.webp by
# scripts/make_logo_assets.py. The UI paints them in its own materials.
juce_add_binary_data(ONYKeyBinaryData SOURCES
    Resources/ONYKey_LogoMask.png
    Resources/ONYKey_KeyholeMask.png
    Resources/CanadaMapleLeaf.png
)

juce_add_plugin(ONYKey
    COMPANY_NAME "ONYVA"
    PLUGIN_MANUFACTURER_CODE Onyv
    PLUGIN_CODE Onky
    FORMATS ${ONYKEY_FORMATS}
    PRODUCT_NAME "ONY Key"
    BUNDLE_ID "com.onyva.onykey"
    VST3_CATEGORIES "Fx" "Analyzer"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    AAX_CATEGORY "AAX_ePlugInCategory_Effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_AUTO_MANIFEST TRUE
)

target_sources(ONYKey PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Version.h
    Source/FileAnalyser.h
    Source/AnalysisThreads.h
    Source/DSP/KeyDetector.h
    Source/DSP/KeyNames.h
    Source/UI/Theme.h
    Source/UI/OnyvaLookAndFeel.h
    Source/UI/KeyWheel.h
    Source/UI/ChromaBars.h
    Source/UI/Wordmark.h
    Source/UI/Animation.h
    Source/UI/StatCards.h
    Source/UI/IconButton.h
    Source/UI/ThemePicker.h
    Source/UI/ParticleOverlay.h
    Source/UI/Shapes.h
)

target_compile_definitions(ONYKey PUBLIC
    JUCE_ALSA=0
    JUCE_DIRECTSOUND=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_PLUGINHOST_AU=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    # Decoders for dropped files: WAV/AIFF/FLAC/Ogg are always on; MP3 is off
    # by default in JUCE. On macOS CoreAudio also covers MP3/M4A/CAF.
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(ONYKey PRIVATE
    ONYKeyBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(ONYKey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

enable_testing()
add_subdirectory(Tests)
