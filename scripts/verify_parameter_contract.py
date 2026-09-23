#!/usr/bin/env python3
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ids_text = (ROOT / "source/pluginids.h").read_text(encoding="utf-8")
controller = (ROOT / "source/controller.cpp").read_text(encoding="utf-8")
processor = (ROOT / "source/processor.cpp").read_text(encoding="utf-8")
processor_h = (ROOT / "source/processor.h").read_text(encoding="utf-8")
hardware = (ROOT / "source/HardwareControls.cpp").read_text(encoding="utf-8")
metering = (ROOT / "source/metering.h").read_text(encoding="utf-8")

EXPECTED_PARAMS = [
    "kParamBypass", "kParamInput", "kParamCalibration", "kParamAutoGain",
    "kParamConsoleOn", "kParamConsoleMode", "kParamConsoleDrive", "kParamConsoleCrosstalk",
    "kParamTubeOn", "kParamTubeAmount",
    "kParamTapeOn", "kParamTapeAmount", "kParamTapeSpeed", "kParamTapeStability",
    "kParamGlueOn", "kParamGlueAmount", "kParamGlueCharacter",
    "kParamVinylOn", "kParamVinylCharacter", "kParamVinylWear",
    "kParamDepth", "kParamWidth", "kParamLowMono",
    "kParamConsoleNoise", "kParamQuality", "kParamOutput",
    "kParamTubeType", "kParamMeterSource", "kParamTapeHiss", "kParamVinylNoise",
]

EXPECTED_DEFAULTS = [
    0.0, 0.5, 0.0, 1.0,
    1.0, 1.0 / 3.0, 0.25, 0.10,
    0.0, 0.20,
    0.0, 0.20, 0.5, 0.9,
    0.0, 0.15, 0.5,
    0.0, 0.25, 0.0,
    0.5, 0.5, 0.0,
    0.0, 0.5, 0.5,
    0.5, 1.0, 0.0, 0.0,
]

EXPECTED_GUI = {
    "VUSourceSelector": "kParamMeterSource",
    "QualitySelector": "kParamQuality",
    "InputRefSelector": "kParamCalibration",
    "ConsoleModeSelector": "kParamConsoleMode",
    "TubeVoiceSelector": "kParamTubeType",
    "TapeSpeedSelector": "kParamTapeSpeed",
    "Bypass": "kParamBypass",
    "ConsolePower": "kParamConsoleOn",
    "TubePower": "kParamTubeOn",
    "TapePower": "kParamTapeOn",
    "GluePower": "kParamGlueOn",
    "VinylPower": "kParamVinylOn",
    "LevelMatch": "kParamAutoGain",
    "Input": "kParamInput",
    "ConsoleDrive": "kParamConsoleDrive",
    "Crosstalk": "kParamConsoleCrosstalk",
    "ConsoleNoise": "kParamConsoleNoise",
    "TubeAmount": "kParamTubeAmount",
    "TapeAmount": "kParamTapeAmount",
    "TapeStability": "kParamTapeStability",
    "TapeHiss": "kParamTapeHiss",
    "GlueAmount": "kParamGlueAmount",
    "GlueCharacter": "kParamGlueCharacter",
    "VinylCharacter": "kParamVinylCharacter",
    "VinylWear": "kParamVinylWear",
    "VinylNoise": "kParamVinylNoise",
    "Depth": "kParamDepth",
    "Width": "kParamWidth",
    "LowMono": "kParamLowMono",
    "Output": "kParamOutput",
}

EXPECTED_SELECTORS = {
    "VUSourceSelector": ("kParamMeterSource", ["INPUT", "OUTPUT"]),
    "QualitySelector": ("kParamQuality", ["ECO 1x", "NORMAL 2x", "HIGH 4x"]),
    "InputRefSelector": ("kParamCalibration", ["-18", "-14", "-10"]),
    "ConsoleModeSelector": ("kParamConsoleMode", ["CLEAN", "CLASSIC", "VINTAGE", "MODERN"]),
    "TubeVoiceSelector": ("kParamTubeType", ["SOFT", "BALANCED", "HOT"]),
    "TapeSpeedSelector": ("kParamTapeSpeed", ["7.5", "15", "30"]),
}

