#include "vinyl_v3_tracing_oracle.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=192000.0;
constexpr int N=65536;
constexpr int warm=8192;

struct Metrics { double fundamental=0.0; double thd=0.0; bool finite=true; };

Metrics measure(double frequency,const MixEngine::V3Research::VinylTracingParameters& p){
    MixEngine::V3Research::VinylTracingOracle oracle(p);
    std::vector<double> y(N);
    for(int n=0;n<N;++n){
        const double phase=2.0*pi*frequency*double(n)/sr;
        y[static_cast<std::size_t>(n)]=oracle.processSinePhase(phase,frequency);
        if(!std::isfinite(y[static_cast<std::size_t>(n)]))return {0.0,0.0,false};
    }
    auto tone=[&](double f){
        long double re=0.0,im=0.0; long long c=0;
        for(int n=warm;n<N;++n){
            const double ph=2.0*pi*f*double(n)/sr;
            re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
            im-=y[static_cast<std::size_t>(n)]*std::sin(ph); ++c;
        }
        return c?2.0*std::sqrt(double(re*re+im*im))/double(c):0.0;
    };
    const double fundamental=tone(frequency);
    long double h2=0.0;
    for(int h=2;h<=9;++h){
        const double hf=frequency*h;
        if(hf>=0.5*sr)break;
        const double m=tone(hf); h2+=m*m;
    }
    return {fundamental,fundamental>1.0e-18?std::sqrt(double(h2))/fundamental:0.0,true};
}
}

int main(){
    bool ok=true;
    using MixEngine::V3Research::VinylTracingParameters;
    VinylTracingParameters nominal{};

    const auto f1=measure(1000.0,nominal),f5=measure(5000.0,nominal),
               f10=measure(10000.0,nominal),f15=measure(15000.0,nominal);
    std::cout<<"frequency THD 1k="<<f1.thd<<" 5k="<<f5.thd
             <<" 10k="<<f10.thd<<" 15k="<<f15.thd<<"\n";
    if(!(f1.finite&&f5.finite&&f10.finite&&f15.finite))ok=false;
    if(!(f5.thd>f1.thd*2.0&&f10.thd>f5.thd*1.5&&f15.thd>f10.thd*1.2))ok=false;

    auto outer=nominal; outer.grooveRadiusM=0.145;
    auto inner=nominal; inner.grooveRadiusM=0.060;
    const auto out10=measure(10000.0,outer),in10=measure(10000.0,inner);
    std::cout<<"groove-radius THD outer="<<out10.thd<<" inner="<<in10.thd
             <<" ratio="<<(in10.thd/std::max(1.0e-30,out10.thd))<<"\n";
    if(!(out10.finite&&in10.finite&&in10.thd>out10.thd*3.0))ok=false;

    auto fine=nominal; fine.stylusRadiusM=3.0e-6;
    auto coarse=nominal; coarse.stylusRadiusM=10.0e-6;
    const auto fine10=measure(10000.0,fine),coarse10=measure(10000.0,coarse);
    std::cout<<"stylus-radius THD fine="<<fine10.thd<<" coarse="<<coarse10.thd
             <<" ratio="<<(coarse10.thd/std::max(1.0e-30,fine10.thd))<<"\n";
    if(!(fine10.finite&&coarse10.finite&&coarse10.thd>fine10.thd*1.8))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 geometric tracing oracle\n";
    return ok?0:1;
}
