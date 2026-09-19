#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace MixEngine {

static const Steinberg::FUID kProcessorUID (0x4A17C2D3, 0x8E5F41B0, 0x9C2A63E1, 0x715A9B44);
static const Steinberg::FUID kControllerUID (0xB8D2517E, 0x3C6A4D92, 0xA1475E20, 0x6F39C8B1);

static const Steinberg::FUID kChannelProcessorUID (0x0F9EFCF3, 0x80C94604, 0xB58940B8, 0x6D6B13E9);
static const Steinberg::FUID kChannelControllerUID (0xC39D1F81, 0x1058489F, 0x9B925AA3, 0xAB9BBB69);

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
