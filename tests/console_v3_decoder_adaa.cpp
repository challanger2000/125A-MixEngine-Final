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

std::vector<double> render(int channels,double f,double amp,double drive,int mode,int variant){
    std::vector<double> y(count);
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double sum=0.0;
        for(int ch=0;ch<channels;++ch){
            if(variant==2)
                sum+=MixEngine::V3Research::consoleV3EncodeAdaa(per,drive,mode,enc[static_cast<std::size_t>(ch)]);
            else
                sum+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);
        }
        y[static_cast<std::size_t>(n)] = variant==0
            ? MixEngine::V3Research::consoleV3Decode(sum,drive,mode)
            : MixEngine::V3Research::consoleV3DecodeAdaa(sum,drive,mode,dec);
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

double cpuMeanUs(int channels,int variant){
    constexpr int block=256;
    constexpr int blocks=220;
    constexpr int warmBlocks=24;
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
                if(variant==2)sum+=MixEngine::V3Research::consoleV3EncodeAdaa(x,.8,1,enc[static_cast<std::size_t>(ch)]);
                else sum+=MixEngine::V3Research::consoleV3EncodeFast(x,.8,1);
            }
            const double y=variant==0?MixEngine::V3Research::consoleV3Decode(sum,.8,1)
                                    :MixEngine::V3Research::consoleV3DecodeAdaa(sum,.8,1,dec);
            sink+=y;
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
    for(int mode=0;mode<4;++mode){
        for(int channels:{8,32}){
            const auto base=render(channels,f,amp,.8,mode,0);
            const auto decoderOnly=render(channels,f,amp,.8,mode,1);
            const auto full=render(channels,f,amp,.8,mode,2);
            const double b=dbc(base,f,alias),d=dbc(decoderOnly,f,alias),a=dbc(full,f,alias);
            std::cout<<"mode="<<mode<<" ch="<<channels
                     <<" base="<<b<<" dBc decoderADAA="<<d
                     <<" fullADAA="<<a
                     <<" decoderReduction="<<(b-d)
                     <<" gapToFull="<<(d-a)<<" dB\n";
            if(!(d<=-50.0 && (b-d)>=12.0 && d<=a+4.0))ok=false;
        }
    }

    const double baseCpu=cpuMeanUs(128,0);
    const double decoderCpu=cpuMeanUs(128,1);
    const double fullCpu=cpuMeanUs(128,2);
    std::cout<<"128ch CPU mean base="<<baseCpu
             <<" decoderOnly="<<decoderCpu
             <<" full="<<fullCpu
             <<" decoder/base="<<(decoderCpu/baseCpu)
             <<" decoder/full="<<(decoderCpu/fullCpu)<<"\n";
    if(!(decoderCpu<fullCpu*0.85 && decoderCpu<baseCpu*1.25))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 decoder-only ADAA research\n";
    return ok?0:1;
}
