#pragma once
#include <algorithm>
#include <cmath>

namespace MixEngine::V3Research {

constexpr double kConsolePi=3.14159265358979323846;

struct ConsoleV3PairParameters {
    double encodeStrength=0.0;
    double busHeadroom=1.0;
};

inline ConsoleV3PairParameters consoleV3PairParameters(double drive,int mode) noexcept {
    const double d=std::clamp(drive,0.0,1.0);
    const int m=std::clamp(mode,0,3);
    static constexpr double base[4]={0.18,0.30,0.38,0.24};
    static constexpr double span[4]={0.34,0.48,0.56,0.40};
    static constexpr double headroom[4]={1.00,0.96,0.92,0.98};
    return {base[m]+span[m]*d,headroom[m]};
}

inline double consoleV3SinFast(double z) noexcept {
    // 9th-order odd Taylor polynomial. The input is always clamped to +/-1.45,
    // where the next omitted term is below ~2e-6 before normalization.
    const double z2=z*z;
    const double z4=z2*z2;
    const double z6=z4*z2;
    const double z8=z4*z4;
    return z*(1.0-z2/6.0+z4/120.0-z6/5040.0+z8/362880.0);
}

inline double consoleV3EncodeFast(double x,double drive,int mode) noexcept {
    if(drive<=0.0)return x;
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double z=std::clamp(a*x,-1.45,1.45);
    return consoleV3SinFast(z)/a;
}

inline double consoleV3Encode(double x,double drive,int mode) noexcept {
    if(drive<=0.0)return x;
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double z=std::clamp(a*x,-1.45,1.45);
    return std::sin(z)/a;
}

inline double consoleV3Decode(double summed,double drive,int mode) noexcept {
    if(drive<=0.0)return summed;
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    // Headroom is only engaged as the summed bus approaches the asin domain.
    // This keeps one-channel encode/decode essentially complementary while
    // allowing multi-channel interaction to remain bounded.
    double z=a*summed;
    const double limit=0.985*p.busHeadroom;
    if(std::abs(z)>limit){
        const double excess=std::abs(z)-limit;
        z=std::copysign(limit+(1.0-limit)*std::tanh(excess/std::max(1.0e-9,1.0-limit)),z);
    }
    z=std::clamp(z,-0.999999,0.999999);
    return std::asin(z)/a;
}

inline double consoleV3CoupledCorrection(double linearSum,double encodedSum,double drive,int mode) noexcept {
    if(drive<=0.0)return 0.0;
    return consoleV3Decode(encodedSum,drive,mode)-linearSum;
}

inline double consoleV3PairedSingle(double x,double drive,int mode) noexcept {
    return consoleV3Decode(consoleV3Encode(x,drive,mode),drive,mode);
}

} // namespace MixEngine::V3Research
