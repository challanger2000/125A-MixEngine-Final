#pragma once
#include <algorithm>

namespace MixEngine {

struct TubeCharacterCoefficients {
    double gain;
    double bias;
    double asym;
    double secondStage;
    double autoGainDb;
};

struct TapeCharacterCoefficients {
    double cutoffHz;
    double bumpFreqHz;
    double bumpAmount;
    double wowHz;
    double flutterHz;
    double hissTone;
};

inline TubeCharacterCoefficients tubeCharacter(double normalized) noexcept {
    static constexpr TubeCharacterCoefficients a{1.45,0.020,0.012,0.12,1.38};
    static constexpr TubeCharacterCoefficients b{1.85,0.038,0.022,0.18,2.39};
    static constexpr TubeCharacterCoefficients c{2.30,0.060,0.036,0.25,3.31};
    const double p=2.0*std::clamp(normalized,0.0,1.0);
    const bool upper=p>=1.0;
    const double t=upper?p-1.0:p;
    const auto& lo=upper?b:a; const auto& hi=upper?c:b;
    const auto lerp=[t](double x,double y){return x+(y-x)*t;};
    return {lerp(lo.gain,hi.gain),lerp(lo.bias,hi.bias),lerp(lo.asym,hi.asym),
            lerp(lo.secondStage,hi.secondStage),lerp(lo.autoGainDb,hi.autoGainDb)};
}

inline TapeCharacterCoefficients tapeCharacter(double normalized) noexcept {
    static constexpr TapeCharacterCoefficients a{10500.0,64.0,0.050,0.42,5.2,0.80};
    static constexpr TapeCharacterCoefficients b{15000.0,80.0,0.025,0.50,6.0,1.00};
    static constexpr TapeCharacterCoefficients c{19500.0,102.0,0.008,0.58,6.8,1.12};
    const double p=2.0*std::clamp(normalized,0.0,1.0);
    const bool upper=p>=1.0;
    const double t=upper?p-1.0:p;
    const auto& lo=upper?b:a; const auto& hi=upper?c:b;
    const auto lerp=[t](double x,double y){return x+(y-x)*t;};
    return {lerp(lo.cutoffHz,hi.cutoffHz),lerp(lo.bumpFreqHz,hi.bumpFreqHz),
            lerp(lo.bumpAmount,hi.bumpAmount),lerp(lo.wowHz,hi.wowHz),
            lerp(lo.flutterHz,hi.flutterHz),lerp(lo.hissTone,hi.hissTone)};
}

} // namespace MixEngine
