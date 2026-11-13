cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
# Windows: C++ ランタイムを静的リンク（VC++ 再頒布パッケージ不要）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(EQAnatomyLab2v1 VERSION 1.0.0)   # EQ Anatomy Lab 2 1.0 (separate product from Lab 2 β 0.2.x and Lab 0.1.x)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ビルド後に自分の Plug-Ins フォルダへコピーするか（手元では ON、配布ビルドでは OFF）
option(EQA_COPY_AFTER_BUILD "Copy plugins to the plug-in folders after build" ON)

# 手元の JUCE を使う: -DJUCE_PATH=/path/to/JUCE ／ 未指定なら GitHub から JUCE_GIT_TAG を取得
set(JUCE_PATH "" CACHE PATH "Local JUCE folder (optional)")
set(JUCE_GIT_TAG "9.0.4" CACHE STRING "JUCE tag to fetch when JUCE_PATH is empty")
if(JUCE_PATH)
  add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_GIT_TAG}
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
  set(EQA_FORMATS AU VST3 Standalone)
else()
  set(EQA_FORMATS VST3 Standalone)
endif()

juce_add_plugin(EQAnatomyLab2v1
  COMPANY_NAME "JPK"
  PLUGIN_MANUFACTURER_CODE Jpkl
  PLUGIN_CODE Eqa3
  FORMATS ${EQA_FORMATS}
  PRODUCT_NAME "EQ Anatomy Lab 2 v1"
  BUNDLE_ID com.jpk.eqanatomylab2v1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx EQ
  COPY_PLUGIN_AFTER_BUILD ${EQA_COPY_AFTER_BUILD})

target_sources(EQAnatomyLab2v1 PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
if(MSVC)
  target_compile_options(EQAnatomyLab2v1 PRIVATE /utf-8)   # 日本語文字列のため
endif()
target_compile_definitions(EQAnatomyLab2v1 PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(EQAnatomyLab2v1 PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
