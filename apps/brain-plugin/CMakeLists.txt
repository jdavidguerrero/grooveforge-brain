cmake_minimum_required(VERSION 3.22)
project(GrooveForgeBrainPrototype VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GF_FIRMWARE "${CMAKE_CURRENT_LIST_DIR}/../firmware-teensy/src")
set(GF_BRIDGE "${CMAKE_CURRENT_LIST_DIR}/../bridge-protocol/include")
add_library(gf_preset_codec STATIC src/preset_codec.cpp "${GF_FIRMWARE}/ui/preset_bank.cpp"
    "${GF_FIRMWARE}/ui/page_params.cpp")
target_include_directories(gf_preset_codec PUBLIC src "${GF_FIRMWARE}" "${GF_BRIDGE}")
set_target_properties(gf_preset_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(gf_prophet_preview STATIC src/prophet_preview.cpp "${GF_FIRMWARE}/engines/prophet_osc_core.cpp"
    "${GF_FIRMWARE}/dsp/lfo.cpp")
target_include_directories(gf_prophet_preview PUBLIC src "${GF_FIRMWARE}")
set_target_properties(gf_prophet_preview PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(gf_prophet_preset STATIC src/prophet_preset.cpp src/dx7_preview.cpp src/dx7_preset.cpp src/juno_preview.cpp src/juno_preset.cpp "${GF_FIRMWARE}/engines/dx7_core.cpp")
target_link_libraries(gf_prophet_preset PUBLIC gf_prophet_preview gf_preset_codec)
set_target_properties(gf_prophet_preset PROPERTIES POSITION_INDEPENDENT_CODE ON)

enable_testing()
add_executable(gf_prophet_preview_test tests/prophet_preview_test.cpp)
target_link_libraries(gf_prophet_preview_test PRIVATE gf_prophet_preset)
add_test(NAME prophet_preview COMMAND gf_prophet_preview_test)
add_executable(gf_juno_preview_test tests/juno_preview_test.cpp)
target_link_libraries(gf_juno_preview_test PRIVATE gf_prophet_preset)
add_test(NAME juno_preview COMMAND gf_juno_preview_test)
add_executable(gf_preset_codec_test tests/preset_codec_test.cpp)
target_link_libraries(gf_preset_codec_test PRIVATE gf_preset_codec)
add_test(NAME preset_codec COMMAND gf_preset_codec_test)

option(GF_BUILD_VST3 "Build the JUCE VST3 prototype" ON)
if(GF_BUILD_VST3)
    set(GF_JUCE_DIR "" CACHE PATH "Path to an existing JUCE source checkout")
    if(NOT EXISTS "${GF_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Set GF_JUCE_DIR to a local JUCE source checkout; see README.md")
    endif()
    add_subdirectory("${GF_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
    juce_add_plugin(GrooveForgeBrainPrototype
        COMPANY_NAME GrooveForge
        BUNDLE_ID com.grooveforge.brain.prototype
        PLUGIN_MANUFACTURER_CODE GrFg
        PLUGIN_CODE GfP1
        FORMATS VST3
        PRODUCT_NAME "GrooveForge Brain Prototype"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(GrooveForgeBrainPrototype PRIVATE src/plugin_processor.cpp)
    target_compile_definitions(GrooveForgeBrainPrototype PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GrooveForgeBrainPrototype PRIVATE gf_prophet_preset juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_executable(gf_plugin_smoke tests/plugin_smoke.cpp)
    target_compile_definitions(gf_plugin_smoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(gf_plugin_smoke PRIVATE GrooveForgeBrainPrototype gf_prophet_preset juce::juce_audio_utils)
    add_test(NAME plugin_smoke COMMAND gf_plugin_smoke)
endif()
