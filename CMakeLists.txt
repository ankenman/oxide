cmake_minimum_required(VERSION 3.22)
project(Oxide VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

# ---------------------------------------------------------------------------
# JUCE: use a local checkout at ./JUCE or JUCE_DIR if given, otherwise fetch.
# ---------------------------------------------------------------------------
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Formats. AU only builds on macOS; Standalone is handy for quick testing.
# ---------------------------------------------------------------------------
set(OXIDE_FORMATS Standalone)
if(APPLE)
    list(APPEND OXIDE_FORMATS AU)
endif()
option(OXIDE_BUILD_VST3 "Also build a VST3" OFF)
if(OXIDE_BUILD_VST3)
    list(APPEND OXIDE_FORMATS VST3)
endif()

juce_add_plugin(Oxide
    PRODUCT_NAME                "Oxide"
    COMPANY_NAME                "Ankenman Audio"
    BUNDLE_ID                   "com.ankenman.oxide"
    PLUGIN_MANUFACTURER_CODE    Ankm
    PLUGIN_CODE                 Oxid
    FORMATS                     ${OXIDE_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE          # installs to ~/Library/Audio/Plug-Ins
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Distortion
    DESCRIPTION                 "Tape saturator")

target_sources(Oxide PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Oxide PRIVATE Source)

target_compile_definitions(Oxide PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Oxide
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
