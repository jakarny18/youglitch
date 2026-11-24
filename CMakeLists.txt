cmake_minimum_required(VERSION 3.22)
project(YouGlitchSMP1024 VERSION 0.1.0)

# JUCE is fetched at configure time. Requires an internet connection on first configure.
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(YouGlitchSMP1024
    COMPANY_NAME "YouGlitch Audio"
    BUNDLE_ID "audio.youglitch.smp1024"
    PLUGIN_MANUFACTURER_CODE Ygch
    PLUGIN_CODE Yg24
    FORMATS VST3 Standalone
    PRODUCT_NAME "YouGlitch SMP 1024"
    DESCRIPTION "1024-step glitch groovebox"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(YouGlitchSMP1024)

target_sources(YouGlitchSMP1024 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StreamingSampleReader.cpp
    Source/StreamingSampleReader.h
    Source/StreamingVoicePool.cpp
    Source/StreamingVoicePool.h)

target_compile_features(YouGlitchSMP1024 PRIVATE cxx_std_17)
target_compile_definitions(YouGlitchSMP1024 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(YouGlitchSMP1024 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
