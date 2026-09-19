#include "oversampling.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
constexpr int kCount = 512;

struct Result { int factor=1; int islands=1; int peakIndex=0; double peak=0.0; double energyCentroid=0.0; bool finite=true; };

Result measure(int factor, int islands) {
    std::array<MixEngine::OversamplingEngine,4> engines{};
    std::vector<double> y(kCount,0.0);
    Result r; r.factor=factor; r.islands=islands;
    double energy=0.0, weighted=0.0;
    for(int n=0;n<kCount;++n){
        double v=(n==0)?1.0:0.0;
        for(int i=0;i<islands;++i) v=engines[static_cast<std::size_t>(i)].process(v,factor,[](double s){return s;});
        y[n]=v;
        if(!std::isfinite(v)) r.finite=false;
        const double e=v*v; energy+=e; weighted+=static_cast<double>(n)*e;
    }
    for(int n=0;n<kCount;++n){const double a=std::abs(y[n]);if(a>r.peak){r.peak=a;r.peakIndex=n;}}
    r.energyCentroid=energy>0.0?weighted/energy:0.0;
    return r;
}
}

int main(){
    bool ok=true;
    std::cout<<std::fixed<<std::setprecision(9);
    for(int factor: {1,2,4}){
        int previousPeak=-1;
        for(int islands=1;islands<=4;++islands){
            const auto r=measure(factor,islands);
            std::cout<<factor<<"x, islands="<<islands<<": impulse_peak="<<r.peakIndex
                     <<" samples, peak="<<r.peak<<", energy_centroid="<<r.energyCentroid
                     <<", finite="<<(r.finite?"yes":"no")<<"\n";
            if(!r.finite || r.peak<=0.0) ok=false;
            if(factor==1 && (r.peakIndex!=0 || std::abs(r.peak-1.0)>1.0e-15)) ok=false;
            if(factor>1 && previousPeak>=0 && r.peakIndex<=previousPeak) ok=false;
            previousPeak=r.peakIndex;
        }
    }
    if(!ok){std::cerr<<"FAILED: multi-island impulse latency diagnostic\n";return 1;}
    std::cout<<"PASSED: multi-island impulse latency is finite and increases with each oversampling island\n";
    return 0;
}