EXPECTED_MIXFX = {
    "mixFxBypass": "kParamBypass",
    "mixFxInput": "kParamInput",
    "mixFxCalibration": "kParamCalibration",
    "mixFxAutoGain": "kParamAutoGain",
    "mixFxConsoleOn": "kParamConsoleOn",
    "mixFxConsoleMode": "kParamConsoleMode",
    "mixFxConsoleDrive": "kParamConsoleDrive",
    "mixFxCrosstalk": "kParamConsoleCrosstalk",
    "mixFxTubeOn": "kParamTubeOn",
    "mixFxTubeAmount": "kParamTubeAmount",
    "mixFxTubeType": "kParamTubeType",
    "mixFxTapeOn": "kParamTapeOn",
    "mixFxTapeAmount": "kParamTapeAmount",
    "mixFxTapeSpeed": "kParamTapeSpeed",
    "mixFxTapeStability": "kParamTapeStability",
    "mixFxGlueOn": "kParamGlueOn",
    "mixFxGlueAmount": "kParamGlueAmount",
    "mixFxGlueCharacter": "kParamGlueCharacter",
    "mixFxVinylOn": "kParamVinylOn",
    "mixFxVinylCharacter": "kParamVinylCharacter",
    "mixFxVinylWear": "kParamVinylWear",
    "mixFxDepth": "kParamDepth",
    "mixFxWidth": "kParamWidth",
    "mixFxLowMono": "kParamLowMono",
    "mixFxConsoleNoise": "kParamConsoleNoise",
    "mixFxTapeHiss": "kParamTapeHiss",
    "mixFxVinylNoise": "kParamVinylNoise",
    "mixFxQuality": "kParamQuality",
    "mixFxOutput": "kParamOutput",
    "mixFxMeterSource": "kParamMeterSource",
}


def fail(message):
    raise SystemExit("PARAMETER CONTRACT FAIL: " + message)


# 1) Parameter IDs are preset/state ABI. Never reorder them.
enum_match = re.search(r"enum ParamIDs[^\{]*\{(.*?)kParamCount", ids_text, re.S)
if not enum_match:
    fail("ParamIDs enum not found")
items = []
value = -1
for part in enum_match.group(1).split(","):
    m = re.search(r"\b(kParam\w+)\b(?:\s*=\s*(\d+))?", part)
    if not m:
        continue
    value = int(m.group(2)) if m.group(2) else value + 1
    items.append((m.group(1), value))
if [x[0] for x in items] != EXPECTED_PARAMS:
    fail("stored parameter order changed")
if [x[1] for x in items] != list(range(len(EXPECTED_PARAMS))):
    fail("stored parameter numeric IDs changed")
if "kParamMeterL = 1000" not in ids_text:
    fail("meter telemetry ID base changed")

# 2) Processor defaults must stay aligned with parameter IDs.
defaults_match = re.search(r"constexpr double kDefaults\[kParamCount\]\s*=\s*\{(.*?)\};", processor, re.S)
if not defaults_match:
    fail("kDefaults not found")
raw_defaults = [x.strip() for x in defaults_match.group(1).split(",") if x.strip()]
try:
    defaults = [float(eval(x, {"__builtins__": {}}, {})) for x in raw_defaults]
except Exception as exc:
    fail(f"could not parse kDefaults: {exc}")
if len(defaults) != len(EXPECTED_DEFAULTS):
    fail("kDefaults count does not match kParamCount")
for i, (actual, expected) in enumerate(zip(defaults, EXPECTED_DEFAULTS)):
    if abs(actual - expected) > 1.0e-12:
        fail(f"default mismatch for {EXPECTED_PARAMS[i]}: {actual} != {expected}")

# 3) Controller defaults must match processor defaults.
controller_defaults = {}
for m in re.finditer(r"addToggle\(parameters,.*?,(kParam\w+),([0-9.]+)\s*\);", controller):
    controller_defaults[m.group(1)] = float(m.group(2))
