# Loop Master: the Loop Draft engine as an instrument plugin (VST3, AU, LV2) and a standalone app.
#
#   cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build plugin/build --config Release
#
# JUCE is fetched at configure time. It is dual-licensed: AGPLv3, or the JUCE licence (free "Starter" tier).
cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Oldest macOS the plugin runs on")
project(LoopMaster VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # no separate runtime to install
  add_compile_options(/utf-8)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.9
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# The engine, exactly the sources the phone apps use.
file(GLOB LD_ENGINE_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/*.cpp)
add_library(loopdraft_engine STATIC ${LD_ENGINE_SRC})
target_include_directories(loopdraft_engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../engine/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/src ${CMAKE_CURRENT_SOURCE_DIR}/../engine/third_party)
if(MSVC)
  target_compile_definitions(loopdraft_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# The presets, drum kits and grooves, inside the plugin.
file(GLOB_RECURSE LD_LIBRARY_JSON CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../content/library/*.json)
juce_add_binary_data(LoopMasterData NAMESPACE LibraryData SOURCES ${LD_LIBRARY_JSON})

# The techno database: all its MIDI files in one block, and the list that says what is where.
juce_add_binary_data(LoopMasterTechno HEADER_NAME TechnoData.h NAMESPACE TechnoData SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../content/techno/pack/techno.json
  ${CMAKE_CURRENT_SOURCE_DIR}/../content/techno/pack/techno.pack)

set(LM_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND LM_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
  list(APPEND LM_FORMATS LV2)
endif()

juce_add_plugin(LoopMaster
  PRODUCT_NAME "Loop Master"
  COMPANY_NAME "Loop Draft"
  BUNDLE_ID com.loopdraft.loopmaster
  PLUGIN_MANUFACTURER_CODE Lpdr
  PLUGIN_CODE Lpms
  FORMATS ${LM_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  LV2URI "https://github.com/friweber/Loop-Draft/loop-master")

target_sources(LoopMaster PRIVATE src/Processor.cpp src/Editor.cpp)
target_compile_definitions(LoopMaster PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)
target_link_libraries(LoopMaster
  PRIVATE loopdraft_engine LoopMasterData LoopMasterTechno juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)

# Loop Pedals: only the pedals, as an effect for sound that comes in (a guitar, a bass, a synth), and as an app.
juce_add_plugin(LoopPedals
  PRODUCT_NAME "Loop Pedals"
  COMPANY_NAME "Loop Draft"
  BUNDLE_ID com.loopdraft.looppedals
  PLUGIN_MANUFACTURER_CODE Lpdr
  PLUGIN_CODE Lppd
  FORMATS ${LM_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Loop Pedals needs the audio input to hear your instrument."
  VST3_CATEGORIES Fx
  AU_MAIN_TYPE kAudioUnitType_Effect
  LV2URI "https://github.com/friweber/Loop-Draft/loop-pedals")
target_sources(LoopPedals PRIVATE pedals/PedalsProcessor.cpp pedals/PedalsEditor.cpp)
target_include_directories(LoopPedals PUBLIC src pedals)
target_compile_definitions(LoopPedals PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)
target_link_libraries(LoopPedals
  PRIVATE loopdraft_engine juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)

# Runs the processor like a host would: transport, outputs, automation, MIDI, state. Used by CI.
option(LOOPMASTER_SELFTEST "Build the plugin self-test" OFF)
if(LOOPMASTER_SELFTEST)
  add_executable(LoopMasterSelfTest tests/SelfTest.cpp)
  target_include_directories(LoopMasterSelfTest PRIVATE $<TARGET_PROPERTY:LoopMaster,INCLUDE_DIRECTORIES>)
  target_compile_definitions(LoopMasterSelfTest PRIVATE $<TARGET_PROPERTY:LoopMaster,COMPILE_DEFINITIONS>)
  target_link_libraries(LoopMasterSelfTest PRIVATE LoopMaster)
  add_executable(LoopPedalsSelfTest tests/PedalsSelfTest.cpp)
  target_include_directories(LoopPedalsSelfTest PRIVATE $<TARGET_PROPERTY:LoopPedals,INCLUDE_DIRECTORIES>)
  target_compile_definitions(LoopPedalsSelfTest PRIVATE $<TARGET_PROPERTY:LoopPedals,COMPILE_DEFINITIONS>)
  target_link_libraries(LoopPedalsSelfTest PRIVATE LoopPedals)
endif()
