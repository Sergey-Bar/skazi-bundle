# TBON 303 by Asher Swissa: acid bass line synth (VST3 + AU + Standalone).
# Shows up in DAWs as "SKAZI 303".
#
#   cmake -B build -G "Visual Studio 17 2022" -A x64      (Windows)
#   cmake -B build -G Xcode                               (macOS)
#   cmake --build build --config Release
#
# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE (or SKAZI303_JUCE_DIR) to use a local JUCE checkout.
cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

project(SKAZI303 VERSION 1.0.0 LANGUAGES C CXX)

option(SKAZI303_COPY_AFTER_BUILD "Install the plugins into the system plug-in folders after building" OFF)
option(SKAZI303_BUILD_TESTS "Build the offline render test and the VST3 host test" OFF)
set(SKAZI303_JUCE_DIR "" CACHE PATH "Local JUCE checkout (optional)")

# Prefer the copy inside the project (external/JUCE) when there is one.
if(NOT SKAZI303_JUCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    set(SKAZI303_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
endif()

if(SKAZI303_JUCE_DIR)
    add_subdirectory("${SKAZI303_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Skazi303
    PRODUCT_NAME "SKAZI 303"
    COMPANY_NAME "SKAZI"
    BUNDLE_ID com.skazi.skazi303
    PLUGIN_MANUFACTURER_CODE Skzi
    PLUGIN_CODE Sk33
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SKAZI303_COPY_AFTER_BUILD}
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth)

target_sources(Skazi303 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StepGrid.cpp)

target_include_directories(Skazi303 PRIVATE Source)
target_compile_features(Skazi303 PRIVATE cxx_std_17)

target_compile_definitions(Skazi303 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Skazi303
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# JUCE-free DSP core rendered offline to a WAV, with sanity checks.
if(SKAZI303_BUILD_TESTS)
    add_executable(Skazi303RenderTest tests/RenderTest.cpp)
    target_include_directories(Skazi303RenderTest PRIVATE Source)
    target_compile_features(Skazi303RenderTest PRIVATE cxx_std_17)

    # Loads the built VST3 the way a DAW does and checks it makes sound.
    juce_add_console_app(Skazi303HostTest PRODUCT_NAME "Skazi303HostTest")
    target_sources(Skazi303HostTest PRIVATE tests/HostTest.cpp)
    target_compile_definitions(Skazi303HostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(Skazi303HostTest PRIVATE
        juce::juce_audio_processors juce::juce_recommended_config_flags)

    # Editor snapshots for checking the panel
    juce_add_console_app(Skazi303Shot PRODUCT_NAME "Skazi303Shot")
    target_sources(Skazi303Shot PRIVATE tests/Shot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/StepGrid.cpp)
    target_include_directories(Skazi303Shot PRIVATE Source)
    target_compile_definitions(Skazi303Shot PRIVATE JucePlugin_Name="SKAZI 303" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(Skazi303Shot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
