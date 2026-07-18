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

# Build formats. Standalone + VST3 on every OS; on macOS also build AU (the
# format Logic Pro and GarageBand use). AU only compiles on Apple platforms.
set(PLUGIN_FORMATS Standalone VST3)
if (APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(MixAnalyzer
    COMPANY_NAME "EERIE"
    PLUGIN_MANUFACTURER_CODE Eeri   # arbitrary 4-char maker tag, change anytime
    PLUGIN_CODE Mxan                # arbitrary 4-char plugin tag, change anytime
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "EERIE - Mix Analyzer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    # BUNDLE_ID defaults to com.EERIE.EERIE-Mix-Analyzer; set a real reverse-domain
    # (e.g. BUNDLE_ID "com.eeriegoesd.mixanalyzer") before a macOS release.
)

juce_generate_juce_header(MixAnalyzer)

target_sources(MixAnalyzer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WaveformDisplay.cpp
    Source/MixLookAndFeel.cpp
    Source/SpectrumDisplay.cpp
    Source/SpectrumSettingsPanel.cpp
    Source/HistoryAnalyzer.cpp
    Source/LoudnessDisplay.cpp
    Source/LoudnessSettingsPanel.cpp
    Source/SoundFieldDisplay.cpp
    Source/SoundFieldSettingsPanel.cpp
    Source/PresetStore.cpp
    Source/PresetBar.cpp
    Source/ebur128/ebur128.c
)

target_include_directories(MixAnalyzer PRIVATE
    Source/ebur128
)

target_compile_definitions(MixAnalyzer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1   # use our own app (no feedback-loop banner)
)

# Our custom standalone app lives only in the Standalone product.
if (TARGET MixAnalyzer_Standalone)
    target_sources(MixAnalyzer_Standalone PRIVATE Source/StandaloneApp.cpp)
endif()

target_link_libraries(MixAnalyzer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
