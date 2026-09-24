#include "tape_v3_ja_oracle.h"
#include <algorithm>\n#include <array>
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
    // Paper gives approximately 5e5 A/m peak-to-peak at the record head.\n    // With bias gain 5, 5e4 A/m per normalized input unit keeps the combined\n    // field in that documented order of magnitude.\n    const double hScale=5.0e4;
    const double biasFreq=55000.0;
    double phase=0.0;
    double previous=0.0;

    const int baseSamples=32768;
    std::vector<double> osMag(static_cast<std::size_t>(baseSamples*os));
    double maxAbsM=0.0;
    std::size_t write=0;
    for(int n=0;n<baseSamples;++n){
        const double x=amp*std::sin(2.0*pi*freq*n/baseRate);
        for(int s=0;s<os;++s){
            const double t=double(s+1)/double(os);
            const double audio=previous+(x-previous)*t;
            const double bias=biasGain*std::cos(phase);
            phase+=2.0*pi*biasFreq/osRate;
            if(phase>=2.0*pi)phase-=2.0*pi;
            const double M=ja.processField(hScale*(audio+bias),osRate);
            if(!std::isfinite(M)){OracleRender bad;bad.finite=false;return bad;}
            maxAbsM=std::max(maxAbsM,std::abs(M));
            osMag[write++]=M/p.Ms;
        }
        previous=x;
    }

    // Physical plausibility gate: an integration that runs to enormous finite
    // values is still a failed oracle. Allow modest dynamic overshoot, not runaway.
    if(maxAbsM>2.0*p.Ms){OracleRender bad;bad.finite=false;return bad;}

    // Offline Blackman-windowed sinc decimator. The oracle contains 55 kHz AC
    // bias, so a one-pole followed by naive sample dropping is not a valid
    // reference path. This FIR keeps the research oracle deliberately expensive
    // and clean; it is not intended for the realtime plug-in.
    constexpr int taps=513;
    constexpr double cutoffHz=20000.0;
    std::array<double,taps> h{};
    const int mid=(taps-1)/2;
    double hSum=0.0;
    for(int i=0;i<taps;++i){
        const int m=i-mid;
        const double fc=cutoffHz/osRate;
        const double sinc=(m==0)?(2.0*fc):(std::sin(2.0*pi*fc*m)/(pi*m));
        const double w=0.42-0.5*std::cos(2.0*pi*i/(taps-1))
                          +0.08*std::cos(4.0*pi*i/(taps-1));
        h[static_cast<std::size_t>(i)]=sinc*w;
        hSum+=h[static_cast<std::size_t>(i)];
    }
    for(auto& v:h)v/=hSum;

    OracleRender r; r.y.assign(baseSamples,0.0);
    for(int n=0;n<baseSamples;++n){
        const long long center=static_cast<long long>(n*os)+mid;
        long double acc=0.0;
        for(int k=0;k<taps;++k){
            const long long idx=center+k-mid;
            if(idx>=0 && idx<static_cast<long long>(osMag.size()))
                acc+=static_cast<long double>(osMag[static_cast<std::size_t>(idx)])*h[static_cast<std::size_t>(k)];
        }
        r.y[static_cast<std::size_t>(n)]=static_cast<double>(acc);
        if(!std::isfinite(r.y[static_cast<std::size_t>(n)])||std::abs(r.y[static_cast<std::size_t>(n)])>2.0){
            r.finite=false;return r;
        }
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

    if(!(fLow>1.0e-5&&fHot>fLow*2.0)) ok=false;
    if(!(rNoBias<0.01&&rBias<0.10&&rHot>rBias*2.0)) ok=false;

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
