#include "console_v3_adaa.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int count=131072;
constexpr int warm=8192;

std::vector<double> renderBase(int channels,double f,double amp,double drive,int mode){
    std::vector<double> y(count);
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2.0*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch)
            encoded+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encoded,drive,mode);
    }
    return y;
}

std::vector<double> renderAdaa(int channels,double f,double amp,double drive,int mode){
    std::vector<double> y(count);
    std::vector<MixEngine::V3Research::ConsoleV3AdaaState> enc(static_cast<std::size_t>(channels));
    MixEngine::V3Research::ConsoleV3AdaaState dec;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2.0*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch)
            encoded+=MixEngine::V3Research::consoleV3EncodeAdaa(per,drive,mode,enc[static_cast<std::size_t>(ch)]);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3DecodeAdaa(encoded,drive,mode,dec);
    }
    return y;
}

double toneMag(const std::vector<double>& y,double f){
    long double re=0.0,im=0.0;long long n=0;
    for(int i=warm;i<count;++i){
        const double ph=2.0*pi*f*double(i)/sr;
        re+=y[static_cast<std::size_t>(i)]*std::cos(ph);
        im-=y[static_cast<std::size_t>(i)]*std::sin(ph);
        ++n;
    }
    return n?2.0*std::sqrt(double(re*re+im*im))/double(n):0.0;
}

bool finite(const std::vector<double>& x){
    return std::all_of(x.begin(),x.end(),[](double v){return std::isfinite(v);});
}
}

int main(){
    bool ok=true;
    constexpr double f=15000.0;
    constexpr double alias=3000.0;
    constexpr double amp=0.70;

    for(int mode=0;mode<4;++mode){
        for(int channels:{8,32}){
            const auto base=renderBase(channels,f,amp,0.80,mode);
            const auto adaa=renderAdaa(channels,f,amp,0.80,mode);
            if(!(finite(base)&&finite(adaa))){ok=false;continue;}

            const double baseFund=toneMag(base,f),baseAlias=toneMag(base,alias);
            const double adaaFund=toneMag(adaa,f),adaaAlias=toneMag(adaa,alias);
            const auto dbc=[](double a,double fund){
                return 20.0*std::log10(std::max(a,1e-30)/std::max(fund,1e-30));
            };
            const double baseDb=dbc(baseAlias,baseFund);
            const double adaaDb=dbc(adaaAlias,adaaFund);
            const double reduction=baseDb-adaaDb;

            std::cout<<"mode="<<mode<<" channels="<<channels
                     <<" baseAlias="<<baseDb<<" dBc"
                     <<" adaaAlias="<<adaaDb<<" dBc"
                     <<" reduction="<<reduction<<" dB\n";

            if(!(adaaFund>1.0e-6&&reduction>=12.0&&adaaDb<=-50.0))ok=false;
        }
    }

    {
        std::array<MixEngine::V3Research::ConsoleV3AdaaState,16> enc{};
        MixEngine::V3Research::ConsoleV3AdaaState dec;
        double peak=0.0;
        for(int n=0;n<100000;++n){
            const double x=1.8*std::sin(0.071*n)+0.7*std::sin(0.231*n);
            double sum=0.0;
            for(auto& st:enc)sum+=MixEngine::V3Research::consoleV3EncodeAdaa(x/16.0,1.0,2,st);
            const double y=MixEngine::V3Research::consoleV3DecodeAdaa(sum,1.0,2,dec);
            if(!std::isfinite(y)){ok=false;break;}
            peak=std::max(peak,std::abs(y));
        }
        std::cout<<"ADAA extreme peak="<<peak<<"\n";
        if(!(peak<8.0))ok=false;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 ADAA alias research\n";
    return ok?0:1;
}
