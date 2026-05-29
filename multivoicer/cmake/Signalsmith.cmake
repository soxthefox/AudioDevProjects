include(FetchContent)

# Signalsmith Stretch — BSD-3, header-only-ish C++17 pitch/time stretcher.
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(signalsmith_stretch)

# Signalsmith doesn't ship its own CMake target; wrap it.
add_library(signalsmith_stretch INTERFACE)
target_include_directories(signalsmith_stretch INTERFACE ${signalsmith_stretch_SOURCE_DIR})
add_library(signalsmith::stretch ALIAS signalsmith_stretch)
