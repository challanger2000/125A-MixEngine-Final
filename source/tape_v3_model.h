#pragma once
#include "oversampling.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace MixEngine::V3Research {

constexpr double kTapePi = 3.14159265358979323846;

struct TapeV3Character {
    double hfCutHz;
    double headBumpHz;
    double headBumpGain;
    double bias;
    double asymmetry;
    double saturationDrive;
    double wowHz;
    double flutterHz;
};

inline TapeV3Character tapeV3Character(double speed) noexcept {
    static constexpr TapeV3Character slow {10500.0, 64.0, 0.085, 0.055, 0.070, 2.20, 0.42, 5.2};
    static constexpr TapeV3Character mid  {15500.0, 82.0, 0.050, 0.040, 0.045, 1.90, 0.50, 6.0};
    static constexpr TapeV3Character fast {20500.0,105.0, 0.020, 0.028, 0.028, 1.62, 0.58, 6.8};
    const double p=2.0*std::clamp(speed,0.0,1.0);
    const bool upper=p>=1.0;
    const double t=upper?p-1.0:p;
    const auto& a=upper?mid:slow;
    const auto& b=upper?fast:mid;
    const auto lerp=[t](double x,double y){return x+(y-x)*t;};
    return {lerp(a.hfCutHz,b.hfCutHz),lerp(a.headBumpHz,b.headBumpHz),
            lerp(a.headBumpGain,b.headBumpGain),lerp(a.bias,b.bias),
            lerp(a.asymmetry,b.asymmetry),lerp(a.saturationDrive,b.saturationDrive),
            lerp(a.wowHz,b.wowHz),lerp(a.flutterHz,b.flutterHz)};
}

struct TapeV3State {
    std::array<double,512> transport {};
    std::size_t write=0;
    double wowPhase=0.0;
    double flutterPhase=0.0;
    double hfMemory=0.0;
    double bumpLp1=0.0;
    double bumpLp2=0.0;
    double envelope=0.0;
    double magnetic=0.0;
    double dcX=0.0;
    double dcY=0.0;
    OversamplingEngine oversampler {};
    int oversamplingFactor=1;

    void reset() noexcept {
        transport.fill(0.0);
        write=0;
        wowPhase=flutterPhase=0.0;
        hfMemory=bumpLp1=bumpLp2=envelope=magnetic=dcX=dcY=0.0;
        oversampler.reset();
        oversamplingFactor=1;
    }
};

inline double cubic(double y0,double y1,double y2,double y3,double t) noexcept {
    const double a0=-0.5*y0+1.5*y1-1.5*y2+0.5*y3;
    const double a1=y0-2.5*y1+2.0*y2-0.5*y3;
    const double a2=-0.5*y0+0.5*y2;
    return ((a0*t+a1)*t+a2)*t+y1;
}

inline double readTransport(const TapeV3State& s,double delaySamples) noexcept {
    constexpr std::size_t N=512;
    const double d=std::clamp(delaySamples,2.0,500.0);
    const double pos=static_cast<double>(s.write)+static_cast<double>(N)-d;
    const auto wrap=[](long long i){
        constexpr long long n=512;
        i%=n; if(i<0)i+=n; return static_cast<std::size_t>(i);
    };
    const long long i1=static_cast<long long>(std::floor(pos));
    const double f=pos-std::floor(pos);
    return cubic(s.transport[wrap(i1-1)],s.transport[wrap(i1)],
                 s.transport[wrap(i1+1)],s.transport[wrap(i1+2)],f);
}

inline double dcBlockTape(double x,TapeV3State& s,double sampleRate) noexcept {
    const double c=std::exp(-2.0*kTapePi*7.0/sampleRate);
    const double y=x-s.dcX+c*s.dcY;
    s.dcX=x; s.dcY=y;
    return y;
}

