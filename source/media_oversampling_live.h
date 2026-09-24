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
struct VinylStaticShapePrepared {
    double bias=0.0;
    double center=0.0;
    double slope=1.0;
    double drive=1.0;
};
inline VinylStaticShapePrepared prepareVinylStaticShape(double drive,double material) noexcept {
    VinylStaticShapePrepared p;
    p.drive=drive;
    p.bias=0.16*std::clamp(material,0.0,1.0);
    p.center=std::tanh(p.bias*p.drive);
    p.slope=p.drive*(1.0-p.center*p.center);
    return p;
}
inline double vinylStaticShapePrepared(double v,const VinylStaticShapePrepared& p) noexcept {
    const double y=std::tanh((v+p.bias)*p.drive)-p.center;
    return p.slope>1.0e-12?y/p.slope:v;
}
inline double vinylStaticShape(double v,double drive,double material) noexcept {
    return vinylStaticShapePrepared(v,prepareVinylStaticShape(drive,material));
}
inline double processVinylOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double drive,double material){
    if(cur!=factor){e.reset();cur=factor;}
    return e.process(x,factor,[&](double v){return vinylStaticShape(v,drive,material);});
}
inline double processVinylV3OversampledPreparedCore(OversamplingEngine& e,int& cur,int factor,double x,
                                                    V3Research::VinylV3State& tracingState,
                                                    double internalRate,double tracingCoefficient,
                                                    double tracingDcCoeff,double tracingAmount,
                                                    const VinylStaticShapePrepared& shape){
    factor=OversamplingEngine::sanitiseFactor(factor);
    if(cur!=factor){
        e.reset();
        tracingState.reset();
        cur=factor;
    }
    return e.process(x,factor,[&](double v){
        const double traced=V3Research::processVinylV3TracingPrepared(
            v,tracingState,internalRate,tracingCoefficient,tracingDcCoeff,tracingAmount);
        return vinylStaticShapePrepared(traced,shape);
    });
}
inline double processVinylV3OversampledCore(OversamplingEngine& e,int& cur,int factor,double x,
                                            V3Research::VinylV3State& tracingState,
                                            double sampleRate,double tracingAmount,
                                            double drive,double material){
    factor=OversamplingEngine::sanitiseFactor(factor);
    const double internalRate=std::max(1.0,sampleRate)*static_cast<double>(factor);
    const V3Research::VinylV3Physical physical{};
    const double coefficient=V3Research::vinylV3TracingCoefficient(physical);
    const double dcCoeff=std::exp(-2.0*V3Research::kVinylModelPi*7.0/internalRate);
    const auto shape=prepareVinylStaticShape(drive,material);
    return processVinylV3OversampledPreparedCore(
        e,cur,factor,x,tracingState,internalRate,coefficient,dcCoeff,tracingAmount,shape);
}
}
