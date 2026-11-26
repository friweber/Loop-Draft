# Loop Master: the Loop Draft engine as an instrument plugin (VST3, AU, LV2) and a standalone app.
#
#   cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build plugin/build --config Release
#
# JUCE is fetched at configure time. It is dual-licensed: AGPLv3, or the JUCE licence (free "Starter" tier).
cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Oldest macOS the plugin runs on")
project(LoopMaster VERSION 0.1.0 LANGUAGES C CXX)

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
  PRIVATE loopdraft_engine LoopMasterData juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)
