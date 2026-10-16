cmake_minimum_required(VERSION 3.24)
project(simple_multi_analyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/3fb5cbc0340758596dac39f7c8de47d562b4896e.tar.gz
    URL_HASH SHA256=2bb7095497a7bb7269dbdf841a7c6aa0939eaeb2a2bc9724ac4758d3ae3565a7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SimpleMultiAnalyzer
    COMPANY_NAME "sevenc-nanashi"
    BUNDLE_ID "com.sevenc-nanashi.simple-multi-analyzer"
    PLUGIN_MANUFACTURER_CODE Svcn
    PLUGIN_CODE Sman
    PRODUCT_NAME "simple_multi_analyzer"
    PLUGIN_NAME "simple_multi_analyzer.vst3"
    DESCRIPTION "Multi-channel spectrum analyzer"
    FORMATS VST3
    VST3_CATEGORIES Fx Analyzer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SimpleMultiAnalyzer PRIVATE
    Source/Analyzer.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_definitions(SimpleMultiAnalyzer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SimpleMultiAnalyzer PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(SimpleMultiAnalyzer PUBLIC /utf-8)
endif()

if(WIN32)
    add_custom_target(package-windows
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DJUCE_SOURCE_DIR=${juce_SOURCE_DIR}"
            "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_SOURCE_DIR}/cmake/Package.cmake"
        DEPENDS SimpleMultiAnalyzer_VST3 VERBATIM)
endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB ANALYZER_SOURCES CONFIGURE_DEPENDS Source/*.h Source/*.cpp)
    add_custom_target(format
        COMMAND "${CLANG_FORMAT}" -i ${ANALYZER_SOURCES} VERBATIM)
    add_custom_target(format-check
        COMMAND "${CLANG_FORMAT}" --dry-run --Werror ${ANALYZER_SOURCES} VERBATIM)
endif()
