# Headless key-detection unit tests. Deliberately does not depend on
# juce_audio_processors or the plugin wrapper: Source/DSP is plain C++/JUCE-DSP,
# so it's exercised directly and fast.

juce_add_console_app(ONYKeyTests
    PRODUCT_NAME "ONYKeyTests"
)

target_sources(ONYKeyTests PRIVATE
    KeyTests.cpp
)

target_include_directories(ONYKeyTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)

target_compile_definitions(ONYKeyTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(ONYKeyTests PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME ONYKeyTests COMMAND ONYKeyTests)

# Offline accuracy tool (not part of ctest): scores the detector against
# folders of samples whose filenames state their key. See KeyEval.cpp.
juce_add_console_app(ONYKeyEval
    PRODUCT_NAME "ONYKeyEval"
)

target_sources(ONYKeyEval PRIVATE
    KeyEval.cpp
)

target_include_directories(ONYKeyEval PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)

target_compile_definitions(ONYKeyEval PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(ONYKeyEval PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Renders the editor to PNG without a host (not part of ctest). Compiles the
# plugin's processor/editor sources directly rather than linking the plugin
# target, so it needs the handful of JucePlugin_* macros they reference.
juce_add_console_app(ONYKeySnapshot
    PRODUCT_NAME "ONYKeySnapshot"
)

target_sources(ONYKeySnapshot PRIVATE
    Snapshot.cpp
    ../Source/PluginProcessor.cpp
    ../Source/PluginEditor.cpp
)

target_include_directories(ONYKeySnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)

target_compile_definitions(ONYKeySnapshot PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JucePlugin_Name="ONY Key"
)

target_link_libraries(ONYKeySnapshot PRIVATE
    ONYKeyBinaryData
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
