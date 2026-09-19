#pragma once

#include "pluginterfaces/vst/ivstdataexchange.h"

namespace MixEngine {

// Small realtime telemetry block. It carries only normalized GUI values and never audio.
constexpr Steinberg::Vst::DataExchangeUserContextID kMeterExchangeContext = 0x125A0001u;

struct MeterExchangeData {
    double vuL {0.0};
    double vuR {0.0};
    double clipL {0.0};
    double clipR {0.0};
};

} // namespace MixEngine
