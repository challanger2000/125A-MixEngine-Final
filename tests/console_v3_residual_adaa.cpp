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
    const auto pair=MixEngine::V3Research::consoleV3PairParameters(drive,mode);
    const double a=std::max(1e-9,pair.encodeStrength),invA=1.0/a,invA2=invA*invA;

    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch){
            if(variant==0){
                encoded+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);
            }else{
                const double residual=MixEngine::V3Research::consoleV3EncodeResidualAdaaPrepared(
                    per,a,invA,invA2,enc[static_cast<std::size_t>(ch)]);
                encoded+=per+residual;
            }
        }
        if(variant==0){
            y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encoded,drive,mode);
        }else{
            const double correction=MixEngine::V3Research::consoleV3CorrectionAdaa(encoded,drive,mode,dec);
            y[static_cast<std::size_t>(n)]=x+correction;
        }
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

struct Stats{double mean=0,p99=0,max=0;};
Stats cpu(int channels,bool residualAdaa){
    constexpr int block=256,blocks=260,warmBlocks=32;
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    const auto pair=MixEngine::V3Research::consoleV3PairParameters(.8,1);
    const double a=std::max(1e-9,pair.encodeStrength),invA=1.0/a,invA2=invA*invA;
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
                if(residualAdaa){
                    encoded+=x+MixEngine::V3Research::consoleV3EncodeResidualAdaaPrepared(
                        x,a,invA,invA2,enc[static_cast<std::size_t>(ch)]);
                }else{
                    encoded+=MixEngine::V3Research::consoleV3EncodeFast(x,.8,1);
                }
            }
            sink+=residualAdaa
                ? linear+MixEngine::V3Research::consoleV3CorrectionAdaa(encoded,.8,1,dec)
                : MixEngine::V3Research::consoleV3Decode(encoded,.8,1);
        }
        const auto t1=std::chrono::steady_clock::now();
        if(b>=warmBlocks)times.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    std::sort(times.begin(),times.end());
    Stats s{};
    s.mean=std::accumulate(times.begin(),times.end(),0.0)/double(times.size());
    s.p99=times[std::min(times.size()-1,static_cast<std::size_t>(std::ceil(.99*times.size())-1))];
    s.max=times.back();
    if(sink==123456.0)std::cerr<<"sink";
    return s;
}
}

int main(){
    bool ok=true;
    constexpr double f=15000.0,alias=3000.0,amp=.70;
    for(int mode=0;mode<4;++mode){
        for(int ch:{8,32}){
            const auto base=render(ch,f,amp,.8,mode,0);
            const auto improved=render(ch,f,amp,.8,mode,1);
            const double b=dbc(base,f,alias),a=dbc(improved,f,alias),reduction=b-a;
            std::cout<<"mode="<<mode<<" ch="<<ch
                     <<" base="<<b<<" dBc residualADAA="<<a
                     <<" reduction="<<reduction<<" dB\n";
            if(!(std::isfinite(a)&&a<=-50.0&&reduction>=14.0))ok=false;
        }
    }

    const auto baseCpu=cpu(128,false);
    const auto adaaCpu=cpu(128,true);
    std::cout<<"128ch base mean/p99/max="<<baseCpu.mean<<"/"<<baseCpu.p99<<"/"<<baseCpu.max
             <<" residualADAA="<<adaaCpu.mean<<"/"<<adaaCpu.p99<<"/"<<adaaCpu.max
             <<" p99Ratio="<<(adaaCpu.p99/std::max(1e-9,baseCpu.p99))<<"\n";
    if(!(adaaCpu.p99<baseCpu.p99*1.45))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 delay-free residual ADAA research\n";
    return ok?0:1;
}
