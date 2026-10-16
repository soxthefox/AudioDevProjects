include(FetchContent)

set(JUCE_VERSION 8.0.4)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${JUCE_VERSION}
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)
