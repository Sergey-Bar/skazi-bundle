cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(SHAKSUKA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local JUCE checkout
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(SHAK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SHAK_FORMATS AU)
endif()

juce_add_plugin(Shaksuka
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Shak
    FORMATS                  ${SHAK_FORMATS}
    PRODUCT_NAME             "SHAKSUKA"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(Shaksuka PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ChopEngine.cpp
    Source/Presets.cpp)

# The SKAZI logo (../Resources/skazi_logo.png), shared with the other plugins;
# without it the editor draws a vector stand-in.
set(SHAK_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
set(SHAK_LOGO_FOUND OFF)
if(EXISTS "${SHAK_LOGO}")
    set(SHAK_LOGO_FOUND ON)
    juce_add_binary_data(ShaksukaData SOURCES "${SHAK_LOGO}")
    target_link_libraries(Shaksuka PRIVATE ShaksukaData)
    target_compile_definitions(Shaksuka PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(Shaksuka PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(SHAK_TESTS "Build the engine test" OFF)
if(SHAK_TESTS)
    juce_add_console_app(ShaksukaEngineTest PRODUCT_NAME "ShaksukaEngineTest")
    target_sources(ShaksukaEngineTest PRIVATE Tests/EngineTest.cpp Source/ChopEngine.cpp Source/Presets.cpp)
    target_compile_definitions(ShaksukaEngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ShaksukaEngineTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()

# Windows installer: run Installer/build_installer.ps1, which builds the plugin,
# stages it in Installer/payload and then builds "SHAKSUKA Setup.exe" around it.
option(SHAK_INSTALLER "Build the Windows installer" OFF)
if(SHAK_INSTALLER AND WIN32)
    set(SHAK_PAYLOAD "${CMAKE_CURRENT_SOURCE_DIR}/Installer/payload")
    foreach(f SHAKSUKA.vst3 moduleinfo.json SHAKSUKA.exe)
        if(NOT EXISTS "${SHAK_PAYLOAD}/${f}")
            message(FATAL_ERROR "Installer/payload/${f} is missing: run Installer/build_installer.ps1")
        endif()
    endforeach()
    set(SHAK_SETUP_DATA "${SHAK_PAYLOAD}/SHAKSUKA.vst3" "${SHAK_PAYLOAD}/moduleinfo.json" "${SHAK_PAYLOAD}/SHAKSUKA.exe")
    if(EXISTS "${SHAK_LOGO}")
        list(APPEND SHAK_SETUP_DATA "${SHAK_LOGO}")
    endif()

    juce_add_gui_app(ShaksukaSetup
        PRODUCT_NAME "SHAKSUKA Setup"
        COMPANY_NAME "SKAZI"
        VERSION      1.0.0)
    target_sources(ShaksukaSetup PRIVATE Installer/SetupMain.cpp)
    juce_add_binary_data(ShaksukaSetupData SOURCES ${SHAK_SETUP_DATA})
    target_compile_definitions(ShaksukaSetup PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        $<$<BOOL:${SHAK_LOGO_FOUND}>:SHAK_HAS_LOGO=1>)
    target_link_libraries(ShaksukaSetup PRIVATE ShaksukaSetupData juce::juce_gui_extra
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_link_options(ShaksukaSetup PRIVATE "/MANIFESTUAC:level='requireAdministrator'")
endif()

target_link_libraries(Shaksuka
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
