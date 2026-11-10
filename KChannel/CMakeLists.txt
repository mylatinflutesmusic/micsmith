cmake_minimum_required(VERSION 3.22)

# High Sierra is Intel-only. Set this before project() so the macOS SDK and
# deployment target are applied to JUCE and every target built here.
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING
         "Support macOS High Sierra 10.13 and later")
    set (CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING
         "Architectures to build for")
endif()

project (KChannel VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (KCHANNEL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH
     "JUCE checkout to build against")
set (KCHANNEL_VST2_SDK "" CACHE PATH
     "Directory containing pluginterfaces/vst2.x/aeffect.h")
set (KCHANNEL_FORMATS "" CACHE STRING
     "Override the plugin formats (for example VST or VST;VST3)")
option (KCHANNEL_BUILD_VST2 "Add VST2 to the default Windows formats" OFF)
option (KCHANNEL_WINDOWS7_COMPAT "Target Windows 7 and use the static MSVC runtime" OFF)

if (WIN32 AND KCHANNEL_WINDOWS7_COMPAT)
    # Cubase 5 machines often have no modern VC++ redistributable.
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory ("${KCHANNEL_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

set (KCHANNEL_ACTIVE_FORMATS "${KCHANNEL_FORMATS}")

if (NOT KCHANNEL_ACTIVE_FORMATS)
    if (APPLE)
        set (KCHANNEL_ACTIVE_FORMATS AU VST3 Standalone)
    elseif (WIN32)
        set (KCHANNEL_ACTIVE_FORMATS VST3 Standalone)

        if (KCHANNEL_BUILD_VST2)
            list (APPEND KCHANNEL_ACTIVE_FORMATS VST)
        endif()
    else()
        set (KCHANNEL_ACTIVE_FORMATS VST3 Standalone)
    endif()
endif()

if ("VST" IN_LIST KCHANNEL_ACTIVE_FORMATS)
    if (NOT KCHANNEL_VST2_SDK)
        message (FATAL_ERROR "VST2 requires KCHANNEL_VST2_SDK to point at the legacy VST2 SDK")
    endif()

    juce_set_vst2_sdk_path ("${KCHANNEL_VST2_SDK}")
endif()

juce_add_plugin (KChannel
    COMPANY_NAME             "K Channel Audio"
    BUNDLE_ID                "com.kchannelaudio.kchannel"
    PLUGIN_MANUFACTURER_CODE KCHN
    PLUGIN_CODE              KCH1
    FORMATS                  ${KCHANNEL_ACTIVE_FORMATS}
    PRODUCT_NAME             "K Channel"
    DESCRIPTION              "9000K-style console channel with Quell resonance control"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    PLUGIN_CATEGORY          "Fx"
    VST3_CATEGORIES          "Fx")

target_sources (KChannel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/ChannelDsp.cpp
    Source/ui/ChannelDisplay.cpp
    Source/ui/ChannelLookAndFeel.cpp
    Source/ui/Knob.cpp
    ../Quell/Source/dsp/SpectralSuppressor.cpp)

target_include_directories (KChannel PRIVATE
    Source
    ../Quell/Source)

target_compile_definitions (KChannel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

if (WIN32 AND KCHANNEL_WINDOWS7_COMPAT)
    target_compile_definitions (KChannel PUBLIC
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000
        JUCE_WIN_PER_MONITOR_DPI_AWARE=0
        JUCE_USE_WINDOWS_POINTER_API=0)
endif()

target_link_libraries (KChannel PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (APPLE)
    # Newer Xcode SDKs can emit libc++ calls that macOS 10.13-10.15 does not
    # export. Linking the C++ runtime classically avoids those symbols.
    option (KCHANNEL_LD_CLASSIC "Link the C++ runtime classically for old macOS targets" OFF)

    if (KCHANNEL_LD_CLASSIC)
        target_link_options (KChannel PRIVATE "LINKER:-ld_classic")
    endif()
endif()

#===============================================================================
# JUCE-free numeric checks for the channel strip.
add_executable (KChannelDSPTest
    tools/dsp_test.cpp
    Source/dsp/ChannelDsp.cpp)

target_include_directories (KChannelDSPTest PRIVATE Source)
target_compile_features (KChannelDSPTest PRIVATE cxx_std_17)

#===============================================================================
# Host-side validation: loads the built VST3 through JUCE's host code.
juce_add_console_app (KChannelHostTest PRODUCT_NAME "KChannelHostTest" EXCLUDE_FROM_ALL)

target_sources (KChannelHostTest PRIVATE tools/host_test.cpp)

target_compile_definitions (KChannelHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (KChannelHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)

if ("VST" IN_LIST KCHANNEL_ACTIVE_FORMATS)
    # A 32-bit build of this target can load the 32-bit VST2 plugin that
    # Cubase 5 uses, so the legacy host path is tested directly.
    juce_add_console_app (KChannelVST2HostTest PRODUCT_NAME "KChannelVST2HostTest" EXCLUDE_FROM_ALL)

    target_sources (KChannelVST2HostTest PRIVATE tools/vst2_host_test.cpp)

    target_compile_definitions (KChannelVST2HostTest PRIVATE
        JUCE_PLUGINHOST_VST=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries (KChannelVST2HostTest PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
