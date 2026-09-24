#include "tape_v3_ja_oracle.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;

struct OracleRender {
    std::vector<double> y;
    bool finite=true;
};

OracleRender render(double baseRate,double freq,double amp,double biasGain,int os=16){
    const double osRate=baseRate*os;
    MixEngine::V3Research::JilesAthertonOracle ja;
    const auto& p=ja.parameters();
    const double hScale=2.5e5;
    const double biasFreq=55000.0;
    double phase=0.0;
    double previous=0.0;
    double lp=0.0;
    const double lpCoeff=1.0-std::exp(-2.0*pi*24000.0/osRate);

    const int baseSamples=32768;
    OracleRender r; r.y.resize(baseSamples);
    for(int n=0;n<baseSamples;++n){
        const double x=amp*std::sin(2.0*pi*freq*n/baseRate);
        for(int s=0;s<os;++s){
            const double t=double(s+1)/double(os);
            const double audio=previous+(x-previous)*t;
            const double bias=biasGain*std::cos(phase);
            phase+=2.0*pi*biasFreq/osRate;
            if(phase>=2.0*pi)phase-=2.0*pi;
            const double M=ja.processField(hScale*(audio+bias),osRate);
            if(!std::isfinite(M)){r.finite=false;return r;}
            lp+=lpCoeff*((M/p.Ms)-lp);
        }
        previous=x;
        r.y[n]=lp;
    }
    return r;
}

double fundamental(const std::vector<double>& y,double sr,double f){
    const int start=8192;
    long double re=0.0,im=0.0; long long n=0;
    for(std::size_t i=start;i<y.size();++i){
        const double ph=2.0*pi*f*double(i)/sr;
        re+=y[i]*std::cos(ph); im-=y[i]*std::sin(ph); ++n;
    }
    return n?2.0*std::sqrt(double(re*re+im*im))/double(n):0.0;
}

double residual(const std::vector<double>& y,double sr,double f){
    const int start=8192;
    long double ss=0,cc=0,sc=0,sy=0,cy=0;
    for(std::size_t i=start;i<y.size();++i){
        const double ph=2.0*pi*f*double(i)/sr,s=std::sin(ph),c=std::cos(ph);
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[i];cy+=c*y[i];
    }
    const double det=double(ss*cc-sc*sc);
    const double a=double((sy*cc-cy*sc)/det),b=double((cy*ss-sy*sc)/det);
    long double e=0; long long n=0;
    for(std::size_t i=start;i<y.size();++i){
        const double ph=2.0*pi*f*double(i)/sr;
        const double fit=a*std::sin(ph)+b*std::cos(ph);
        const double d=y[i]-fit;e+=d*d;++n;
    }
    return n?std::sqrt(double(e/n)):0.0;
}
}

int main(){
    bool ok=true;
    constexpr double sr=48000.0;
    constexpr double f=997.0;

    const auto lowNoBias=render(sr,f,0.05,0.0);
    const auto lowBias=render(sr,f,0.05,5.0);
    const auto hotBias=render(sr,f,0.75,5.0);

    if(!(lowNoBias.finite&&lowBias.finite&&hotBias.finite)) ok=false;

    const double fLow=fundamental(lowBias.y,sr,f);
    const double fHot=fundamental(hotBias.y,sr,f);
    const double rNoBias=residual(lowNoBias.y,sr,f);
    const double rBias=residual(lowBias.y,sr,f);
    const double rHot=residual(hotBias.y,sr,f);

    std::cout<<"JA oracle low fundamental="<<fLow
             <<" hot fundamental="<<fHot
             <<" low residual noBias="<<rNoBias
             <<" bias5="<<rBias
             <<" hot residual="<<rHot<<"\n";

    if(!(fLow>1.0e-6&&fHot>fLow)) ok=false;
    if(!(rHot>rBias)) ok=false;

    // Direct hysteresis-loop area check without bias/playback filtering.
    MixEngine::V3Research::JilesAthertonOracle ja;
    long double area=0.0;
    double prevH=0.0,prevM=0.0;
    const double osRate=768000.0;
    for(int n=0;n<32768;++n){
        const double H=2.2e5*std::sin(2.0*pi*2000.0*n/osRate);
        const double M=ja.processField(H,osRate);
        if(!std::isfinite(M)){ok=false;break;}
        if(n>0) area+=0.5L*(M+prevM)*(H-prevH);
        prevH=H;prevM=M;
    }
    std::cout<<"JA hysteresis loop signed area="<<double(area)<<"\n";
    if(std::abs(double(area))<1.0e6) ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Jiles-Atherton physical tape oracle\n";
    return ok?0:1;
}
