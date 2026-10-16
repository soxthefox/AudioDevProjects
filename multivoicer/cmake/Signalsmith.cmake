include(FetchContent)

# Signalsmith Stretch — BSD-3, header-only-ish C++17 pitch/time stretcher.
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(signalsmith_stretch)

# signalsmith-stretch's own CMakeLists creates the `signalsmith-stretch` target
# (include subdir + signalsmith-linear dep).  Alias it to the name used in this project.
add_library(signalsmith::stretch ALIAS signalsmith-stretch)
