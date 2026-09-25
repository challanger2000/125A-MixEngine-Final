#pragma once
#include "character_morph.h"
#include <algorithm>
#include <cmath>

namespace MixEngine {

inline double analogCharacterAmount(double amount) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    return 0.06+0.94*std::pow(a,0.85);
}

inline double creativeZone(double normalized) noexcept {
    const double x=std::clamp(normalized,0.0,1.0);
    const double x2=x*x;
    return x2*x2;
}

inline double consoleSoftClip(double x,double amount) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(a<=0.0)return x;
    const double shape=1.0+a;
    const double norm=std::tanh(shape);
    const double saturated=norm>0.0?std::tanh(x*shape)/norm:x;
    return x+(saturated-x)*a;
}

inline double processConsoleNonlinearCore(double x,double low,double high,int mode,double drive) noexcept {
    const double d=std::clamp(drive,0.0,1.0);
    const double zone=creativeZone(d);
    const double e=d*(1.40-0.40*d)+0.32*zone;
    const double hot=1.0+0.28*zone;
    double y=x;
    switch(mode){
        case 0:
            // Clean: retain the restrained low range, but open the upper half so
            // 75-100 % remains a genuine creative reserve instead of a cosmetic turn.
            y=consoleSoftClip(x,std::min(1.0,0.42*e+0.12*zone));
            break;
        case 1:
            y=consoleSoftClip((x+0.070*e*low)*hot,e)/hot;
            y+=0.018*e*high*std::abs(x);
            // Once the soft-clip amount reaches its natural ceiling, keep the
            // final quarter meaningful with a bounded parallel asymmetry/density term.
            y+=0.032*zone*(x*std::abs(x))/(1.0+0.75*x*x);
            break;
        case 2:
            y=consoleSoftClip((x+0.135*e*low
                +0.050*e*x*x*(x>=0.0?1.0:-0.42))*hot,e)/hot;
            y-=0.040*e*high;
            break;
        default:
            y=consoleSoftClip((x+0.020*e*high)*hot,0.70*e)/hot;
            y+=0.018*e*high*(1.0-std::min(1.0,std::abs(x)));
            break;
    }
    return y;
}

inline double processTubeNonlinearCore(double x,double typeMorph,double amount) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(a<=0.0)return x;
    const double zone=creativeZone(a);
    const double e=a*(1.35-0.35*a)+0.24*zone;
    const auto model=tubeCharacter(typeMorph);

    const double driven=x*(1.0+(model.gain-1.0)*e);
    const double b=model.bias*e;
    const double centered=std::tanh(b);
    const double slope=std::max(1.0e-9,1.0-centered*centered);
    double first=(std::tanh(driven+b)-centered)/slope;

    const double d2=driven*driven;
    first+=model.asym*e*(driven*std::abs(driven))/(1.0+0.65*d2);

    const double stage2Drive=1.0+model.secondStage*e;
    const double second=std::tanh(first*stage2Drive)/stage2Drive;
    const double dense=first+(second-first)*(0.15+0.35*std::min(1.0,e));
    const double wet=std::min(1.0,e*(0.25+0.75*e));
    return x+(dense-x)*wet;
}

// Compatibility overload for FINAL v1.1.0 diagnostics and any internal code
// that still uses the historical 0/1/2 Tube type convention.
inline double processTubeNonlinearCore(double x,int type,double amount) noexcept {
    const int t=std::clamp(type,0,2);
    return processTubeNonlinearCore(x,0.5*static_cast<double>(t),amount);
}

} // namespace MixEngine
