cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

# SKAZI REVERB by Asher Swissa: mega reverb machine with 10 algorithms, reverse layer and tail filter.
project(SKVERB VERSION 1.1.0)

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

# The codes are the ones SKAZI REVERB shipped with before joining the bundle, so saved projects still load it
juce_add_plugin(Skverb
    COMPANY_NAME             "SKAZI"
    BUNDLE_ID                com.asherswissa.skverb
    PLUGIN_MANUFACTURER_CODE Ashs
    PLUGIN_CODE              Skvb
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "SKAZI REVERB"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Reverb
    AU_MAIN_TYPE             kAudioUnitType_Effect)

set(SKVERB_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Displays.cpp
    Source/Widgets.cpp)

target_sources(Skverb PRIVATE ${SKVERB_SOURCES})

juce_add_binary_data(SkverbAssets SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")

target_compile_definitions(Skverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Skverb
    PRIVATE
        SkverbAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(SKVERB_TESTS "Build the offline test app" OFF)
if(SKVERB_TESTS)
    # Renders every algorithm and preset, stress-tests freeze, shimmer, the tail filter and the reverse layer
    #   SkverbTest <renders dir>             audio tests
    #   SkverbTest shot <out.png> [preset]   editor screenshot after playing drums through it
    juce_add_console_app(SkverbTest PRODUCT_NAME "SkverbTest")
    target_sources(SkverbTest PRIVATE Tests/SkverbTest.cpp ${SKVERB_SOURCES})
    target_compile_definitions(SkverbTest PRIVATE JucePlugin_Name="SKAZI REVERB" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SkverbTest PRIVATE SkverbAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
