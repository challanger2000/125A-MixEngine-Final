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

inline double consoleV3CosFast(double z) noexcept {
    // 12th-order even Taylor polynomial. |z| is guaranteed <= 1.45 by the
    // console encoder domain; the next omitted term is only a few e-9.
    const double z2=z*z;
    const double z4=z2*z2;
    const double z6=z4*z2;
    const double z8=z4*z4;
    const double z10=z8*z2;
    const double z12=z6*z6;
    return 1.0-z2/2.0+z4/24.0-z6/720.0+z8/40320.0-z10/3628800.0+z12/479001600.0;
}

inline double consoleV3EncodePrimitiveFast(double x,double drive,int mode) noexcept {
    const auto p=consoleV3PairParameters(drive,mode);
    const double a=std::max(1.0e-9,p.encodeStrength);
    const double z=std::clamp(a*x,-1.45,1.45);
    return -consoleV3CosFast(z)/(a*a);
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

inline double consoleV3EncodeAdaaFast(double x,double drive,int mode,ConsoleV3AdaaState& state) noexcept {
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

    const double f1=consoleV3EncodePrimitiveFast(x,drive,mode);
    const double f0=consoleV3EncodePrimitiveFast(previous,drive,mode);
    return (f1-f0)/dx;
}

inline double consoleV3EncodeAdaaPrepared(double x,double a,double invA,double invA2,
                                          ConsoleV3AdaaState& state) noexcept {
    if(!state.encoderInitialised){
        state.previousInput=x;
        state.encoderInitialised=true;
        const double z=std::clamp(a*x,-1.45,1.45);
        return consoleV3SinFast(z)*invA;
    }

    const double previous=state.previousInput;
    state.previousInput=x;
    const double dx=x-previous;
    if(std::abs(dx)<1.0e-8){
        const double z=std::clamp(a*0.5*(x+previous),-1.45,1.45);
        return consoleV3SinFast(z)*invA;
    }

    const double z1=std::clamp(a*x,-1.45,1.45);
    const double z0=std::clamp(a*previous,-1.45,1.45);
    return (consoleV3CosFast(z0)-consoleV3CosFast(z1))*invA2/dx;
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

inline double consoleV3EncodeResidualPrimitivePrepared(double x,double a,double invA2) noexcept {
    const double z=std::clamp(a*x,-1.45,1.45);
    return -consoleV3CosFast(z)*invA2-0.5*x*x;
}

inline double consoleV3EncodeResidualAdaaPrepared(double x,double a,double invA,double invA2,
                                                  ConsoleV3AdaaState& state) noexcept {
    const double z=std::clamp(a*x,-1.45,1.45);
    const double memoryless=consoleV3SinFast(z)*invA-x;
    if(!state.encoderInitialised){
        state.previousInput=x;
        state.encoderInitialised=true;
        return memoryless;
    }

    const double previous=state.previousInput;
    state.previousInput=x;

    // The closed-form primitive below is exact in the unclamped interior.
    // If either sample reaches the safety clamp, fall back to the bounded
    // memoryless residual rather than pretending a wrong antiderivative.
    if(std::abs(a*x)>=1.44 || std::abs(a*previous)>=1.44)
        return memoryless;

    const double dx=x-previous;
    if(std::abs(dx)<1.0e-8){
        const double mid=0.5*(x+previous);
        const double mz=std::clamp(a*mid,-1.45,1.45);
        return consoleV3SinFast(mz)*invA-mid;
    }

    const double f1=consoleV3EncodeResidualPrimitivePrepared(x,a,invA2);
    const double f0=consoleV3EncodeResidualPrimitivePrepared(previous,a,invA2);
    return (f1-f0)/dx;
}

inline double consoleV3CorrectionPrimitiveInterior(double summed,double drive,int mode) noexcept {
    return consoleV3DecodePrimitiveInterior(summed,drive,mode)-0.5*summed*summed;
}

inline double consoleV3CorrectionAdaa(double encodedSum,double drive,int mode,ConsoleV3AdaaState& state) noexcept {
    if(drive<=0.0){
        state.previousEncodedSum=encodedSum;
        state.decoderInitialised=true;
        return 0.0;
    }
    const double currentMemoryless=consoleV3Decode(encodedSum,drive,mode)-encodedSum;
    if(!state.decoderInitialised){
        state.previousEncodedSum=encodedSum;
        state.decoderInitialised=true;
        return currentMemoryless;
    }

    const double previous=state.previousEncodedSum;
    state.previousEncodedSum=encodedSum;

    if(!consoleV3DecodeAdaaInterior(encodedSum,drive,mode) ||
       !consoleV3DecodeAdaaInterior(previous,drive,mode))
        return currentMemoryless;

    const double dx=encodedSum-previous;
    if(std::abs(dx)<1.0e-8){
        const double mid=0.5*(encodedSum+previous);
        return consoleV3Decode(mid,drive,mode)-mid;
    }

    const double f1=consoleV3CorrectionPrimitiveInterior(encodedSum,drive,mode);
    const double f0=consoleV3CorrectionPrimitiveInterior(previous,drive,mode);
    return (f1-f0)/dx;
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
