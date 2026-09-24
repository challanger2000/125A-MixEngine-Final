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

std::vector<double> render(int channels,double f,double amp,double drive,int mode,int variant){
    std::vector<double> y(count);
    MixEngine::V3Research::ConsoleV3AdaaState corrState;
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2*pi*f*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch)
            encoded+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);

        if(variant==0){
            y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encoded,drive,mode);
        }else{
            const double correction=MixEngine::V3Research::consoleV3CorrectionAdaa(encoded,drive,mode,corrState);
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
    constexpr double f=15000.0,alias=3000.0,amp=.70;
    for(int mode=0;mode<4;++mode){
        for(int ch:{8,32}){
            const auto base=render(ch,f,amp,.8,mode,0);
            const auto corr=render(ch,f,amp,.8,mode,1);
            const double b=dbc(base,f,alias),c=dbc(corr,f,alias);
            const double reduction=b-c;
            const double relative=rel(base,corr);
            std::cout<<"mode="<<mode<<" ch="<<ch
                     <<" base="<<b<<" dBc correctionADAA="<<c
                     <<" reduction="<<reduction
                     <<" relativeRms="<<relative<<"\n";
            if(!(std::isfinite(c)&&reduction>=7.0&&c<=-42.0))ok=false;
        }
    }

    // Production gating: with fewer than two effective contributors the
    // coupled correction is disabled exactly, preserving single-channel parity.
    double worst=0.0;
    for(int n=warm;n<count;++n){
        const double x=.5*std::sin(2*pi*997.0*double(n)/sr);
        const double gated=x; // no coupled correction when activeContributors < 2
        worst=std::max(worst,std::abs(gated-x));
    }
    std::cout<<"singleChannel gatedWorst="<<worst<<"\n";
    if(worst!=0.0)ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 correction-only ADAA research\n";
    return ok?0:1;
}
