#include "analog_models_v2.h"
#include "oversampling.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr int kCount = 65536;
constexpr int kWarmup = 4096;

using MixEngine::OversamplingEngine;

double fitResidual(const std::vector<double>& y,double frequency) {
    double ss=0.0,cc=0.0,sc=0.0,sy=0.0,cy=0.0;
    for(int n=kWarmup;n<kCount;++n){
        const double p=2.0*kPi*frequency*n/kSampleRate;
        const double s=std::sin(p),c=std::cos(p);
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[n];cy+=c*y[n];
    }
    const double det=ss*cc-sc*sc;
    const double a=(sy*cc-cy*sc)/det;
    const double b=(cy*ss-sy*sc)/det;
    double err=0.0;int count=0;
    for(int n=kWarmup;n<kCount;++n){
        const double p=2.0*kPi*frequency*n/kSampleRate;
        const double fit=a*std::sin(p)+b*std::cos(p);
        const double d=y[n]-fit;
        err+=d*d;++count;
    }
    return std::sqrt(err/static_cast<double>(count));
}

bool tapeEcoExact(int speed,double amount) {
    OversamplingEngine e;
    MixEngine::TapeMagneticState directState,ecoState;
    for(int i=0;i<20000;++i){
        const double x=1.2*std::sin(0.013*i)+0.21*std::sin(0.071*i);
        const double direct=MixEngine::processTapeMagneticV2(x,directState,speed,amount,kSampleRate);
        const double eco=e.process(x,1,[&](double v){
            return MixEngine::processTapeMagneticV2(v,ecoState,speed,amount,kSampleRate);
        });
        if(direct!=eco)return false;
    }
    return true;
}

bool vinylEcoExact(double character,double wear) {
    OversamplingEngine e;
    for(int i=0;i<20000;++i){
        const double x=1.2*std::sin(0.013*i)+0.21*std::sin(0.071*i);
        const double direct=MixEngine::processVinylGrooveV2(x,character,wear);
        const double eco=e.process(x,1,[&](double v){
            return MixEngine::processVinylGrooveV2(v,character,wear);
        });
        if(direct!=eco)return false;
    }
    return true;
}

double tapeResidual(int factor,double frequency,int speed,double amount) {
    OversamplingEngine e;
    MixEngine::TapeMagneticState state;
    std::vector<double> y(kCount);
    for(int n=0;n<kCount;++n){
        const double x=0.92*std::sin(2.0*kPi*frequency*n/kSampleRate);
        y[n]=e.process(x,factor,[&](double v){
            return MixEngine::processTapeMagneticV2(v,state,speed,amount,kSampleRate*static_cast<double>(factor));
        });
        if(!std::isfinite(y[n]))return 1.0e9;
    }
    return fitResidual(y,frequency);
}

double vinylResidual(int factor,double frequency,double character,double wear) {
    OversamplingEngine e;
    std::vector<double> y(kCount);
    for(int n=0;n<kCount;++n){
        const double x=0.92*std::sin(2.0*kPi*frequency*n/kSampleRate);
        y[n]=e.process(x,factor,[&](double v){
            return MixEngine::processVinylGrooveV2(v,character,wear);
        });
        if(!std::isfinite(y[n]))return 1.0e9;
    }
    return fitResidual(y,frequency);
}
}

int main(){
    bool ok=true;

    for(int speed=0;speed<3;++speed)
        for(double amount:{0.0,0.5,1.0}){
            const bool exact=tapeEcoExact(speed,amount);
            std::cout<<"V2 Tape Eco exact speed="<<speed
                     <<" amount="<<amount<<" "<<(exact?"yes":"NO")<<"\n";
            ok=ok&&exact;
        }

    for(const auto p:std::vector<std::pair<double,double>>{{0.0,0.0},{0.5,0.25},{1.0,1.0}}){
        const bool exact=vinylEcoExact(p.first,p.second);
        std::cout<<"V2 Vinyl Eco exact color="<<p.first
                 <<" wear="<<p.second<<" "<<(exact?"yes":"NO")<<"\n";
        ok=ok&&exact;
    }

    // First V2 pass: collect real alias residuals from the live stateful cores.
    // Reject only regressions where oversampling makes residuals materially
    // worse; tighter monotonic limits are set from measured V2 data.
    for(double amount:{0.0,0.5,1.0})
        for(double frequency:{9000.0,15000.0}){
            const double r1=tapeResidual(1,frequency,1,amount);
            const double r2=tapeResidual(2,frequency,1,amount);
            const double r4=tapeResidual(4,frequency,1,amount);
            const bool finite=std::isfinite(r1)&&std::isfinite(r2)&&std::isfinite(r4)
                              &&r1<1.0e8&&r2<1.0e8&&r4<1.0e8;
            const bool sane=finite&&r2<=r1*1.20&&r4<=r1*1.20;
            std::cout<<"V2 Tape amount="<<amount<<" f="<<frequency
                     <<" residual 1x="<<r1<<" 2x="<<r2<<" 4x="<<r4
                     <<" "<<(sane?"PASS":"FAIL")<<"\n";
            ok=ok&&sane;
        }

    for(const auto p:std::vector<std::pair<double,double>>{{0.5,0.25},{1.0,1.0}})
        for(double frequency:{9000.0,15000.0}){
            const double r1=vinylResidual(1,frequency,p.first,p.second);
            const double r2=vinylResidual(2,frequency,p.first,p.second);
            const double r4=vinylResidual(4,frequency,p.first,p.second);
            const bool finite=std::isfinite(r1)&&std::isfinite(r2)&&std::isfinite(r4)
                              &&r1<1.0e8&&r2<1.0e8&&r4<1.0e8;
            const bool sane=finite&&r2<=r1*1.20&&r4<=r1*1.20;
            std::cout<<"V2 Vinyl color="<<p.first<<" wear="<<p.second
                     <<" f="<<frequency<<" residual 1x="<<r1
                     <<" 2x="<<r2<<" 4x="<<r4
                     <<" "<<(sane?"PASS":"FAIL")<<"\n";
            ok=ok&&sane;
        }

    if(!ok){
        std::cerr<<"FAILED: V2 Tape/Vinyl oversampling verification\n";
        return 1;
    }
    std::cout<<"PASSED: V2 Tape/Vinyl Eco equivalence, finite processing and oversampling sanity\n";
    return 0;
}
