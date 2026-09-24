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
constexpr int count=131072;
constexpr int warm=8192;

std::vector<double> render(int channels,double f,double amp,double drive,int mode,bool fast){
    std::vector<double> y(count);
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double sum=0.0;
        for(int ch=0;ch<channels;++ch)
            sum += fast
                ? MixEngine::V3Research::consoleV3EncodeAdaaFast(per,drive,mode,enc[static_cast<std::size_t>(ch)])
                : MixEngine::V3Research::consoleV3EncodeAdaa(per,drive,mode,enc[static_cast<std::size_t>(ch)]);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3DecodeAdaa(sum,drive,mode,dec);
    }
    return y;
}

double tone(const std::vector<double>& y,double f){
    long double re=0,im=0;long long n=0;
    for(int i=warm;i<count;++i){
        const double p=2*pi*f*double(i)/sr;
        re+=y[static_cast<std::size_t>(i)]*std::cos(p);
        im-=y[static_cast<std::size_t>(i)]*std::sin(p);++n;
    }
    return n?2*std::sqrt(double(re*re+im*im))/double(n):0.0;
}

double dbc(const std::vector<double>& y,double f,double alias){
    return 20*std::log10(std::max(tone(y,alias),1e-30)/std::max(tone(y,f),1e-30));
}

double relativeRms(const std::vector<double>& a,const std::vector<double>& b){
    long double e=0,r=0;long long n=0;
    for(int i=warm;i<count;++i){
        const double d=a[static_cast<std::size_t>(i)]-b[static_cast<std::size_t>(i)];
        e+=d*d;r+=a[static_cast<std::size_t>(i)]*a[static_cast<std::size_t>(i)];++n;
    }
    return std::sqrt(double(e/n))/std::max(1e-15,std::sqrt(double(r/n)));
}

double cpuMeanUs(int channels,bool fast){
    constexpr int block=256, blocks=220, warmBlocks=24;
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    volatile double sink=0.0;
    std::vector<double> times;times.reserve(blocks-warmBlocks);
    for(int b=0;b<blocks;++b){
        const auto t0=std::chrono::steady_clock::now();
        for(int n=0;n<block;++n){
            const long long sample=static_cast<long long>(b)*block+n;
            double sum=0.0;
            for(int ch=0;ch<channels;++ch){
                const double x=.035*std::sin(2*pi*(83.0+7.0*(ch%23))*double(sample)/sr+.11*ch)
                              +.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(sample)/sr);
                sum += fast
                    ? MixEngine::V3Research::consoleV3EncodeAdaaFast(x,.8,1,enc[static_cast<std::size_t>(ch)])
                    : MixEngine::V3Research::consoleV3EncodeAdaa(x,.8,1,enc[static_cast<std::size_t>(ch)]);
            }
            sink+=MixEngine::V3Research::consoleV3DecodeAdaa(sum,.8,1,dec);
        }
        const auto t1=std::chrono::steady_clock::now();
        if(b>=warmBlocks)times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    if(sink==123456.0)std::cerr<<"sink";
    return std::accumulate(times.begin(),times.end(),0.0)/double(times.size());
}
}

int main(){
    bool ok=true;

    double worstCos=0.0;
    for(int i=-145000;i<=145000;++i){
        const double z=double(i)/100000.0;
        worstCos=std::max(worstCos,std::abs(std::cos(z)-MixEngine::V3Research::consoleV3CosFast(z)));
    }
    std::cout<<"fast cos worst error="<<worstCos<<"\n";
    if(!(worstCos<5.0e-9))ok=false;

    constexpr double f=15000.0,alias=3000.0,amp=.70;
    for(int mode=0;mode<4;++mode){
        for(int channels:{8,32}){
            const auto exact=render(channels,f,amp,.8,mode,false);
            const auto fast=render(channels,f,amp,.8,mode,true);
            const double exactAlias=dbc(exact,f,alias),fastAlias=dbc(fast,f,alias);
            const double rel=relativeRms(exact,fast);
            std::cout<<"mode="<<mode<<" ch="<<channels
                     <<" exactAlias="<<exactAlias<<" dBc fastAlias="<<fastAlias
                     <<" dBc gap="<<(fastAlias-exactAlias)
                     <<" relativeRms="<<rel<<"\n";
            if(!(fastAlias<=-50.0&&std::abs(fastAlias-exactAlias)<0.5&&rel<1.0e-4))ok=false;
        }
    }

    const double exactCpu=cpuMeanUs(128,false);
    const double fastCpu=cpuMeanUs(128,true);
    std::cout<<"128ch ADAA CPU mean exact="<<exactCpu<<" fast="<<fastCpu
             <<" ratio="<<(fastCpu/exactCpu)<<"\n";
    if(!(fastCpu<exactCpu*0.85))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 fast ADAA encoder research\n";
    return ok?0:1;
}
