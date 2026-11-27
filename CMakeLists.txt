cmake_minimum_required(VERSION 3.22)
project(AtelierGrain VERSION 0.4.0)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)
juce_add_binary_data(AtelierAssets SOURCES Assets/logo.png)
set_target_properties(AtelierAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
juce_add_plugin(AtelierGrain
  COMPANY_NAME "Oktoplus" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE
  PLUGIN_MANUFACTURER_CODE Okto PLUGIN_CODE Osyn
  FORMATS VST3 PRODUCT_NAME "Oktoplus Synth" COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(AtelierGrain PRIVATE Source/Plugin.cpp)
target_compile_definitions(AtelierGrain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(AtelierGrain PRIVATE AtelierAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
target_compile_options(AtelierGrain PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
