#pragma once
#include "oversampling.h"
#include "latency_alignment.h"
#include <algorithm>
#include <cmath>

namespace MixEngine::V3Research {

constexpr double kTubePi=3.14159265358979323846;

struct TubeV3Character {
    double drive;
    double baseBias;
    double asymmetry;
    double gridThreshold;
    double blockingDepth;
    double recoveryMs;
    double millerCutoffHz;
    double secondStage;
};

inline TubeV3Character tubeV3Character(double type) noexcept {
    static constexpr TubeV3Character soft{
        1.45,0.020,0.010,0.72,0.040,42.0,30000.0,0.10
    };
    static constexpr TubeV3Character balanced{
        1.85,0.036,0.022,0.64,0.065,58.0,24000.0,0.16
    };
    static constexpr TubeV3Character hot{
        2.35,0.058,0.038,0.54,0.095,82.0,18500.0,0.24
    };
    const double p=2.0*std::clamp(type,0.0,1.0);
    const bool upper=p>=1.0;
    const double t=upper?p-1.0:p;
    const auto& a=upper?balanced:soft;
    const auto& b=upper?hot:balanced;
    const auto L=[t](double x,double y){return x+(y-x)*t;};
    return {L(a.drive,b.drive),L(a.baseBias,b.baseBias),L(a.asymmetry,b.asymmetry),
            L(a.gridThreshold,b.gridThreshold),L(a.blockingDepth,b.blockingDepth),
            L(a.recoveryMs,b.recoveryMs),L(a.millerCutoffHz,b.millerCutoffHz),
            L(a.secondStage,b.secondStage)};
}

struct TubeV3State {
    double gridCharge=0.0;
    double cathodeBias=0.0;
    double millerState=0.0;
    double couplingDc=0.0;
    OversamplingEngine oversampler{};
    LatencyAligner dryAligner{};
    int oversamplingFactor=1;

    void reset() noexcept {
        gridCharge=cathodeBias=millerState=couplingDc=0.0;
        oversampler.reset();
        dryAligner.reset();
        oversamplingFactor=1;
    }
};

inline double processTubeV3(double x,
                            TubeV3State& s,
                            double sampleRate,
                            double type,
                            double amount,
                            int osFactor) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(a<=0.0)return x;

    const auto ch=tubeV3Character(type);
    const double creative=a*a*a*a;

    // Coupling-cap / Miller region. The low-frequency DC estimate is removed
    // very gently, while the high-frequency pole becomes more influential as
    // the stage is driven harder.
    const double dcCoeff=1.0-std::exp(-2.0*kTubePi*7.0/sampleRate);
    s.couplingDc+=dcCoeff*(x-s.couplingDc);
    const double ac=x-s.couplingDc;

    const double cutoff=std::min(ch.millerCutoffHz*(1.0-0.18*a),sampleRate*0.45);
    const double hfCoeff=1.0-std::exp(-2.0*kTubePi*cutoff/sampleRate);
    s.millerState+=hfCoeff*(ac-s.millerState);
    const double frequencyShaped=s.millerState;

    osFactor=OversamplingEngine::sanitiseFactor(osFactor);
    if(s.oversamplingFactor!=osFactor){
        s.oversampler.reset();
        s.dryAligner.reset();
        s.oversamplingFactor=osFactor;
    }

    const double recoverySeconds=0.001*ch.recoveryMs;
    const double baseRecovery=1.0-std::exp(-1.0/(std::max(1.0,sampleRate)*recoverySeconds));
    const double osRecovery=1.0-std::pow(std::max(1.0e-12,1.0-baseRecovery),1.0/static_cast<double>(osFactor));

    const double nonlinear=s.oversampler.process(frequencyShaped,osFactor,[&](double v){
        // Positive grid conduction charges the coupling network. The resulting
        // negative bias shift recovers over tens of milliseconds: a compact,
        // bounded blocking-distortion model rather than a static waveshaper.
        const double driven=v*(1.0+(ch.drive-1.0)*(0.20+0.80*a)+0.28*creative);
        const double conduction=std::max(0.0,driven-ch.gridThreshold);
        const double chargeAttack=0.18+0.34*a;
        s.gridCharge+=chargeAttack*(conduction-s.gridCharge);
        s.gridCharge+=osRecovery*(0.0-s.gridCharge);

        const double targetBias=-ch.blockingDepth*a*s.gridCharge;
        s.cathodeBias+=osRecovery*(targetBias-s.cathodeBias);

        const double bias=ch.baseBias*a+s.cathodeBias;
        const double centered=std::tanh(bias);
        const double slope=std::max(1.0e-9,1.0-centered*centered);
        double y=(std::tanh(driven+bias)-centered)/slope;

        const double d2=driven*driven;
        y+=ch.asymmetry*a*(driven*std::abs(driven))/(1.0+0.55*d2);

        const double stage2Drive=1.0+ch.secondStage*a+0.20*creative;
        const double stage2=std::tanh(y*stage2Drive)/stage2Drive;
        const double density=0.16+0.34*a;
        return y+(stage2-y)*density;
    });

    const double wet=std::clamp(a*(0.28+0.64*a)+0.08*creative,0.0,1.0);
    const double alignedDry=s.dryAligner.process(frequencyShaped,oversamplingBulkDelay(osFactor,1));
    return alignedDry+(nonlinear-alignedDry)*wet;
}

} // namespace MixEngine::V3Research
