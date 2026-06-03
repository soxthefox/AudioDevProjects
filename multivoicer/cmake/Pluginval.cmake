include(FetchContent)

# pluginval ships pre-built binaries per OS. We download the right one.
if(WIN32)
    set(_pluginval_url      "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(_pluginval_glob     "pluginval.exe")
elseif(APPLE)
    set(_pluginval_url      "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    # Recent pluginval releases ship the binary at pluginval.app/Contents/MacOS/Release/pluginval,
    # older ones at pluginval.app/Contents/MacOS/pluginval. Glob to handle both.
    set(_pluginval_glob     "pluginval.app/Contents/MacOS/*pluginval*")
else()
    set(_pluginval_url      "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    set(_pluginval_glob     "pluginval")
endif()

FetchContent_Declare(pluginval
    URL "${_pluginval_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(pluginval)

# Find the actual binary (location varies between releases on macOS).
file(GLOB_RECURSE _pluginval_candidates "${pluginval_SOURCE_DIR}/${_pluginval_glob}")
list(FILTER _pluginval_candidates EXCLUDE REGEX "\\.dSYM($|/)")
list(LENGTH _pluginval_candidates _pluginval_count)
if(_pluginval_count EQUAL 0)
    message(FATAL_ERROR "Pluginval binary not found under ${pluginval_SOURCE_DIR} (glob: ${_pluginval_glob})")
endif()
list(GET _pluginval_candidates 0 _pluginval_resolved)

set(PLUGINVAL_EXECUTABLE "${_pluginval_resolved}" CACHE FILEPATH "" FORCE)
message(STATUS "Multivoicer: pluginval at ${PLUGINVAL_EXECUTABLE}")

# Zips extracted by FetchContent strip the unix execute bit.
# Without this, ctest reports "Not Run" on macOS/Linux.
if(NOT WIN32 AND EXISTS "${PLUGINVAL_EXECUTABLE}")
    execute_process(COMMAND chmod +x "${PLUGINVAL_EXECUTABLE}")
endif()

function(multivoicer_add_pluginval_test target_name plugin_path)
    add_test(
        NAME ${target_name}
        COMMAND "${PLUGINVAL_EXECUTABLE}"
            --strictness-level 5
            --validate-in-process
            --skip-gui-tests
            --validate "${plugin_path}")
    set_tests_properties(${target_name} PROPERTIES TIMEOUT 300)
endfunction()
