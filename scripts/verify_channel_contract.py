#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ids = (ROOT / "source/pluginids.h").read_text(encoding="utf-8")
processor_h = (ROOT / "source/processor.h").read_text(encoding="utf-8")
processor_cpp = (ROOT / "source/processor.cpp").read_text(encoding="utf-8")
factory = (ROOT / "source/factory_channel.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
editor = (ROOT / "source/editor.cpp").read_text(encoding="utf-8")

def fail(message):
    raise SystemExit("CHANNEL CONTRACT FAIL: " + message)

for needle in ("kChannelProcessorUID", "kChannelControllerUID"):
    if needle not in ids:
        fail(f"missing dedicated Channel UID: {needle}")

for needle in (
    '#define stringPluginName "125A MixEngine V3 Channel"',
    "kChannelProcessorUID",
    "kChannelControllerUID",
    "kVstAudioEffectClass",
    "Vst::PlugType::kFx",
):
    if needle not in factory:
        fail(f"standard Channel factory token missing: {needle}")

if '"Audio Mix Processor"' in factory:
    fail("Channel factory still registers as PreSonus Audio Mix Processor")

if "MIXENGINE_CHANNEL_BUILD" not in processor_h or "MIXENGINE_CHANNEL_BUILD" not in processor_cpp:
    fail("Channel build does not gate the PreSonus interfaces")

if "public PresonusMixFx::IAudioMixProcessor" not in processor_h:
    fail("Mix FX implementation disappeared from shared source")
if "#ifndef MIXENGINE_CHANNEL_BUILD" not in processor_h:
    fail("PreSonus interfaces are not compile-time excluded for Channel")

for needle in (
    "setControllerClass(kChannelControllerUID)",
    "setControllerClass(kControllerUID)",
    "tresult PLUGIN_API Processor::process(ProcessData& data)",
):
    if needle not in processor_cpp:
        fail(f"shared Processor routing token missing: {needle}")

for needle in (
    "smtg_add_vst3plugin(125A-MixEngine-V3-Channel",
    "source/factory_channel.cpp",
    "MIXENGINE_CHANNEL_BUILD=1",
    "smtg_target_add_plugin_resources(125A-MixEngine-V3-Channel",
    'resource/mixengine_channel.uidesc',
):
    if needle not in cmake:
        fail(f"Channel CMake target token missing: {needle}")

if "source/factory.cpp" not in cmake:
    fail("original Mix FX target factory disappeared")

if '"mixengine_channel.uidesc"' not in editor or '"mixengine.uidesc"' not in editor:
    fail("editor does not select dedicated Channel/Mix FX UIDESC resources")
if "MIXENGINE_CHANNEL_BUILD" not in editor:
    fail("editor UIDESC selection is not gated by Channel build")

if "static constexpr int kMaxMixFxChannels = 0;" not in processor_h:
    fail("Channel build still carries full Mix FX source-state capacity")
channel_factory = (ROOT / "source/factory_channel.cpp").read_text(encoding="utf-8")
if 'sizeof(MixEngine::Processor) < 64 * 1024' not in channel_factory:
    fail("Channel processor size regression guard missing")

if 'project(MixEngine VERSION 3.0.0' not in cmake:
    fail("project version is not V3 3.0.0")
mix_factory = (ROOT / "source/factory.cpp").read_text(encoding="utf-8")
if '#define stringPluginName "125A MixEngine V3"' not in mix_factory:
    fail("Mix FX factory is not V3")
if 'smtg_add_vst3plugin(125A-MixEngine-V3 ' not in cmake:
    fail("V3 Mix FX CMake target missing")

print("Channel contract PASS: dedicated standard VST3 identity, shared DSP/GUI, Mix FX API gated")
