cmake_minimum_required(VERSION 3.22)
project(Multivoicer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(JUCE)
include(Signalsmith)
enable_testing()
include(Catch2)

juce_add_plugin(Multivoicer
    PRODUCT_NAME "Multivoicer"
    COMPANY_NAME "AudioDevProjects"
    BUNDLE_ID "com.audiodevprojects.multivoicer"
    PLUGIN_MANUFACTURER_CODE Adev
    PLUGIN_CODE Mvcr
    FORMATS VST3 Standalone $<$<PLATFORM_ID:Darwin>:AU>
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(Multivoicer PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Music/MusicTheory.cpp)

target_compile_definitions(Multivoicer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Multivoicer PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    signalsmith::stretch
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_include_directories(Multivoicer PRIVATE Source)

add_subdirectory(Tests)
