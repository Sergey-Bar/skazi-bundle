cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(MIDIGOD VERSION 1.1.0)

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

# An instrument (so every DAW loads it on a track) that also sends its loop out as MIDI
juce_add_plugin(MidiGod
    COMPANY_NAME             "SKAZI"
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE              Mgod
    FORMATS                  ${FORMATS_LIST}
    PRODUCT_NAME             "MIDI GOD"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Instrument Generator
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice)

set(MIDIGOD_ENGINE
    Source/Pattern.cpp
    Source/AudioToMidi.cpp
    Source/Presets.cpp)

target_sources(MidiGod PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${MIDIGOD_ENGINE})

# Save the SKAZI logo as ../Resources/skazi_logo.png to use it instead of the drawn stand-in
set(LOGO "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/skazi_logo.png")
if(EXISTS "${LOGO}")
    juce_add_binary_data(MidiGodData SOURCES "${LOGO}")
    target_link_libraries(MidiGod PRIVATE MidiGodData)
    target_compile_definitions(MidiGod PRIVATE SHAK_HAS_LOGO=1)
endif()

target_compile_definitions(MidiGod PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(MIDIGOD_TESTS "Build the engine test" OFF)
if(MIDIGOD_TESTS)
    juce_add_console_app(MidiGodTest PRODUCT_NAME "MidiGodTest")
    target_sources(MidiGodTest PRIVATE Tests/MidiGodTest.cpp ${MIDIGOD_ENGINE})
    target_compile_definitions(MidiGodTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
    target_link_libraries(MidiGodTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)

    # Editor snapshot for checking the skin
    juce_add_console_app(MidiGodShot PRODUCT_NAME "MidiGodShot")
    target_sources(MidiGodShot PRIVATE Tests/MidiGodShot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp ${MIDIGOD_ENGINE})
    target_compile_definitions(MidiGodShot PRIVATE JucePlugin_Name="MIDI GOD" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(TARGET MidiGodData)
        target_link_libraries(MidiGodShot PRIVATE MidiGodData)
        target_compile_definitions(MidiGodShot PRIVATE SHAK_HAS_LOGO=1)
    endif()
    target_link_libraries(MidiGodShot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

target_link_libraries(MidiGod
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
