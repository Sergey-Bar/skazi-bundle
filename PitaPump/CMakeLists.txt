cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(PITAPUMP VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local JUCE checkout
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(PUMP_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PUMP_FORMATS AU)
endif()

juce_add_plugin(PitaPump
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Pita
    FORMATS                  ${PUMP_FORMATS}
    PRODUCT_NAME             "PITA PUMP"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Dynamics
    AU_MAIN_TYPE             kAudioUnitType_MusicEffect)

target_sources(PitaPump PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PumpEngine.cpp
    Source/Presets.cpp)

# The SKAZI logo (../Resources/skazi_logo.png), shared with the other plugins
set(PUMP_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${PUMP_LOGO}")
    juce_add_binary_data(PitaPumpData SOURCES "${PUMP_LOGO}")
    target_link_libraries(PitaPump PRIVATE PitaPumpData)
    target_compile_definitions(PitaPump PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(PitaPump PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(PUMP_TESTS "Build the engine test" OFF)
if(PUMP_TESTS)
    juce_add_console_app(PitaPumpTest PRODUCT_NAME "PitaPumpTest")
    target_sources(PitaPumpTest PRIVATE Tests/PumpTest.cpp Source/PumpEngine.cpp Source/Presets.cpp)
    target_compile_definitions(PitaPumpTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PitaPumpTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()

target_link_libraries(PitaPump
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
