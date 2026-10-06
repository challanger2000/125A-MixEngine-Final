#include "console_v3_adaa.h"
#include <algorithm>
#include <cmath>
#include <iostream>
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
    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 prepared ADAA quality parity\n";
    return ok?0:1;
}
