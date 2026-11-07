cmake_minimum_required(VERSION 3.22)

project (Quell VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Reuse the JUCE checkout vendored at the repository root.
add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

if (APPLE)
    set (QUELL_FORMATS AU VST3 Standalone)
else()
    set (QUELL_FORMATS VST3 Standalone)
endif()

juce_add_plugin (Quell
    COMPANY_NAME             "Quell Audio"
    BUNDLE_ID                "com.quellaudio.quell"
    PLUGIN_MANUFACTURER_CODE QELL
    PLUGIN_CODE              QLL1
    FORMATS                  ${QUELL_FORMATS}
    PRODUCT_NAME             "Quell"
    DESCRIPTION              "Dynamic resonance suppressor"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    PLUGIN_CATEGORY          "Fx"
    VST3_CATEGORIES          "Fx")

target_sources (Quell PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetLibrary.cpp
    Source/dsp/SpectralSuppressor.cpp
    Source/ui/PresetBrowser.cpp
    Source/ui/QuellLookAndFeel.cpp
    Source/ui/LabelledKnob.cpp
    Source/ui/SpectrumDisplay.cpp)

target_include_directories (Quell PRIVATE Source)

target_compile_definitions (Quell PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries (Quell PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

#==============================================================================
# JUCE-free numeric checks for the suppression engine.
add_executable (QuellDSPTest
    tools/dsp_test.cpp
    Source/dsp/SpectralSuppressor.cpp)

target_include_directories (QuellDSPTest PRIVATE Source)
target_compile_features (QuellDSPTest PRIVATE cxx_std_17)

#==============================================================================
# Host-side validation: loads the built VST3 through JUCE's host code, and can
# render the editor to a PNG without a DAW.
juce_add_console_app (QuellHostTest PRODUCT_NAME "QuellHostTest" EXCLUDE_FROM_ALL)

target_sources (QuellHostTest PRIVATE
    tools/host_test.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetLibrary.cpp
    Source/dsp/SpectralSuppressor.cpp
    Source/ui/PresetBrowser.cpp
    Source/ui/QuellLookAndFeel.cpp
    Source/ui/LabelledKnob.cpp
    Source/ui/SpectrumDisplay.cpp)

target_include_directories (QuellHostTest PRIVATE Source)

target_compile_definitions (QuellHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (QuellHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
