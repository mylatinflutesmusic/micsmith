cmake_minimum_required(VERSION 3.22)

# Must be set before project(). macOS High Sierra 10.13 is Intel-only, so an
# x86_64 build is the correct target; it also runs on Apple Silicon via Rosetta.
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Support macOS High Sierra 10.13 and later")
    set (CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Architectures to build for")
endif()

project (MicSmith VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory (JUCE)

#==============================================================================
# Audio Units as well as VST3 on macOS: Logic and GarageBand only load AU.
if (APPLE)
    set (MICSMITH_FORMATS AU VST3 Standalone)
else()
    set (MICSMITH_FORMATS VST3 Standalone)
endif()

juce_add_plugin (MicSmith
    COMPANY_NAME             "MicSmith Audio"
    BUNDLE_ID                "com.micsmithaudio.micsmith"
    PLUGIN_MANUFACTURER_CODE Msmi
    PLUGIN_CODE              Msm1
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "MicSmith processes audio through its standalone app for auditioning."
    FORMATS                  ${MICSMITH_FORMATS}
    PRODUCT_NAME             "MicSmith"
    DESCRIPTION              "Microphone modelling processor"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    PLUGIN_CATEGORY          "Fx"
    VST3_CATEGORIES          "Fx")

target_sources (MicSmith PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/MicSmithLookAndFeel.cpp
    Source/ui/LabelledKnob.cpp
    Source/ui/ResponseDisplay.cpp)

target_include_directories (MicSmith PRIVATE Source)

target_compile_definitions (MicSmith PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries (MicSmith PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (APPLE)
    # Newer Xcode SDKs can emit libc++ calls that older macOS (10.13-10.15) does
    # not export, which leaves the plugin unable to load. Linking the C++ runtime
    # classically avoids those symbols. See BUILD-MACOS.md.
    option (MICSMITH_LD_CLASSIC "Link the C++ runtime classically for old macOS targets" OFF)

    if (MICSMITH_LD_CLASSIC)
        target_link_options (MicSmith PRIVATE "LINKER:-ld_classic")
    endif()
endif()

#==============================================================================
# JUCE-free numeric test for the modelling maths.
add_executable (MicSmithDSPTest tools/dsp_test.cpp)
target_include_directories (MicSmithDSPTest PRIVATE Source)
target_compile_features (MicSmithDSPTest PRIVATE cxx_std_17)

#==============================================================================
# Host-side validation: loads the built plugin through JUCE's VST3 host format.
juce_add_console_app (MicSmithHostTest PRODUCT_NAME "MicSmithHostTest" EXCLUDE_FROM_ALL)

target_sources (MicSmithHostTest PRIVATE
    tools/host_test.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/MicSmithLookAndFeel.cpp
    Source/ui/LabelledKnob.cpp
    Source/ui/ResponseDisplay.cpp)

target_include_directories (MicSmithHostTest PRIVATE Source)

target_compile_definitions (MicSmithHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (MicSmithHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