for m in re.finditer(
    r"addRange\(parameters,.*?,(kParam\w+),(-?[0-9.]+),(-?[0-9.]+),(-?[0-9.]+),",
    controller,
):
    lo, hi, plain = map(float, m.group(2, 3, 4))
    controller_defaults[m.group(1)] = (plain - lo) / (hi - lo)
for m in re.finditer(r"setParamNormalized\((kParam\w+),\s*([0-9.]+|1\./3\.)\)", controller):
    controller_defaults[m.group(1)] = 1.0 / 3.0 if m.group(2) == "1./3." else float(m.group(2))
controller_defaults["kParamBypass"] = 0.0
legacy_crosstalk = 'new RangeParameter(STR16("Legacy Crosstalk"),kParamConsoleCrosstalk,STR16("%"),0.0,100.0,10.0,0,ParameterInfo::kIsHidden)'
if legacy_crosstalk not in controller:
    fail("legacy Crosstalk state slot is not registered as hidden")
controller_defaults["kParamConsoleCrosstalk"] = 0.10
for param, expected in zip(EXPECTED_PARAMS, EXPECTED_DEFAULTS):
    if param not in controller_defaults:
        fail(f"controller default missing for {param}")
    if abs(controller_defaults[param] - expected) > 1.0e-12:
        fail(f"controller/processor default drift for {param}")

# VST3 ParameterInfo.defaultNormalized must match the runtime defaults too.
# StringListParameter defaults are 0 unless explicitly written into ParameterInfo.
if "p->getInfo().defaultNormalizedValue=std::clamp(d,0.0,1.0);" not in controller:
    fail("StringList/toggle VST3 metadata default helper missing")
for needle in [
    "setListDefault(calibration,0.0)",
    "setListDefault(cm,1./3.)",
    "setListDefault(tt,.5)",
    "setListDefault(ts,.5)",
    "setListDefault(q,.5)",
    "setListDefault(meterSource,1.0)",
]:
    if needle not in controller:
        fail(f"VST3 StringList metadata default missing: {needle}")

# 4) Every user-facing custom control must be wired to exactly the intended ID.
gui_map = {}
for m in re.finditer(r'(?:knob|toggle|selector)\("([^"]+)",\s*(kParam\w+)', controller):
    gui_map[m.group(1)] = m.group(2)
if gui_map != EXPECTED_GUI:
    missing = {k: v for k, v in EXPECTED_GUI.items() if gui_map.get(k) != v}
    extra = {k: v for k, v in gui_map.items() if k not in EXPECTED_GUI}
    fail(f"GUI routing changed; wrong/missing={missing}, extra={extra}")
expected_gui_params = set(EXPECTED_PARAMS)
if set(gui_map.values()) != expected_gui_params:
    fail("user-facing GUI parameter coverage changed")
if len(gui_map) != len(set(gui_map.values())):
    fail("user-facing GUI contains duplicate parameter IDs")

# 5) Selector order is semantic, not cosmetic.
for view, (param, labels) in EXPECTED_SELECTORS.items():
    labels_src = ",".join(f'"{x}"' for x in labels)
    needle = f'selector("{view}",{param},{{{labels_src}}})'
    if needle not in controller:
        fail(f"selector order/labels changed for {view}")

# 6) Mix FX atomics must mirror the same IDs one-for-one.
sync_match = re.search(r"void Processor::syncMixFxTargets\(\)\{(.*?)\n\}", processor, re.S)
if not sync_match:
    fail("syncMixFxTargets not found")
mixfx_map = {
    m.group(1): m.group(2)
    for m in re.finditer(r"(mixFx\w+)_\.store\(params_\[(kParam\w+)\]", sync_match.group(1))
}
if mixfx_map != EXPECTED_MIXFX:
    wrong = {k: v for k, v in EXPECTED_MIXFX.items() if mixfx_map.get(k) != v}
    fail(f"Mix FX parameter routing changed: {wrong}")

