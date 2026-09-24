#pragma once
#include "console_v3_model.h"
#include <algorithm>
#include <cmath>

namespace MixEngine::V3Research {

struct ConsoleV3AdaaState {
    double previousInput=0.0;
    double previousEncodedSum=0.0;
    bool encoderInitialised=false;
    bool decoderInitialised=false;
    void reset() noexcept {
        previousInput=0.0;
        previousEncodedSum=0.0;
        encoderInitialised=false;
        decoderInitialised=false;
    }
};

inline double consoleV3EncodePrimitive(double x,double drive,int mode) noexcept {
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double z=std::clamp(a*x,-1.45,1.45);
    return -std::cos(z)/(a*a);
}

inline double consoleV3EncodeAdaa(double x,double drive,int mode,ConsoleV3AdaaState& state) noexcept {
    if(drive<=0.0){
        state.previousInput=x;
        state.encoderInitialised=true;
        return x;
    }
    if(!state.encoderInitialised){
        state.previousInput=x;
        state.encoderInitialised=true;
        return consoleV3EncodeFast(x,drive,mode);
    }

    const double previous=state.previousInput;
    state.previousInput=x;
    const double dx=x-previous;
    if(std::abs(dx)<1.0e-8)
        return consoleV3EncodeFast(0.5*(x+previous),drive,mode);

    const double f1=consoleV3EncodePrimitive(x,drive,mode);
    const double f0=consoleV3EncodePrimitive(previous,drive,mode);
    return (f1-f0)/dx;
}

inline double consoleV3DecodePrimitiveInterior(double summed,double drive,int mode) noexcept {
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double z=std::clamp(a*summed,-0.999999,0.999999);
    return (summed*std::asin(z))/a + std::sqrt(std::max(0.0,1.0-z*z))/(a*a);
}

inline bool consoleV3DecodeAdaaInterior(double summed,double drive,int mode) noexcept {
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double limit=0.985*p.busHeadroom;
    return std::abs(a*summed)<0.92*limit;
}

inline double consoleV3DecodeAdaa(double encodedSum,double drive,int mode,ConsoleV3AdaaState& state) noexcept {
    if(drive<=0.0){
        state.previousEncodedSum=encodedSum;
        state.decoderInitialised=true;
        return encodedSum;
    }
    if(!state.decoderInitialised){
        state.previousEncodedSum=encodedSum;
        state.decoderInitialised=true;
        return consoleV3Decode(encodedSum,drive,mode);
    }

    const double previous=state.previousEncodedSum;
    state.previousEncodedSum=encodedSum;

    if(!consoleV3DecodeAdaaInterior(encodedSum,drive,mode) ||
       !consoleV3DecodeAdaaInterior(previous,drive,mode))
        return consoleV3Decode(encodedSum,drive,mode);

    const double dx=encodedSum-previous;
    if(std::abs(dx)<1.0e-8)
        return consoleV3Decode(0.5*(encodedSum+previous),drive,mode);

    const double f1=consoleV3DecodePrimitiveInterior(encodedSum,drive,mode);
    const double f0=consoleV3DecodePrimitiveInterior(previous,drive,mode);
    return (f1-f0)/dx;
}

} // namespace MixEngine::V3Research
