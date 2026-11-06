cmake_minimum_required(VERSION 3.22)
project(MeteoraMagneto VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(MAGNETO_FORMATS VST3 AU)
else()
    set(MAGNETO_FORMATS VST3)
endif()

juce_add_plugin(MeteoraMagneto
    COMPANY_NAME "Meteora Plugins"
    BUNDLE_ID com.meteoraplugins.magneto
    PLUGIN_MANUFACTURER_CODE Mtra
    PLUGIN_CODE Mgnt
    FORMATS ${MAGNETO_FORMATS}
    PRODUCT_NAME "Magneto"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(MeteoraMagneto PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_binary_data(MagnetoAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Assets/background.jpg
        Assets/led_power_on.png
        Assets/led_tap_on.png
        Assets/mode_knob.png
        Assets/toggle_off.png
        Assets/reel.png
        Assets/capstan.png
        Assets/roller_a.png
        Assets/roller_b.png
        Assets/roller_c.png
        Assets/guide_a.png
        Assets/guide_b.png)

set_target_properties(MagnetoAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(MeteoraMagneto PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MeteoraMagneto
    PRIVATE
        MagnetoAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
