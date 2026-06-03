add_executable(MultivoicerTests
    smoke_test.cpp
    music_theory_test.cpp
    yin_detector_test.cpp
    ${CMAKE_SOURCE_DIR}/Source/Music/MusicTheory.cpp
    ${CMAKE_SOURCE_DIR}/Source/Dsp/YinPitchDetector.cpp)

target_link_libraries(MultivoicerTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_utils
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
