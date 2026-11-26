cmake_minimum_required(VERSION 3.22)
project(AtelierGrain VERSION 0.1.0)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(AtelierGrain
  COMPANY_NAME "Atelier" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE
  PLUGIN_MANUFACTURER_CODE Atel PLUGIN_CODE Agrn
  FORMATS VST3 PRODUCT_NAME "Atelier Grain" COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(AtelierGrain PRIVATE Source/Plugin.cpp)
target_compile_definitions(AtelierGrain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(AtelierGrain PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
