cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

# SKILTER by Asher Swissa: morphing multimode filter with LFO, envelope follower, step sequencer and drive.
project(SKILTER VERSION 3.0.0)

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

# The codes are the ones SKILTER shipped with before joining the bundle, so saved projects still load it
juce_add_plugin(Skilter
    COMPANY_NAME             "SKAZI"
    BUNDLE_ID                com.asherswissa.lowpass
    PLUGIN_MANUFACTURER_CODE Ashs
    PLUGIN_CODE              Lpf1
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "SKILTER"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Filter
    AU_MAIN_TYPE             kAudioUnitType_Effect)

target_sources(Skilter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Displays.cpp
    Source/Widgets.cpp
    Source/Presets.cpp)

juce_add_binary_data(SkilterAssets SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")

target_compile_definitions(Skilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(SKILTER_TESTS "Build the offline test host" OFF)
if(SKILTER_TESTS)
    # Loads the built VST3 like a DAW, stress-tests it and renders demo WAVs
    juce_add_console_app(SkilterTest PRODUCT_NAME "SkilterTest")
    target_sources(SkilterTest PRIVATE Tests/SkilterTest.cpp)
    target_compile_definitions(SkilterTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SkilterTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

target_link_libraries(Skilter
    PRIVATE
        SkilterAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
