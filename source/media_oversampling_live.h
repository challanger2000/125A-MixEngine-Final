#pragma once
#include "oversampling.h"
#include <cmath>
namespace MixEngine {
inline double processTapeOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double shape){if(cur!=factor){e.reset();cur=factor;}const double k=std::max(0.0,shape-1.0);return e.process(x,factor,[&](double v){const double vv=v*v;return v/std::sqrt(1.0+(1.35*k)*vv);});}
inline double processVinylOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double drive){if(cur!=factor){e.reset();cur=factor;}return e.process(x,factor,[&](double v){return std::tanh(v*drive)/drive;});}
}
