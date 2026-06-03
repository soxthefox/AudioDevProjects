include(FetchContent)

# pluginval ships pre-built binaries per OS. We download the right one.
if(WIN32)
    set(_pluginval_url      "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(_pluginval_filename "pluginval.exe")
else()
    if(APPLE)
        set(_pluginval_url  "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    else()
        set(_pluginval_url  "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    endif()
    set(_pluginval_filename "pluginval")
endif()

FetchContent_Declare(pluginval
    URL "${_pluginval_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(pluginval)

# Recursively find the binary anywhere under the extracted tree.
# Recent pluginval macOS releases place it at
# pluginval.app/Contents/MacOS/Release/pluginval; older releases at
# pluginval.app/Contents/MacOS/pluginval. file(GLOB_RECURSE) with a
# pure filename pattern handles both.
file(GLOB_RECURSE _pluginval_candidates "${pluginval_SOURCE_DIR}/${_pluginval_filename}")
list(FILTER _pluginval_candidates EXCLUDE REGEX "\\.dSYM($|/)")
# Prefer Release/pluginval over a debug build if both exist.
list(SORT _pluginval_candidates)
list(REVERSE _pluginval_candidates)
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
