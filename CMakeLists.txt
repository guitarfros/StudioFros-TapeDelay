cmake_minimum_required(VERSION 3.22)
project(StudioFrosTapeDelay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---- options -----------------------------------------------------------------
option(STUDIOFROS_INSTALL_AFTER_BUILD
       "Copy the VST3 into the user's VST3 folder (~/.vst3 on Linux) after building" ON)
option(STUDIOFROS_BUILD_TESTS "Build the standalone DSP engine tests" OFF)
set(STUDIOFROS_JUCE_TAG "8.0.6" CACHE STRING "JUCE git tag to download")
set(STUDIOFROS_JUCE_PATH "" CACHE PATH "Use a local JUCE checkout instead of downloading one")

# ---- JUCE ---------------------------------------------------------------------
if(STUDIOFROS_JUCE_PATH)
  add_subdirectory(${STUDIOFROS_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${STUDIOFROS_JUCE_TAG}
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# ---- plugin ---------------------------------------------------------------------
juce_add_binary_data(StudioFrosAssets SOURCES
  Assets/background.jpg
  Assets/switch_left.png
  Assets/switch_mid.png
  Assets/switch_right.png
  Assets/button_plate.png)

if(STUDIOFROS_INSTALL_AFTER_BUILD)
  set(_copy TRUE)
else()
  set(_copy FALSE)
endif()

juce_add_plugin(StudioFros
  COMPANY_NAME                 "GuitarFros"
  COMPANY_WEBSITE              "https://github.com/guitarfros"
  PRODUCT_NAME                 "StudioFros - Tape Delay"
  BUNDLE_ID                    "com.guitarfros.studiofros-tapedelay"
  PLUGIN_MANUFACTURER_CODE     Gfro
  PLUGIN_CODE                  Sftd
  FORMATS                      VST3 Standalone
  VST3_CATEGORIES              Fx Delay
  IS_SYNTH                     FALSE
  NEEDS_MIDI_INPUT             FALSE
  NEEDS_MIDI_OUTPUT            FALSE
  IS_MIDI_EFFECT               FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
  COPY_PLUGIN_AFTER_BUILD      ${_copy})

target_sources(StudioFros PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetManager.cpp)

target_include_directories(StudioFros PRIVATE Source)

target_compile_definitions(StudioFros PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(StudioFros
  PRIVATE
    StudioFrosAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- tests --------------------------------------------------------------------------
if(STUDIOFROS_BUILD_TESTS)
  enable_testing()
  add_executable(engine_test tests/engine_test.cpp)
  target_include_directories(engine_test PRIVATE Source)
  add_test(NAME engine_test COMMAND engine_test)
endif()
