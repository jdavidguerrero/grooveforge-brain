cmake_minimum_required(VERSION 3.22)
project(GrooveForgeBrainPrototype VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GF_FIRMWARE "${CMAKE_CURRENT_LIST_DIR}/../firmware-teensy/src")
set(GF_BRIDGE "${CMAKE_CURRENT_LIST_DIR}/../bridge-protocol/include")
set(GF_DX7_SIX_SOURCES
    "${GF_FIRMWARE}/engines/dx7_6op_core.cpp"
    "${GF_FIRMWARE}/engines/dx7_6op_algorithms.cpp"
    "${GF_FIRMWARE}/engines/dx7_6op_sysex.cpp"
    "${GF_FIRMWARE}/engines/dx7_6op_params.cpp")
foreach(component env pitchenv lfo sin exp2 freqlut)
    list(APPEND GF_DX7_SIX_SOURCES "${GF_FIRMWARE}/engines/msfa/${component}.cpp")
endforeach()
add_library(gf_dx7_6op STATIC src/dx7_6op_preview.cpp ${GF_DX7_SIX_SOURCES})
target_include_directories(gf_dx7_6op PUBLIC src "${GF_FIRMWARE}")
set_target_properties(gf_dx7_6op PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(gf_preset_codec STATIC src/preset_codec.cpp src/hybrid_codec.cpp src/hybrid_router.cpp "${GF_FIRMWARE}/ui/layer_bank.cpp" "${GF_FIRMWARE}/ui/preset_bank.cpp"
    "${GF_FIRMWARE}/ui/page_params.cpp")
target_include_directories(gf_preset_codec PUBLIC src "${GF_FIRMWARE}" "${GF_BRIDGE}")
set_target_properties(gf_preset_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(gf_preset_codec PUBLIC gf_dx7_6op)
add_library(gf_prophet_preview STATIC src/prophet_preview.cpp "${GF_FIRMWARE}/engines/prophet_osc_core.cpp"
    "${GF_FIRMWARE}/dsp/lfo.cpp")
target_include_directories(gf_prophet_preview PUBLIC src "${GF_FIRMWARE}")
set_target_properties(gf_prophet_preview PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(gf_prophet_preset STATIC src/prophet_preset.cpp src/dx7_preview.cpp src/dx7_preset.cpp src/juno_preview.cpp src/juno_preset.cpp src/ob6_preview.cpp src/ob6_preset.cpp src/model_d_preview.cpp src/model_d_preset.cpp src/arp2600_preview.cpp src/arp2600_preset.cpp src/hybrid_preview.cpp "${GF_FIRMWARE}/engines/dx7_core.cpp")
target_link_libraries(gf_prophet_preset PUBLIC gf_prophet_preview gf_preset_codec gf_dx7_6op)
set_target_properties(gf_prophet_preset PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(GF_ENGINE_LIBRARIES_ONLY "Export only the JUCE-free engine libraries to a parent project" OFF)
if(GF_ENGINE_LIBRARIES_ONLY)
    return()
endif()

add_executable(gf_juno_preview_reference "${CMAKE_CURRENT_LIST_DIR}/../../tools/gemelo-baseline/juno_preview_reference.cpp")
target_link_libraries(gf_juno_preview_reference PRIVATE gf_prophet_preset)
add_executable(gf_ob6_preview_reference "${CMAKE_CURRENT_LIST_DIR}/../../tools/gemelo-baseline/ob6_preview_reference.cpp")
target_link_libraries(gf_ob6_preview_reference PRIVATE gf_prophet_preset)

enable_testing()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME juno_comparison_smoke COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/tests/juno_comparison_test.py" $<TARGET_FILE:gf_juno_preview_reference>)
    add_test(NAME ob6_comparison_smoke COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/tests/ob6_comparison_test.py" $<TARGET_FILE:gf_ob6_preview_reference>)
endif()
add_executable(gf_prophet_preview_test tests/prophet_preview_test.cpp)
target_link_libraries(gf_prophet_preview_test PRIVATE gf_prophet_preset)
add_test(NAME prophet_preview COMMAND gf_prophet_preview_test)
add_executable(gf_juno_preview_test tests/juno_preview_test.cpp)
target_link_libraries(gf_juno_preview_test PRIVATE gf_prophet_preset)
add_test(NAME juno_preview COMMAND gf_juno_preview_test)
add_executable(gf_ob6_preview_test tests/ob6_preview_test.cpp)
target_link_libraries(gf_ob6_preview_test PRIVATE gf_prophet_preset)
add_test(NAME ob6_preview COMMAND gf_ob6_preview_test)
add_executable(gf_model_d_preview_test tests/model_d_preview_test.cpp)
target_link_libraries(gf_model_d_preview_test PRIVATE gf_prophet_preset)
add_test(NAME model_d_preview COMMAND gf_model_d_preview_test)
add_executable(gf_arp2600_preview_test tests/arp2600_preview_test.cpp)
target_link_libraries(gf_arp2600_preview_test PRIVATE gf_prophet_preset)
add_test(NAME arp2600_preview COMMAND gf_arp2600_preview_test)
add_executable(gf_hybrid_preview_test tests/hybrid_preview_test.cpp)
target_link_libraries(gf_hybrid_preview_test PRIVATE gf_prophet_preset)
add_test(NAME hybrid_preview COMMAND gf_hybrid_preview_test)
add_executable(gf_hybrid_router_test tests/hybrid_router_test.cpp)
target_link_libraries(gf_hybrid_router_test PRIVATE gf_preset_codec)
add_test(NAME hybrid_router COMMAND gf_hybrid_router_test)
add_executable(gf_hybrid_codec_test tests/hybrid_codec_test.cpp)
target_link_libraries(gf_hybrid_codec_test PRIVATE gf_preset_codec)
add_test(NAME hybrid_codec COMMAND gf_hybrid_codec_test)
add_executable(gf_preset_codec_test tests/preset_codec_test.cpp)
target_link_libraries(gf_preset_codec_test PRIVATE gf_preset_codec)
add_test(NAME preset_codec COMMAND gf_preset_codec_test)

option(GF_BUILD_VST3 "Build the JUCE VST3 prototype" ON)
if(GF_BUILD_VST3)
    set(gf_bench_default OFF)
else()
    set(gf_bench_default ON)
endif()
option(GF_BUILD_ENGINE_BENCH "Build the JUCE-free Pi DSP benchmark" ${gf_bench_default})
if(GF_BUILD_ENGINE_BENCH)
    add_subdirectory(bench)
endif()

if(GF_BUILD_VST3)
    enable_language(C)
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
    # IBM Plex Mono (SIL OFL 1.1, OFL.txt junto a los .ttf): la misma tipografía que la cabeza de Studio
    juce_add_binary_data(GfBrainFonts HEADER_NAME gf_fonts.h NAMESPACE gf_fonts SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../studio-head/assets/fonts/IBMPlexMono-Regular.ttf"
        "${CMAKE_CURRENT_LIST_DIR}/../studio-head/assets/fonts/IBMPlexMono-SemiBold.ttf")
    set_target_properties(GfBrainFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_sources(GrooveForgeBrainPrototype PRIVATE src/plugin_processor.cpp)
    target_compile_definitions(GrooveForgeBrainPrototype PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GrooveForgeBrainPrototype PRIVATE gf_prophet_preset GfBrainFonts juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_executable(gf_plugin_smoke tests/plugin_smoke.cpp)
    target_compile_definitions(gf_plugin_smoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(gf_plugin_smoke PRIVATE GrooveForgeBrainPrototype gf_prophet_preset juce::juce_audio_utils)
    add_test(NAME plugin_smoke COMMAND gf_plugin_smoke)
endif()

include(tests/dx7_6op/reference.cmake)
add_executable(gf_dx7_6op_test ../firmware-teensy/test/test_dx7_6op/test_main.cpp)
target_compile_definitions(gf_dx7_6op_test PRIVATE GF_SIX_CTEST)
target_link_libraries(gf_dx7_6op_test PRIVATE gf_dx7_6op)
add_test(NAME dx7_6op COMMAND gf_dx7_6op_test)
