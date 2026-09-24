#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace MixEngine {

// V3 uses distinct class IDs so V2 and V3 can be installed and loaded side by side.
static const Steinberg::FUID kProcessorUID (0xDB6E7D25, 0x92A85E13, 0x877AD5B4, 0xDA4C1D37);
static const Steinberg::FUID kControllerUID (0xD7581724, 0xFC9C5CD5, 0xA1EEAA60, 0xB43EA4C7);

static const Steinberg::FUID kChannelProcessorUID (0xB667A41A, 0x93E15E40, 0x97A44D18, 0xF0989B14);
static const Steinberg::FUID kChannelControllerUID (0xF8A45740, 0xB085582A, 0x8F4AF5EA, 0xE4A9A3BD);

enum ParamIDs : Steinberg::Vst::ParamID {
    kParamBypass = 0,
    kParamInput,
    kParamCalibration,
    kParamAutoGain,

    kParamConsoleOn,
    kParamConsoleMode,
    kParamConsoleDrive,
    kParamConsoleCrosstalk,

    kParamTubeOn,
    kParamTubeAmount,

    kParamTapeOn,
    kParamTapeAmount,
    kParamTapeSpeed,
    kParamTapeStability,

    kParamGlueOn,
    kParamGlueAmount,
    kParamGlueCharacter,

    kParamVinylOn,
    kParamVinylCharacter,
    kParamVinylWear,

    kParamDepth,
    kParamWidth,
    kParamLowMono,

    kParamConsoleNoise,
    kParamQuality,
    kParamOutput,

    // Appended parameters preserve every pre-existing parameter ID/state slot.
    kParamTubeType,
    kParamMeterSource,

    // Appended module-owned noise controls; IDs 0..27 stay unchanged.
    kParamTapeHiss,
    kParamVinylNoise,

    kParamCount
};

// Processor-to-controller display parameters. These are deliberately outside
// kParamCount: they are read-only UI telemetry and are never stored in presets.
enum MeterParamIDs : Steinberg::Vst::ParamID {
    kParamMeterL = 1000,
    kParamMeterR,
    kParamClipL,
    kParamClipR
};

enum ConsoleMode : int32_t {
    kClean = 0,
    kClassic,
    kVintage,
    kModern
};

enum TubeType : int32_t {
    kTube12AU7 = 0,
    kTube12AT7,
    kTube12AX7
};

} // namespace MixEngine
