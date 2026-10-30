cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

# SKWIDE by Asher Swissa: phase-safe stereo imager with allpass spread, phase rotation, depth and Haas SUPER WIDE.
project(SKWIDE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local JUCE checkout
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(FORMATS_LIST VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS_LIST AU)
endif()

# The codes are the ones SKWIDE shipped with before joining the bundle, so saved projects still load it
juce_add_plugin(Skwide
    COMPANY_NAME             "SKAZI"
    BUNDLE_ID                com.asherswissa.skwide
    PLUGIN_MANUFACTURER_CODE Ashs
    PLUGIN_CODE              Skwd
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "SKWIDE"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Spatial
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(Skwide PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Displays.cpp
    Source/Widgets.cpp)

juce_add_binary_data(SkwideAssets SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")

target_compile_definitions(Skwide PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(SKWIDE_TESTS "Build the offline test host" OFF)
if(SKWIDE_TESTS)
    # Loads the built VST3 like a DAW, proves the mono sum is untouched and renders demo WAVs
    juce_add_console_app(SkwideTest PRODUCT_NAME "SkwideTest")
    target_sources(SkwideTest PRIVATE Tests/SkwideTest.cpp)
    target_compile_definitions(SkwideTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkwideTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

target_link_libraries(Skwide
    PRIVATE
        SkwideAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
