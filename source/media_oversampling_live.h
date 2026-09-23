#pragma once
#include "oversampling.h"
#include <algorithm>
#include <cmath>
namespace MixEngine {
inline double processTapeOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double shape){
    if(cur!=factor){e.reset();cur=factor;}
    const double k=std::max(0.0,shape-1.0);
    return e.process(x,factor,[&](double v){const double vv=v*v;return v/std::sqrt(1.0+(1.35*k)*vv);});
}
inline double processVinylOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double drive,double material){
    if(cur!=factor){e.reset();cur=factor;}
    const double m=std::clamp(material,0.0,1.0),bias=0.16*m;
    return e.process(x,factor,[&](double v){
        const double center=std::tanh(bias*drive);
        const double slope=drive*(1.0-center*center);
        const double y=std::tanh((v+bias)*drive)-center;
        return slope>1.0e-12?y/slope:v;
    });
}
}
