#pragma once
#include <algorithm>
#include <cmath>

namespace MixEngine::V3Research {

constexpr double kVinylModelPi=3.14159265358979323846;

struct VinylV3Physical {
    double grooveRadiusM=0.100;
    double stylusRadiusM=5.0e-6;
    double rpm=33.3333333333333;
};

struct VinylV3State {
    double previousInput=0.0;
    double hfMemory=0.0;
    double dcX=0.0;
    double dcY=0.0;
    bool primed=false;
    void reset() noexcept { previousInput=hfMemory=dcX=dcY=0.0; primed=false; }
};

inline double vinylV3GrooveSpeed(const VinylV3Physical& p) noexcept {
    return std::max(1.0e-6,p.grooveRadiusM*(2.0*kVinylModelPi*p.rpm/60.0));
}

inline double vinylV3TracingCoefficient(const VinylV3Physical& p) noexcept {
    // Small-error expansion of spherical stylus tracing gives distortion
    // proportional to stylus radius and approximately inverse-square with
    // groove speed. The absolute constant is fitted to the geometric oracle
    // at the nominal 100 mm / 5 um / 33 1/3 rpm condition.
    constexpr double referenceRadius=5.0e-6;
    constexpr double referenceGrooveRadius=0.100;
    constexpr double referenceRpm=33.3333333333333;
    constexpr double fittedSeconds=4.72e-6;
    const double referenceSpeed=referenceGrooveRadius*(2.0*kVinylModelPi*referenceRpm/60.0);
    const double speed=vinylV3GrooveSpeed(p);
    return fittedSeconds*(p.stylusRadiusM/referenceRadius)
           *(referenceSpeed*referenceSpeed)/(speed*speed);
}

inline double vinylV3DcBlock(double x,VinylV3State& s,double sampleRate) noexcept {
    const double c=std::exp(-2.0*kVinylModelPi*7.0/std::max(1.0,sampleRate));
    const double y=x-s.dcX+c*s.dcY;
    s.dcX=x; s.dcY=y;
    return y;
}

inline double processVinylV3TracingPrepared(double x,VinylV3State& s,double internalRate,
                                            double coefficient,double dcCoeff,double amount) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(!s.primed){
        s.previousInput=x;
        s.dcX=x;
        s.dcY=x;
        s.primed=true;
        return x;
    }
    if(a<=0.0){s.previousInput=x;return x;}
    const double derivative=(x-s.previousInput)*internalRate;
    s.previousInput=x;
    const double tracing=coefficient*x*derivative;
    const double z=tracing*1.35;
    // The oracle-fit operating region normally stays well below |z|=0.5.
    // Use the fifth-order tanh series there to avoid a transcendental call in
    // the realtime hot path; fall back to exact tanh for strong/pathological
    // excursions so the original bounded-safety behaviour is preserved.
    const double az=std::abs(z);
    const double z2=z*z;
    const double tanhLike=az<=0.5
        ?z*(1.0-z2/3.0+(2.0/15.0)*z2*z2)
        :std::tanh(z);
    const double bounded=tanhLike/1.35;
    const double y=x+a*bounded;
    const double dc=y-s.dcX+dcCoeff*s.dcY;
    s.dcX=y;
    s.dcY=dc;
    return dc;
}

inline double processVinylV3Tracing(double x,VinylV3State& s,double sampleRate,
                                    const VinylV3Physical& p,double amount) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(!s.primed){
        // Prime from the first actual sample so activation/reset never creates
        // a derivative impulse merely because previousInput started at zero.
        s.previousInput=x;
        s.dcX=x;
        s.dcY=x;
        s.primed=true;
        return x;
    }
    if(a<=0.0){s.previousInput=x;return x;}

    // x * dx/dt is the first compact term of the tracing-error surrogate.
    // It creates predominantly second-order, frequency-dependent distortion:
    // stronger at high frequency, with larger stylus radius, and at lower
    // groove speed. This is a realtime surrogate, not a literal stylus solver.
    const double derivative=(x-s.previousInput)*sampleRate;
    s.previousInput=x;
    const double coefficient=vinylV3TracingCoefficient(p);
    const double tracing=coefficient*x*derivative;

    // Keep the geometric term bounded under pathological host/test signals.
    // Match the prepared realtime path so oracle/regression measurements cover
    // the exact production transfer in its normal operating region.
    const double z=tracing*1.35;
    const double az=std::abs(z);
    const double z2=z*z;
    const double tanhLike=az<=0.5
        ?z*(1.0-z2/3.0+(2.0/15.0)*z2*z2)
        :std::tanh(z);
    const double bounded=tanhLike/1.35;
    const double y=x+a*bounded;
    return vinylV3DcBlock(y,s,sampleRate);
}

} // namespace MixEngine::V3Research
