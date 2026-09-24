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

std::vector<double> render(int channels,double f,double amp,double drive,int mode,bool prepared){
    std::vector<double> y(count);
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    const auto pair=MixEngine::V3Research::consoleV3PairParameters(drive,mode);
    const double a=std::max(1e-9,pair.encodeStrength),invA=1.0/a,invA2=invA*invA;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double sum=0.0;
        for(int ch=0;ch<channels;++ch)
            sum += prepared
                ? MixEngine::V3Research::consoleV3EncodeAdaaPrepared(per,a,invA,invA2,enc[static_cast<std::size_t>(ch)])
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
double rel(const std::vector<double>& a,const std::vector<double>& b){
    long double e=0,r=0;long long n=0;
    for(int i=warm;i<count;++i){
        const double d=a[static_cast<std::size_t>(i)]-b[static_cast<std::size_t>(i)];
        e+=d*d;r+=a[static_cast<std::size_t>(i)]*a[static_cast<std::size_t>(i)];++n;
    }
    return std::sqrt(double(e/n))/std::max(1e-15,std::sqrt(double(r/n)));
}
double cpuMean(int channels,bool prepared){
    constexpr int block=256,blocks=240,warmBlocks=24;
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    const auto pair=MixEngine::V3Research::consoleV3PairParameters(.8,1);
    const double a=std::max(1e-9,pair.encodeStrength),invA=1.0/a,invA2=invA*invA;
    volatile double sink=0.0;
    std::vector<double> t;t.reserve(blocks-warmBlocks);
    for(int b=0;b<blocks;++b){
        const auto t0=std::chrono::steady_clock::now();
        for(int n=0;n<block;++n){
            const long long s=static_cast<long long>(b)*block+n;
            double sum=0.0;
            for(int ch=0;ch<channels;++ch){
                const double x=.035*std::sin(2*pi*(83.0+7.0*(ch%23))*double(s)/sr+.11*ch)
                              +.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(s)/sr);
                sum += prepared
                    ? MixEngine::V3Research::consoleV3EncodeAdaaPrepared(x,a,invA,invA2,enc[static_cast<std::size_t>(ch)])
                    : MixEngine::V3Research::consoleV3EncodeAdaa(x,.8,1,enc[static_cast<std::size_t>(ch)]);
            }
            sink+=MixEngine::V3Research::consoleV3DecodeAdaa(sum,.8,1,dec);
        }
        const auto t1=std::chrono::steady_clock::now();
        if(b>=warmBlocks)t.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    if(sink==123456.0)std::cerr<<"sink";
    return std::accumulate(t.begin(),t.end(),0.0)/double(t.size());
}
}

int main(){
    bool ok=true;
    constexpr double f=15000,alias=3000,amp=.70;
    for(int mode=0;mode<4;++mode){
        for(int ch:{8,32}){
            const auto exact=render(ch,f,amp,.8,mode,false);
            const auto prep=render(ch,f,amp,.8,mode,true);
            const double ea=dbc(exact,f,alias),pa=dbc(prep,f,alias),rr=rel(exact,prep);
            std::cout<<"mode="<<mode<<" ch="<<ch<<" exactAlias="<<ea
                     <<" preparedAlias="<<pa<<" gap="<<(pa-ea)
                     <<" relativeRms="<<rr<<"\n";
            if(!(pa<=-50.0&&std::abs(pa-ea)<.5&&rr<1e-4))ok=false;
        }
    }
    const double exactCpu=cpuMean(128,false);
    const double prepCpu=cpuMean(128,true);
    std::cout<<"128ch ADAA CPU exact="<<exactCpu<<" prepared="<<prepCpu
             <<" ratio="<<(prepCpu/exactCpu)<<"\n";
    if(!(prepCpu<exactCpu*.75))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 prepared ADAA research\n";
    return ok?0:1;
}
