cmake_minimum_required(VERSION 3.25)
project(MixEngine VERSION 3.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
FetchContent_Declare(hiir GIT_REPOSITORY https://github.com/unevens/hiir.git GIT_TAG 4589fedb4d08b899514cb605ccd7418bf262ab18 SOURCE_DIR "${CMAKE_BINARY_DIR}/hiir")
FetchContent_MakeAvailable(hiir)

# Approved 125A Knob Designer asset set. Pin the exact successful generated-assets
# commit so GUI builds remain reproducible and never silently pick up a newer export.
FetchContent_Declare(knob_assets
 GIT_REPOSITORY https://github.com/challanger2000/125A-Knob-Designer.git
 GIT_TAG generated-assets-v0.1.0
 GIT_SHALLOW FALSE
 SOURCE_DIR "${CMAKE_BINARY_DIR}/125a-knob-assets")
FetchContent_GetProperties(knob_assets)
if(NOT knob_assets_POPULATED)
 FetchContent_Populate(knob_assets)
endif()
set(MIXENGINE_KNOB_RESOURCE_DIR "${knob_assets_SOURCE_DIR}/assets/mixengine")
set(MIXENGINE_KNOB_RESOURCES
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_L_128px_100pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_L_192px_150pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_L_256px_200pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_L_384px_300pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_M_96px_100pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_M_144px_150pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_M_192px_200pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_M_288px_300pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_S_64px_100pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_S_96px_150pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_S_128px_200pct_128f.png"
 "${MIXENGINE_KNOB_RESOURCE_DIR}/125A_MixEngine_Analog_S_192px_300pct_128f.png")
set(SMTG_CREATE_PLUGIN_LINK 0 CACHE BOOL "Disable automatic VST3 install links" FORCE)
if(NOT DEFINED VST3_SDK_ROOT)
 if(DEFINED ENV{VST3_SDK_ROOT})
  set(VST3_SDK_ROOT "$ENV{VST3_SDK_ROOT}")
 else()
  message(FATAL_ERROR "Set VST3_SDK_ROOT to the Steinberg VST3 SDK directory.")
 endif()
endif()
file(TO_CMAKE_PATH "${VST3_SDK_ROOT}" vst3sdk_SOURCE_DIR)
add_subdirectory("${vst3sdk_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/vst3sdk")
smtg_enable_vst3_sdk()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resource/125A_Logo_Master_FINAL.svg" MIXENGINE_BRANDING_SVG)
string(REGEX MATCHALL "<path d=\\\"[^\\\"]+\\\" fill=\\\"#[0-9A-Fa-f]+\\\"/>" MIXENGINE_BRANDING_PATH_TAGS "${MIXENGINE_BRANDING_SVG}")
list(LENGTH MIXENGINE_BRANDING_PATH_TAGS MIXENGINE_BRANDING_PATH_COUNT)
if(NOT MIXENGINE_BRANDING_PATH_COUNT EQUAL 30)
 message(FATAL_ERROR "125A branding master must contain exactly 30 vector paths; found ${MIXENGINE_BRANDING_PATH_COUNT}.")
endif()
set(MIXENGINE_BRANDING_PATHS "")
foreach(MIXENGINE_BRANDING_TAG IN LISTS MIXENGINE_BRANDING_PATH_TAGS)
 if(NOT MIXENGINE_BRANDING_TAG MATCHES "^<path d=\\\"([^\\\"]+)\\\" fill=\\\"([^\\\"]+)\\\"/>$")
  message(FATAL_ERROR "Could not parse 125A branding path tag.")
 endif()
 set(MIXENGINE_BRANDING_D "${CMAKE_MATCH_1}")
 set(MIXENGINE_BRANDING_FILL "${CMAKE_MATCH_2}")
 if(MIXENGINE_BRANDING_FILL STREQUAL "#D71920")
  set(MIXENGINE_BRANDING_RED "true")
 elseif(MIXENGINE_BRANDING_FILL STREQUAL "#D9D9D9")
  set(MIXENGINE_BRANDING_RED "false")
 else()
  message(FATAL_ERROR "Unexpected 125A branding fill: ${MIXENGINE_BRANDING_FILL}")
 endif()
 string(APPEND MIXENGINE_BRANDING_PATHS "    {\"${MIXENGINE_BRANDING_D}\", ${MIXENGINE_BRANDING_RED}},\n")
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/source/branding_master.h.in" "${CMAKE_BINARY_DIR}/branding_master.h" @ONLY)

smtg_add_vst3plugin(125A-MixEngine-V3 source/pluginids.h source/oversampling.h source/latency_alignment.h source/nonlinear_cores.h source/metering.h source/meter_exchange.h source/stereo_field.h source/processor.h source/processor.cpp source/controller.h source/controller.cpp source/editor.cpp source/factory.cpp source/HardwareControls.h source/HardwareControls.cpp)
target_sources(125A-MixEngine-V3 PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_link_libraries(125A-MixEngine-V3 PRIVATE sdk vstgui_support)
target_include_directories(125A-MixEngine-V3 PRIVATE "${CMAKE_BINARY_DIR}")
smtg_target_add_plugin_resources(125A-MixEngine-V3 RESOURCES "resource/mixengine.uidesc" ${MIXENGINE_KNOB_RESOURCES})

smtg_add_vst3plugin(125A-MixEngine-V3-Channel source/pluginids.h source/oversampling.h source/latency_alignment.h source/nonlinear_cores.h source/metering.h source/meter_exchange.h source/stereo_field.h source/processor.h source/processor.cpp source/controller.h source/controller.cpp source/editor.cpp source/factory_channel.cpp source/HardwareControls.h source/HardwareControls.cpp)
target_sources(125A-MixEngine-V3-Channel PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_link_libraries(125A-MixEngine-V3-Channel PRIVATE sdk vstgui_support)
target_include_directories(125A-MixEngine-V3-Channel PRIVATE "${CMAKE_BINARY_DIR}")
target_compile_definitions(125A-MixEngine-V3-Channel PRIVATE MIXENGINE_CHANNEL_BUILD=1)
smtg_target_add_plugin_resources(125A-MixEngine-V3-Channel RESOURCES "resource/mixengine_channel.uidesc" ${MIXENGINE_KNOB_RESOURCES})
add_executable(oversampling-roundtrip tests/oversampling_roundtrip.cpp source/oversampling.h)
target_include_directories(oversampling-roundtrip PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(oversampling-alias tests/oversampling_alias.cpp source/oversampling.h)
target_include_directories(oversampling-alias PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(tube-oversampling tests/tube_oversampling.cpp source/oversampling.h)
target_include_directories(tube-oversampling PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(media-oversampling tests/media_oversampling.cpp source/oversampling.h)
target_include_directories(media-oversampling PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(console-core-equivalence tests/console_core_equivalence.cpp source/nonlinear_cores.h)
target_include_directories(console-core-equivalence PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
add_executable(console-tube-island tests/console_tube_island.cpp source/nonlinear_cores.h source/oversampling.h)
target_include_directories(console-tube-island PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(console-tube-ordering tests/console_tube_ordering.cpp source/nonlinear_cores.h)
target_include_directories(console-tube-ordering PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
add_executable(oversampling-latency tests/oversampling_latency.cpp source/oversampling.h)
target_include_directories(oversampling-latency PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")
add_executable(metering-diagnostic tests/metering.cpp source/metering.h)
target_include_directories(metering-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
add_executable(stereo-field-diagnostic tests/stereo_field.cpp source/stereo_field.h)
target_include_directories(stereo-field-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
add_executable(metering-integration tests/metering_integration.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(metering-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(metering-integration PRIVATE sdk)
add_executable(processor-latency-integration tests/processor_latency.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(processor-latency-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(processor-latency-integration PRIVATE sdk)
add_executable(phase-mono-diagnostic tests/phase_mono.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(phase-mono-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(phase-mono-diagnostic PRIVATE sdk)
add_executable(host-matrix-diagnostic tests/host_matrix.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(host-matrix-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(host-matrix-diagnostic PRIVATE sdk)

add_executable(state-roundtrip-diagnostic tests/state_roundtrip.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp)
target_include_directories(state-roundtrip-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(state-roundtrip-diagnostic PRIVATE sdk)

add_executable(state-migration-diagnostic tests/state_migration.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp)
target_include_directories(state-migration-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(state-migration-diagnostic PRIVATE sdk)

add_executable(release-robustness-diagnostic tests/release_robustness.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(release-robustness-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(release-robustness-diagnostic PRIVATE sdk)

add_executable(controller-lifecycle-diagnostic tests/controller_lifecycle.cpp source/controller.cpp source/editor.cpp source/HardwareControls.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(controller-lifecycle-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(controller-lifecycle-diagnostic PRIVATE sdk vstgui_support)
if(WIN32)
 target_sources(controller-lifecycle-diagnostic PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
endif()
add_executable(latency-matrix-diagnostic tests/latency_matrix.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(latency-matrix-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(latency-matrix-diagnostic PRIVATE sdk)
add_executable(console-noise-contract-diagnostic tests/console_noise_contract.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(console-noise-contract-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(console-noise-contract-diagnostic PRIVATE sdk)
add_executable(level-match-diagnostic tests/level_match.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(level-match-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(level-match-diagnostic PRIVATE sdk)
add_executable(automation-stress-diagnostic tests/automation_stress.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(automation-stress-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(automation-stress-diagnostic PRIVATE sdk)

add_executable(parameter-transition-diagnostic tests/parameter_transition.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(parameter-transition-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(parameter-transition-diagnostic PRIVATE sdk)
add_executable(mixfx-crosstalk-diagnostic tests/mixfx_crosstalk.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(mixfx-crosstalk-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(mixfx-crosstalk-diagnostic PRIVATE sdk)
if(WIN32)
 target_sources(125A-MixEngine-V3 PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
 target_sources(125A-MixEngine-V3-Channel PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
endif()
smtg_target_configure_version_file(125A-MixEngine-V3)
smtg_target_configure_version_file(125A-MixEngine-V3-Channel)
target_compile_definitions(125A-MixEngine-V3 PRIVATE MIXENGINE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(125A-MixEngine-V3-Channel PRIVATE MIXENGINE_VERSION="${PROJECT_VERSION}")

add_executable(range-philosophy-diagnostic tests/range_philosophy.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(range-philosophy-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(range-philosophy-diagnostic PRIVATE sdk)

add_executable(tape-memory-diagnostic tests/tape_memory.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(tape-memory-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(tape-memory-diagnostic PRIVATE sdk)

add_executable(tape-hiss-contract-diagnostic tests/tape_hiss_contract.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(tape-hiss-contract-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(tape-hiss-contract-diagnostic PRIVATE sdk)

add_executable(secondary-controls-diagnostic tests/secondary_controls.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(secondary-controls-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(secondary-controls-diagnostic PRIVATE sdk)

add_executable(glue-transient-diagnostic tests/glue_transient.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(glue-transient-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(glue-transient-diagnostic PRIVATE sdk)

add_executable(vinyl-material-diagnostic tests/vinyl_material.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(vinyl-material-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(vinyl-material-diagnostic PRIVATE sdk)

add_executable(console-character-diagnostic tests/console_character.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(console-character-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(console-character-diagnostic PRIVATE sdk)

add_executable(console-vintage-memory-diagnostic tests/console_vintage_memory.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(console-vintage-memory-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(console-vintage-memory-diagnostic PRIVATE sdk)

add_executable(quality-switch-diagnostic tests/quality_switch.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(quality-switch-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(quality-switch-diagnostic PRIVATE sdk)

add_executable(zero-neutrality-diagnostic tests/zero_neutrality.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(zero-neutrality-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(zero-neutrality-diagnostic PRIVATE sdk)

add_executable(sample-accuracy-diagnostic tests/sample_accuracy.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(sample-accuracy-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(sample-accuracy-diagnostic PRIVATE sdk)

add_executable(mixfx-sample-accuracy-diagnostic tests/mixfx_sample_accuracy.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(mixfx-sample-accuracy-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(mixfx-sample-accuracy-diagnostic PRIVATE sdk)

add_executable(sonic-impact-diagnostic tests/sonic_impact.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(sonic-impact-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(sonic-impact-diagnostic PRIVATE sdk)

add_executable(program-material-diagnostic tests/program_material.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(program-material-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(program-material-diagnostic PRIVATE sdk)

add_executable(sample-rate-matrix-diagnostic tests/sample_rate_matrix.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(sample-rate-matrix-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(sample-rate-matrix-diagnostic PRIVATE sdk)

add_executable(character-morph-diagnostic tests/character_morph.cpp)
target_include_directories(character-morph-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(denormal-safety-diagnostic tests/denormal_safety.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(denormal-safety-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(denormal-safety-diagnostic PRIVATE sdk)

add_executable(tape-v3-research-diagnostic tests/tape_v3_model.cpp source/tape_v3_model.h source/oversampling.h)
target_include_directories(tape-v3-research-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")

add_executable(tape-v3-ja-oracle-diagnostic tests/tape_v3_ja_oracle.cpp source/tape_v3_ja_oracle.h)
target_include_directories(tape-v3-ja-oracle-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(tape-v3-sample-rate-diagnostic tests/tape_v3_sample_rate.cpp source/tape_v3_model.h source/oversampling.h)
target_include_directories(tape-v3-sample-rate-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")

add_executable(tape-v3-autolevel-diagnostic tests/tape_v3_autolevel.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(tape-v3-autolevel-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(tape-v3-autolevel-diagnostic PRIVATE sdk)

add_executable(console-v3-research-diagnostic tests/console_v3_model.cpp source/console_v3_model.h source/nonlinear_cores.h)
target_include_directories(console-v3-research-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(tube-v3-research-diagnostic tests/tube_v3_model.cpp source/tube_v3_model.h source/oversampling.h source/latency_alignment.h)
target_include_directories(tube-v3-research-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")

add_executable(tube-v3-autolevel-diagnostic tests/tube_v3_autolevel.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(tube-v3-autolevel-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(tube-v3-autolevel-diagnostic PRIVATE sdk)

add_executable(mixfx-console-v3-diagnostic tests/mixfx_console_v3.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(mixfx-console-v3-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(mixfx-console-v3-diagnostic PRIVATE sdk)

add_executable(console-v3-alias-diagnostic tests/console_v3_alias.cpp source/console_v3_model.h)
target_include_directories(console-v3-alias-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-cpu-diagnostic tests/console_v3_cpu.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(console-v3-cpu-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(console-v3-cpu-diagnostic PRIVATE sdk)

add_executable(console-v3-adaa-diagnostic tests/console_v3_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-adaa-production-gate tests/console_v3_adaa_production.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-adaa-production-gate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-decoder-adaa-diagnostic tests/console_v3_decoder_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-decoder-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-fast-adaa-diagnostic tests/console_v3_fast_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-fast-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-prepared-adaa-diagnostic tests/console_v3_prepared_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-prepared-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-prepared-cpu-diagnostic tests/console_v3_prepared_cpu.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-prepared-cpu-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-correction-adaa-diagnostic tests/console_v3_correction_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-correction-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-residual-adaa-diagnostic tests/console_v3_residual_adaa.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-residual-adaa-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(console-v3-correction-adaa2-diagnostic tests/console_v3_correction_adaa2.cpp source/console_v3_adaa.h source/console_v3_model.h)
target_include_directories(console-v3-correction-adaa2-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(vinyl-v3-tracing-oracle-diagnostic tests/vinyl_v3_tracing_oracle.cpp source/vinyl_v3_tracing_oracle.h)
target_include_directories(vinyl-v3-tracing-oracle-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(vinyl-v3-surrogate-diagnostic tests/vinyl_v3_surrogate.cpp source/vinyl_v3_model.h source/vinyl_v3_tracing_oracle.h)
target_include_directories(vinyl-v3-surrogate-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(vinyl-v3-alias-diagnostic tests/vinyl_v3_alias.cpp source/vinyl_v3_model.h source/oversampling.h)
target_include_directories(vinyl-v3-alias-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}")

add_executable(vinyl-v3-live-diagnostic tests/vinyl_v3_live.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(vinyl-v3-live-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(vinyl-v3-live-diagnostic PRIVATE sdk)

add_executable(vinyl-v3-autolevel-diagnostic tests/vinyl_v3_autolevel.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(vinyl-v3-autolevel-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(vinyl-v3-autolevel-diagnostic PRIVATE sdk)

add_executable(vinyl-v3-cpu-diagnostic tests/vinyl_v3_cpu.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(vinyl-v3-cpu-diagnostic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(vinyl-v3-cpu-diagnostic PRIVATE sdk)

add_executable(v3-autolevel-level-sweep tests/v3_autolevel_level_sweep.cpp source/processor.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/dataexchange.cpp)
target_include_directories(v3-autolevel-level-sweep PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_BINARY_DIR}" "${vst3sdk_SOURCE_DIR}")
target_link_libraries(v3-autolevel-level-sweep PRIVATE sdk)
