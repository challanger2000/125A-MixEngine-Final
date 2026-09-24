#include "console_v3_adaa.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int block=256;
constexpr int blocks=240;
constexpr int warmBlocks=32;

struct Stats { double mean=0,p95=0,p99=0,max=0; };
Stats makeStats(std::vector<double> v){
    std::sort(v.begin(),v.end());
    Stats s{};
    if(v.empty()) return s;
    s.mean=std::accumulate(v.begin(),v.end(),0.0)/double(v.size());
    auto q=[&](double p){const auto i=std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1));return v[i];};
    s.p95=q(.95);s.p99=q(.99);s.max=v.back();return s;
}

double runCpu(int channels,bool adaa){
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    volatile double sink=0.0;
    std::vector<double> durations;durations.reserve(blocks-warmBlocks);
    for(int b=0;b<blocks;++b){
        const auto t0=std::chrono::steady_clock::now();
        for(int n=0;n<block;++n){
            const long long sample=static_cast<long long>(b)*block+n;
            double sum=0.0;
            for(int ch=0;ch<channels;++ch){
                const double x=0.035*std::sin(2*pi*(83.0+7.0*(ch%23))*double(sample)/sr+0.11*ch)
                              +0.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(sample)/sr);
                sum += adaa
                    ? MixEngine::V3Research::consoleV3EncodeAdaa(x,0.8,1,enc[static_cast<std::size_t>(ch)])
                    : MixEngine::V3Research::consoleV3EncodeFast(x,0.8,1);
            }
            sink += adaa
                ? MixEngine::V3Research::consoleV3DecodeAdaa(sum,0.8,1,dec)
                : MixEngine::V3Research::consoleV3Decode(sum,0.8,1);
        }
        const auto t1=std::chrono::steady_clock::now();
        if(b>=warmBlocks)durations.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    const auto s=makeStats(std::move(durations));
    std::cout<<(adaa?"ADAA":"BASE")<<" ch="<<channels
             <<" mean/p95/p99/max="<<s.mean<<"/"<<s.p95<<"/"<<s.p99<<"/"<<s.max<<" us\n";
    if(sink==123456.0)std::cerr<<"sink";
    return s.p99;
}

std::vector<double> render(bool adaa,int chunk){
    constexpr int N=65536;
    std::vector<double> y(N);
    std::array<MixEngine::V3Research::ConsoleV3AdaaState,8> enc{};
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    for(int base=0;base<N;base+=chunk){
        const int end=std::min(N,base+chunk);
        for(int n=base;n<end;++n){
            double sum=0.0;
            for(int ch=0;ch<8;++ch){
                const double x=0.045*std::sin(2*pi*(211.0+31*ch)*double(n)/sr+0.07*ch)
                              +0.020*std::sin(2*pi*(4000.0+170*ch)*double(n)/sr);
                sum += adaa
                    ? MixEngine::V3Research::consoleV3EncodeAdaa(x,0.75,2,enc[static_cast<std::size_t>(ch)])
                    : MixEngine::V3Research::consoleV3EncodeFast(x,0.75,2);
            }
            y[static_cast<std::size_t>(n)] = adaa
                ? MixEngine::V3Research::consoleV3DecodeAdaa(sum,0.75,2,dec)
                : MixEngine::V3Research::consoleV3Decode(sum,0.75,2);
        }
    }
    return y;
}

double rmsDiff(const std::vector<double>& a,const std::vector<double>& b,int skip=2048){
    long double e=0;long long n=0;
    for(std::size_t i=static_cast<std::size_t>(skip);i<a.size()&&i<b.size();++i){
        const double d=a[i]-b[i];e+=d*d;++n;
    }
    return n?std::sqrt(double(e/n)):0.0;
}

double bestAlignedResidual(const std::vector<double>& ref,const std::vector<double>& test,int& bestLag){
    double best=1e9;bestLag=0;
    for(int lag=-4;lag<=4;++lag){
        long double e=0,r=0;long long n=0;
        for(int i=4096;i<static_cast<int>(ref.size())-4;++i){
            const int j=i+lag;if(j<0||j>=static_cast<int>(test.size()))continue;
            const double d=ref[static_cast<std::size_t>(i)]-test[static_cast<std::size_t>(j)];
            e+=d*d;r+=ref[static_cast<std::size_t>(i)]*ref[static_cast<std::size_t>(i)];++n;
        }
        const double rel=std::sqrt(double(e/std::max<long long>(1,n)))/std::max(1e-15,std::sqrt(double(r/std::max<long long>(1,n))));
        if(rel<best){best=rel;bestLag=lag;}
    }
    return best;
}
}

int main(){
    bool ok=true;
    const double deadline=1e6*block/sr;
    for(int ch:{2,8,32,64,128}){
        const double base99=runCpu(ch,false);
        const double adaa99=runCpu(ch,true);
        std::cout<<"CPU ch="<<ch<<" p99Ratio="<<(adaa99/std::max(1e-9,base99))
                 <<" deadline="<<deadline<<" us\n";
        // Isolated math kernel: even 128ch ADAA must leave substantial room.
        if(!(adaa99<deadline*0.55))ok=false;
    }

    const auto base=render(false,256);
    const auto adaa256=render(true,256);
    const auto adaa31=render(true,31);
    const auto adaa997=render(true,997);

    const double chunk31=rmsDiff(adaa256,adaa31,0);
    const double chunk997=rmsDiff(adaa256,adaa997,0);
    std::cout<<"ADAA chunk determinism 31="<<chunk31<<" 997="<<chunk997<<"\n";
    if(chunk31!=0.0||chunk997!=0.0)ok=false;

    int lag=0;
    const double aligned=bestAlignedResidual(base,adaa256,lag);
    std::cout<<"ADAA vs base bestLag="<<lag<<" relativeResidual="<<aligned<<"\n";
    if(!std::isfinite(aligned)||std::abs(lag)>1)ok=false;

    // Startup must remain bounded and settle quickly; no giant first-sample event.
    double startupPeak=0.0;
    for(int i=0;i<128;++i)startupPeak=std::max(startupPeak,std::abs(adaa256[static_cast<std::size_t>(i)]));
    double steadyPeak=0.0;
    for(std::size_t i=4096;i<adaa256.size();++i)steadyPeak=std::max(steadyPeak,std::abs(adaa256[i]));
    std::cout<<"ADAA startupPeak="<<startupPeak<<" steadyPeak="<<steadyPeak<<"\n";
    if(!(startupPeak<steadyPeak*2.5+1e-6))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 ADAA production gate\n";
    return ok?0:1;
}
