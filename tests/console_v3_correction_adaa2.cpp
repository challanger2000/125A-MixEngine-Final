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

std::vector<double> render(int channels,double f,double amp,double drive,int mode,int order){
    std::vector<double> y(count);
    MixEngine::V3Research::ConsoleV3AdaaState state;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch)
            encoded+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);
        if(order==0)y[n]=MixEngine::V3Research::consoleV3Decode(encoded,drive,mode);
        else{
            const double corr=order==1
                ? MixEngine::V3Research::consoleV3CorrectionAdaa(encoded,drive,mode,state)
                : MixEngine::V3Research::consoleV3CorrectionAdaa2(encoded,drive,mode,state);
            y[n]=x+corr;
        }
    }
    return y;
}
double tone(const std::vector<double>& y,double f){
    long double re=0,im=0;long long n=0;
    for(int i=warm;i<count;++i){
        const double p=2*pi*f*double(i)/sr;
        re+=y[i]*std::cos(p); im-=y[i]*std::sin(p); ++n;
    }
    return n?2*std::sqrt(double(re*re+im*im))/double(n):0.0;
}
double dbc(const std::vector<double>& y,double f,double alias){
    return 20*std::log10(std::max(tone(y,alias),1e-30)/std::max(tone(y,f),1e-30));
}
double cpuMeanUs(int channels,int order){
    constexpr int block=256,blocks=220,warmBlocks=24;
    MixEngine::V3Research::ConsoleV3AdaaState state;
    volatile double sink=0.0;
    std::vector<double> times;times.reserve(blocks-warmBlocks);
    for(int b=0;b<blocks;++b){
        const auto t0=std::chrono::steady_clock::now();
        for(int n=0;n<block;++n){
            const long long sample=static_cast<long long>(b)*block+n;
            double linear=0.0,encoded=0.0;
            for(int ch=0;ch<channels;++ch){
                const double x=.035*std::sin(2*pi*(83.0+7.0*(ch%23))*double(sample)/sr+.11*ch)
                              +.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(sample)/sr);
                linear+=x;
                encoded+=MixEngine::V3Research::consoleV3EncodeFast(x,.8,1);
            }
            if(order==0)sink+=MixEngine::V3Research::consoleV3Decode(encoded,.8,1);
            else if(order==1)sink+=linear+MixEngine::V3Research::consoleV3CorrectionAdaa(encoded,.8,1,state);
            else sink+=linear+MixEngine::V3Research::consoleV3CorrectionAdaa2(encoded,.8,1,state);
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
    constexpr double f=15000.0,alias=3000.0,amp=.70;
    for(int mode=0;mode<4;++mode)for(int ch:{8,32}){
        const auto base=render(ch,f,amp,.8,mode,0);
        const auto first=render(ch,f,amp,.8,mode,1);
        const auto second=render(ch,f,amp,.8,mode,2);
        const double b=dbc(base,f,alias),f1=dbc(first,f,alias),f2=dbc(second,f,alias);
        std::cout<<"mode="<<mode<<" ch="<<ch<<" base="<<b<<" first="<<f1
                 <<" second="<<f2<<" firstReduction="<<(b-f1)
                 <<" secondReduction="<<(b-f2)<<" dB\n";
        if(!(std::isfinite(f2)&&f2<=-54.0&&(b-f2)>=17.0))ok=false;
    }
    MixEngine::V3Research::ConsoleV3AdaaState extreme;
    double peak=0.0;
    for(int n=0;n<100000;++n){
        const double x=1.7*std::sin(.071*n)+.8*std::sin(.227*n);
        double encoded=0.0;
        for(int ch=0;ch<16;++ch)encoded+=MixEngine::V3Research::consoleV3EncodeFast(x/16.0,1.0,2);
        const double y=x+MixEngine::V3Research::consoleV3CorrectionAdaa2(encoded,1.0,2,extreme);
        if(!std::isfinite(y)){ok=false;break;} peak=std::max(peak,std::abs(y));
    }
    std::cout<<"secondOrder extremePeak="<<peak<<"\n";
    if(!(peak<8.0))ok=false;
    const double baseCpu=cpuMeanUs(128,0),firstCpu=cpuMeanUs(128,1),secondCpu=cpuMeanUs(128,2);
    std::cout<<"128ch CPU mean base="<<baseCpu<<" first="<<firstCpu<<" second="<<secondCpu
             <<" second/base="<<(secondCpu/std::max(1e-9,baseCpu))<<"\n";
    if(!(secondCpu<baseCpu*1.15))ok=false;
    std::cout<<(ok?"PASS":"REJECTED (non-gating research)")<<": Console V3 second-order correction ADAA research\n";
    return ok?0:1;
}
