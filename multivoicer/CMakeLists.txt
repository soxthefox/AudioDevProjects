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

# juce_add_plugin's FORMATS parameter is read literally at configure time —
# it does NOT expand generator expressions. Build the list with plain if().
set(MULTIVOICER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MULTIVOICER_FORMATS AU)
endif()

juce_add_plugin(Multivoicer
    PRODUCT_NAME "Multivoicer"
    COMPANY_NAME "AudioDevProjects"
    BUNDLE_ID "com.audiodevprojects.multivoicer"
    PLUGIN_MANUFACTURER_CODE Adev
    PLUGIN_CODE Mvcr
    FORMATS ${MULTIVOICER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(Multivoicer PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/Plugin/Parameters.cpp
    Source/Music/MusicTheory.cpp
    Source/Dsp/YinPitchDetector.cpp
    Source/Dsp/SignalsmithPitchShifter.cpp
    Source/Dsp/VoiceEq.cpp
    Source/Voicing/Voice.cpp
    Source/Voicing/VoiceManager.cpp
    Source/Voicing/MidiVoiceAllocator.cpp
    Source/Voicing/ModeRouter.cpp
    Source/Presets/PresetBank.cpp)

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

include(Pluginval)

# Windows + Linux: validate VST3. macOS: validate AU + VST3.
# On Windows, JUCE lays out VST3 as:
#   <ARTEFACT_DIR>/VST3/Multivoicer.vst3/Contents/x86_64-win/Multivoicer.vst3
# TARGET_FILE_DIR resolves to the x86_64-win directory; navigate up 3 levels
# to reach the outer .vst3 bundle folder that pluginval expects.
# On macOS, TARGET_BUNDLE_DIR works correctly for both VST3 and AU.
if(WIN32)
    multivoicer_add_pluginval_test(pluginval_vst3
        "$<TARGET_FILE_DIR:Multivoicer_VST3>/../../..")
elseif(APPLE)
    multivoicer_add_pluginval_test(pluginval_au
        "$<TARGET_BUNDLE_DIR:Multivoicer_AU>")
    multivoicer_add_pluginval_test(pluginval_vst3
        "$<TARGET_BUNDLE_DIR:Multivoicer_VST3>")
endif()
