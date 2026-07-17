cmake_minimum_required(VERSION 3.22)

project(MixAnalyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull JUCE automatically (pinned to a real release)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MixAnalyzer
    COMPANY_NAME "EERIE"
    PLUGIN_MANUFACTURER_CODE Eeri   # arbitrary 4-char maker tag, change anytime
    PLUGIN_CODE Mxan                # arbitrary 4-char plugin tag, change anytime
    FORMATS Standalone VST3
    PRODUCT_NAME "EERIE - Mix Analyzer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(MixAnalyzer)

target_sources(MixAnalyzer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WaveformDisplay.cpp
    Source/MixLookAndFeel.cpp
)

target_compile_definitions(MixAnalyzer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(MixAnalyzer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
