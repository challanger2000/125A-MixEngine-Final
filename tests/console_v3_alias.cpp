#include "console_v3_model.h"
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

std::vector<double> render(int channels,double frequency,double amplitude,double drive,int mode){
    std::vector<double> y(count);
    for(int n=0;n<count;++n){
        const double x=amplitude*std::sin(2.0*pi*frequency*double(n)/sr);
        const double per=x/double(channels);
        double encoded=0.0;
        for(int ch=0;ch<channels;++ch)
            encoded+=MixEngine::V3Research::consoleV3EncodeFast(per,drive,mode);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encoded,drive,mode);
    }
    return y;
}

double toneMag(const std::vector<double>& y,double f){
    long double re=0.0,im=0.0;long long nCount=0;
    for(int n=warm;n<count;++n){
        const double ph=2.0*pi*f*double(n)/sr;
        re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
        im-=y[static_cast<std::size_t>(n)]*std::sin(ph);
        ++nCount;
    }
    return nCount?2.0*std::sqrt(double(re*re+im*im))/double(nCount):0.0;
}

bool finite(const std::vector<double>& y){
    return std::all_of(y.begin(),y.end(),[](double v){return std::isfinite(v);});
}
}

int main(){
    bool ok=true;
    // 15 kHz -> third-order nonlinear energy folds from 45 kHz to 3 kHz at 48 kHz.
    constexpr double f=15000.0;
    constexpr double alias=3000.0;
    constexpr double amp=0.70;

    for(int mode=0;mode<4;++mode){
        const auto one=render(1,f,amp,0.80,mode);
        const auto eight=render(8,f,amp,0.80,mode);
        const auto thirtyTwo=render(32,f,amp,0.80,mode);
        if(!(finite(one)&&finite(eight)&&finite(thirtyTwo))){ok=false;continue;}

        const double oneFund=toneMag(one,f),oneAlias=toneMag(one,alias);
        const double eFund=toneMag(eight,f),eAlias=toneMag(eight,alias);
        const double tFund=toneMag(thirtyTwo,f),tAlias=toneMag(thirtyTwo,alias);
        const auto db=[](double a,double b){return 20.0*std::log10(std::max(a,1e-30)/std::max(b,1e-30));};

        std::cout<<"mode="<<mode
                 <<" singleAlias="<<db(oneAlias,oneFund)<<" dBc"
                 <<" eightAlias="<<db(eAlias,eFund)<<" dBc"
                 <<" thirtyTwoAlias="<<db(tAlias,tFund)<<" dBc\n";

        // Single-channel paired encode/decode should remain numerically transparent.
        if(!(oneFund>1.0e-6&&oneAlias<oneFund*1.0e-8))ok=false;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 alias measurement harness\n";
    return ok?0:1;
}
