#include "console_v3_adaa.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int block=256;
constexpr int channels=128;
constexpr int blocks=320;
constexpr int warm=40;

struct Stats{double mean=0,p95=0,p99=0,max=0;};
Stats stats(std::vector<double> v){
    std::sort(v.begin(),v.end());Stats s{};
    s.mean=std::accumulate(v.begin(),v.end(),0.0)/double(v.size());
    auto q=[&](double p){return v[std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1))];};
    s.p95=q(.95);s.p99=q(.99);s.max=v.back();return s;
}

Stats run(bool prepared){
    std::vector<double> input(static_cast<std::size_t>(channels*block));
    for(int n=0;n<block;++n)
        for(int ch=0;ch<channels;++ch)
            input[static_cast<std::size_t>(n*channels+ch)]
              =.035*std::sin(2*pi*(83.0+7.0*(ch%23))*double(n)/sr+.11*ch)
              +.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(n)/sr);

    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(channels);
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    const auto pair=MixEngine::V3Research::consoleV3PairParameters(.8,1);
    const double a=std::max(1e-9,pair.encodeStrength),invA=1.0/a,invA2=invA*invA;
    volatile double sink=0.0;
    std::vector<double> times;times.reserve(blocks-warm);
    for(int b=0;b<blocks;++b){
        const auto t0=std::chrono::steady_clock::now();
        for(int n=0;n<block;++n){
            double sum=0.0;
            for(int ch=0;ch<channels;++ch){
                const double x=input[static_cast<std::size_t>(n*channels+ch)];
                sum += prepared
                    ? MixEngine::V3Research::consoleV3EncodeAdaaPrepared(x,a,invA,invA2,enc[static_cast<std::size_t>(ch)])
                    : MixEngine::V3Research::consoleV3EncodeAdaa(x,.8,1,enc[static_cast<std::size_t>(ch)]);
            }
            sink+=MixEngine::V3Research::consoleV3DecodeAdaa(sum,.8,1,dec);
        }
        const auto t1=std::chrono::steady_clock::now();
        if(b>=warm)times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    if(sink==123456.0)std::cerr<<"sink";
    return stats(std::move(times));
}
}

int main(){
    const auto exact=run(false),prep=run(true);
    std::cout<<"exact mean/p95/p99/max="<<exact.mean<<"/"<<exact.p95<<"/"<<exact.p99<<"/"<<exact.max
             <<" prepared="<<prep.mean<<"/"<<prep.p95<<"/"<<prep.p99<<"/"<<prep.max
             <<" meanRatio="<<(prep.mean/exact.mean)
             <<" p99Ratio="<<(prep.p99/exact.p99)<<"\n";
    const bool ok=prep.mean<exact.mean*.75 && prep.p99<exact.p99*.82;
    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 prepared ADAA hot-path CPU\n";
    return ok?0:1;
}
