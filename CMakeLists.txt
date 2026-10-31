cmake_minimum_required(VERSION 3.22)
project(KWEQLab VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(KWEQLab
    COMPANY_NAME "KWBoost"
    PLUGIN_MANUFACTURER_CODE KWbs
    PLUGIN_CODE KWlb
    FORMATS VST3
    PRODUCT_NAME "KW EQ Lab"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(KWEQLab PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/KwLab.h)
target_compile_features(KWEQLab PRIVATE cxx_std_17)
target_compile_definitions(KWEQLab PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(KWEQLab PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Optional developer tool (not needed to use the plugin): renders the editor to PNG files
option(KW_BUILD_TESTS "Build the editor snapshot tool" OFF)
if(KW_BUILD_TESTS)
    juce_add_console_app(KWLabSnapshot PRODUCT_NAME "KWLabSnapshot")
    target_sources(KWLabSnapshot PRIVATE Tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(KWLabSnapshot PRIVATE Source)
    target_compile_features(KWLabSnapshot PRIVATE cxx_std_17)
    target_compile_definitions(KWLabSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0)
    target_link_libraries(KWLabSnapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
