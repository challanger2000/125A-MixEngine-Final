#pragma once
#include "oversampling.h"
#include <cmath>
namespace MixEngine {
inline double processTapeOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double shape){if(cur!=factor){e.reset();cur=factor;}return e.process(x,factor,[&](double v){return std::tanh(v*shape)/shape;});}
inline double processVinylOversampledCore(OversamplingEngine& e,int& cur,int factor,double x,double drive){if(cur!=factor){e.reset();cur=factor;}return e.process(x,factor,[&](double v){return std::tanh(v*drive)/drive;});}
}
