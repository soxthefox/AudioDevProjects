include(FetchContent)

# pluginval ships pre-built binaries per OS. We download the right one.
if(WIN32)
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip")
    set(_pluginval_exe   "pluginval.exe")
elseif(APPLE)
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip")
    set(_pluginval_exe   "pluginval.app/Contents/MacOS/pluginval")
else()
    set(_pluginval_url   "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip")
    set(_pluginval_exe   "pluginval")
endif()

FetchContent_Declare(pluginval
    URL "${_pluginval_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(pluginval)

set(PLUGINVAL_EXECUTABLE "${pluginval_SOURCE_DIR}/${_pluginval_exe}" CACHE FILEPATH "")

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