inline double processTapeV3(double x,TapeV3State& s,double sampleRate,
                            double speed,double amount,double stability,int osFactor) noexcept {
    const double a=std::clamp(amount,0.0,1.0);
    if(a<=0.0) return x;

    const auto ch=tapeV3Character(speed);
    const double creative=a*a*a*a;
    const double instability=1.0-std::clamp(stability,0.0,1.0);

    s.transport[s.write]=x;
    s.write=(s.write+1)%s.transport.size();

    s.wowPhase+=2.0*kTapePi*ch.wowHz/sampleRate;
    s.flutterPhase+=2.0*kTapePi*ch.flutterHz/sampleRate;
    if(s.wowPhase>=2.0*kTapePi)s.wowPhase-=2.0*kTapePi;
    if(s.flutterPhase>=2.0*kTapePi)s.flutterPhase-=2.0*kTapePi;

    // Real causal variable-delay transport modulation. The base delay makes
    // the read position causal; V3 live integration must report/align it.
    // Keep the transport delay bounded to a fixed sample budget so the research
    // model cannot silently exceed the plug-in's declared latency. The eventual
    // live integration must align this explicitly with kFixedLatencySamples.
    const double maxDelaySamples=std::min(18.0, sampleRate*0.000375);
    const double minDelaySamples=2.0;
    const double excursion=(1.0-std::exp(-2.2*instability))*(maxDelaySamples-minDelaySamples)*0.46;
    const double center=minDelaySamples+excursion;
    const double modulation=0.78*std::sin(s.wowPhase)+0.22*std::sin(s.flutterPhase);
    const double delaySamples=std::clamp(center+excursion*modulation,minDelaySamples,maxDelaySamples);
    const double transported=readTransport(s,delaySamples);

    // Program-dependent compression before magnetics.
    const double attack=std::exp(-1.0/(0.001*2.2*sampleRate));
    const double release=std::exp(-1.0/(0.001*(70.0+45.0*a)*sampleRate));
    const double level=std::abs(transported);
    const double ec=level>s.envelope?attack:release;
    s.envelope=ec*s.envelope+(1.0-ec)*level;
    const double over=std::max(0.0,s.envelope-0.18);
    const double comp=1.0/(1.0+(0.85+0.55*a)*a*over);
    const double compressed=transported*comp;

    // Bias + stateful magnetic stage. Crucially, the primary nonlinear
    // operation itself lives inside the oversampling island.
    const double drive=1.0+(ch.saturationDrive-1.0)*(0.25+0.75*a)+0.35*creative;
    const double bias=ch.bias*a;

    osFactor=OversamplingEngine::sanitiseFactor(osFactor);
    if(s.oversamplingFactor!=osFactor){s.oversampler.reset();s.oversamplingFactor=osFactor;}

    // Convert the base-rate memory coefficient to an oversampled-rate
    // coefficient that preserves approximately the same time constant.
    const double baseMemoryRate=0.16+0.28*a;
    const double osMemoryRate=1.0-std::pow(std::max(1.0e-12,1.0-baseMemoryRate),1.0/static_cast<double>(osFactor));

    const double nonlinear=s.oversampler.process(compressed,osFactor,[&](double v){
        const double target=std::tanh(drive*v+bias+0.24*a*s.magnetic);
        s.magnetic+=osMemoryRate*(target-s.magnetic);
        const double centered=std::tanh(bias);
        const double slope=std::max(1.0e-9,drive*(1.0-centered*centered));
        double y=(target-centered)/slope;
        y+=ch.asymmetry*a*(v*std::abs(v))/(1.0+0.7*v*v);
        y+=0.10*a*(s.magnetic-target);
        const double finalDrive=1.0+0.45*a+0.30*creative;
        const double norm=std::tanh(finalDrive);
        return norm>1.0e-12?std::tanh(y*finalDrive)/norm:y;
    });

    // Speed-dependent HF loss after the magnetic stage.
    const double cutoff=std::min(ch.hfCutHz,sampleRate*0.45);
    const double hc=1.0-std::exp(-2.0*kTapePi*cutoff/sampleRate);
    s.hfMemory+=hc*(nonlinear-s.hfMemory);

    // Two cascaded one-poles provide a broad low-frequency resonant/body term.
    const double bc=1.0-std::exp(-2.0*kTapePi*ch.headBumpHz/sampleRate);
    s.bumpLp1+=bc*(s.hfMemory-s.bumpLp1);
    s.bumpLp2+=bc*(s.bumpLp1-s.bumpLp2);
    const double bumpBand=s.bumpLp1-s.bumpLp2;
    double wet=s.hfMemory+bumpBand*ch.headBumpGain*a*(1.0+0.75*creative);

    wet=dcBlockTape(wet,s,sampleRate);
    const double mix=std::clamp(a*(0.30+0.62*a)+0.08*creative,0.0,1.0);
    return transported+(wet-transported)*mix;
}

} // namespace MixEngine::V3Research
