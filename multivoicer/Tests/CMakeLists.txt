add_executable(MultivoicerTests
    smoke_test.cpp
    music_theory_test.cpp
    yin_detector_test.cpp
    pitch_shifter_test.cpp
    voice_eq_test.cpp
    voice_test.cpp
    voice_allocator_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Music/MusicTheory.cpp
    ${CMAKE_SOURCE_DIR}/Source/Dsp/YinPitchDetector.cpp
    ${CMAKE_SOURCE_DIR}/Source/Dsp/SignalsmithPitchShifter.cpp
    ${CMAKE_SOURCE_DIR}/Source/Dsp/VoiceEq.cpp
    ${CMAKE_SOURCE_DIR}/Source/Dsp/Envelope.cpp
    ${CMAKE_SOURCE_DIR}/Source/Voicing/Voice.cpp
    ${CMAKE_SOURCE_DIR}/Source/Voicing/VoiceManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/Voicing/MidiVoiceAllocator.cpp
    mode_router_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Voicing/ModeRouter.cpp
    preset_bank_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Presets/PresetBank.cpp
    audio_io.cpp
    integration_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/Parameters.cpp)

target_link_libraries(MultivoicerTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    signalsmith::stretch)

target_compile_definitions(MultivoicerTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_include_directories(MultivoicerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source)

# Make tests depend on the plugin compiling — Layer 0 build gate.
add_dependencies(MultivoicerTests Multivoicer)

catch_discover_tests(MultivoicerTests)
