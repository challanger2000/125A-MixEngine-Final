#pragma once
#include "oversampling.h"
#include "vinyl_v3_model.h"
#include <algorithm>
#include <cmath>
namespace MixEngine {
inline double processTapeOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double shape){
    if(cur!=factor){e.reset();cur=factor;}
    const double k=std::max(0.0,shape-1.0);
    return e.process(x,factor,[&](double v){const double vv=v*v;return v/std::sqrt(1.0+(1.35*k)*vv);});
}
inline double vinylStaticShape(double v,double drive,double material) noexcept {
    const double m=std::clamp(material,0.0,1.0),bias=0.16*m;
    const double center=std::tanh(bias*drive);
    const double slope=drive*(1.0-center*center);
    const double y=std::tanh((v+bias)*drive)-center;
    return slope>1.0e-12?y/slope:v;
}
inline double processVinylOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double drive,double material){
    if(cur!=factor){e.reset();cur=factor;}
    return e.process(x,factor,[&](double v){return vinylStaticShape(v,drive,material);});
}
inline double processVinylV3OversampledCore(OversamplingEngine& e,int& cur,int factor,double x,
                                            V3Research::VinylV3State& tracingState,
                                            double sampleRate,double tracingAmount,
                                            double drive,double material){
    factor=OversamplingEngine::sanitiseFactor(factor);
    if(cur!=factor){
        e.reset();
        tracingState.reset();
        cur=factor;
    }
    const double internalRate=std::max(1.0,sampleRate)*static_cast<double>(factor);
    const V3Research::VinylV3Physical physical{}; // nominal 100 mm / 5 um / 33 1/3 rpm oracle fit
    return e.process(x,factor,[&](double v){
        const double traced=V3Research::processVinylV3Tracing(v,tracingState,internalRate,physical,tracingAmount);
        return vinylStaticShape(traced,drive,material);
    });
}
}