# 7) Legacy state migration must remain symmetrical in processor and controller.
migration_needles = [
    "id==kParamTubeType",
    "id==kParamMeterSource",
    "id==kParamTapeHiss",
    "id==kParamVinylNoise",
]
for needle in migration_needles:
    if needle not in processor or needle not in controller:
        fail(f"state migration case missing: {needle}")
if "for(ParamID id=0;id<kParamCount;++id)" not in processor:
    fail("processor state/parameter loop no longer spans kParamCount")
if "for(ParamID id=0;id<kParamCount;++id)" not in controller:
    fail("controller state loop no longer spans kParamCount")

# 8) Critical DSP routing checks.
critical = [
    "outputSource?outputMeter_:inputMeter_",
    "mixFxMeterSource_.load(std::memory_order_relaxed)>=0.5",
    "widthGain=2.0*std::clamp",
    "mixFxWidth_.load",
    "params_[kParamWidth]",
    "mixFxDepth_.load",
    "params_[kParamDepth]",
    "mixFxLowMono_.load",
    "params_[kParamLowMono]",
    "processStereoFieldSample",
    "stereoDepthGain(depthBipolar)",
    "stereoOnePoleCoefficient(2000.0,sampleRate_)",
]
for needle in critical:
    if needle not in processor:
        fail(f"critical DSP routing token missing: {needle}")
# Crosstalk is a real Mix FX-only channel-to-channel feature. The standard
# Channel build keeps ID 7 hidden for state ABI compatibility and must not use it.
for needle in (
    "mixFxCrosstalk_.store(params_[kParamConsoleCrosstalk]",
    "captureMixFxInputSnapshot",
    "mixFxCrosstalkSource",
    "targetIndex-1,targetIndex+1",
    "crosstalk>0.0",
):
    if needle not in processor:
        fail(f"true Mix FX Crosstalk routing missing: {needle}")
for needle in ('knob("Crosstalk",kParamConsoleCrosstalk', 'label("LabelCrosstalk","CROSSTALK"'):
    if needle not in controller:
        fail(f"Mix FX Crosstalk GUI mapping missing: {needle}")
normal_marker = "tresult PLUGIN_API Processor::process(ProcessData& data)"
normal_tail = processor[processor.find(normal_marker):]
if not normal_tail:
    fail("standard process() path missing")
if "params_[kParamConsoleCrosstalk]" in normal_tail:
    fail("standard Channel process() unexpectedly uses Crosstalk")

if processor.count("processStereoFieldSample(") != 2:
    fail("normal and Mix FX paths are not both using the shared stereo stage")
if "2500.0/sampleRate_" in processor or "0.25*depthBipolar" in processor:
    fail("legacy weak DEPTH equation returned")
if "return processTubeModelV2(x,state,type,amount,effectiveSampleRate);" not in processor:
    fail("live V2 Tube path is no longer routed through the tested stateful core")
if "TubeChannelState& state" not in processor_h or "TubeModelState" not in processor_h:
    fail("V2 Tube state is not part of the shared processor architecture")
if processor.count("outBus.silenceFlags=0;") < 2:
    fail("normal and Mix FX paths are not both conservative about output silence metadata")

# 9) VU telemetry and face must use one calibrated non-linear scale.
if "vuScaleNormalizedFromDb" not in metering:
    fail("shared calibrated VU scale mapping missing")
for needle in [
    "vuScaleNormalizedFromDb(static_cast<double>(db))",
    "vuScaleNormalizedFromDb(mark.db)",
    "meterStartDeg+meterSweepDeg*value",
    "std::atan2(tip.y-pivot.y,tip.x-pivot.x)",
    "cy+std::sin(scaleAngle)*targetRadius",
]:
    if needle not in hardware:
        fail(f"VU face/needle calibration token missing: {needle}")
if "218.0+104.0*value" in hardware:
    fail("legacy mismatched VU needle angle returned")

print(f"Parameter/routing contract PASS: {len(EXPECTED_PARAMS)} stored parameters, "
      f"{len(EXPECTED_GUI)} GUI controls, {len(EXPECTED_MIXFX)} Mix FX mirrors")
