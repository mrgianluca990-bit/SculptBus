cmake_minimum_required(VERSION 3.22)
project(SculptBus VERSION 0.3.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(SCULPT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SCULPT_FORMATS AU)
endif()
juce_add_binary_data(SculptAssets
    SOURCES
        assets/sculpt_bus_gui_bg_v03.png
)
juce_add_plugin(SculptBus
    COMPANY_NAME "Sculpt"
    BUNDLE_ID "audio.sculpt.bus"
    PLUGIN_MANUFACTURER_CODE ScPt
    PLUGIN_CODE ScBs
    FORMATS ${SCULPT_FORMATS}
    PRODUCT_NAME "Sculpt Bus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(SculptBus PRIVATE
    PluginProcessor.cpp PluginProcessor.h PluginEditor.cpp PluginEditor.h)
target_compile_definitions(SculptBus PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SculptBus PRIVATE
    SculptAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
