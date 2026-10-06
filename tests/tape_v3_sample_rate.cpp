#include "tape_v3_model.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;

std::vector<double> render(double sr,double f,double amp,double speed,double amount,double stability,int os){
    MixEngine::V3Research::TapeV3State st;
    const int count=65536;
    std::vector<double> y(static_cast<std::size_t>(count));
    for(int n=0;n<count;++n){
        const double x=amp*std::sin(2.0*pi*f*double(n)/sr);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::processTapeV3(x,st,sr,speed,amount,stability,os);
    }
    return y;
}

bool finite(const std::vector<double>& x){
    return std::all_of(x.begin(),x.end(),[](double v){return std::isfinite(v);});
}

double tone(const std::vector<double>& y,double sr,double f){
    const int start=8192;
    long double re=0.0,im=0.0; long long n=0;
    for(std::size_t i=start;i<y.size();++i){
        const double p=2.0*pi*f*double(i)/sr;
        re+=y[i]*std::cos(p); im-=y[i]*std::sin(p); ++n;
    }
    return n?2.0*std::sqrt(double(re*re+im*im))/double(n):0.0;
}

double residual(const std::vector<double>& y,double sr,double f){
    const int start=8192;
    long double ss=0,cc=0,sc=0,sy=0,cy=0;
    for(std::size_t i=start;i<y.size();++i){
        const double p=2.0*pi*f*double(i)/sr,s=std::sin(p),c=std::cos(p);
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[i];cy+=c*y[i];
    }
    const double det=double(ss*cc-sc*sc);
    const double a=double((sy*cc-cy*sc)/det),b=double((cy*ss-sy*sc)/det);
    long double e=0.0;long long n=0;
    for(std::size_t i=start;i<y.size();++i){
        const double p=2.0*pi*f*double(i)/sr;
        const double fit=a*std::sin(p)+b*std::cos(p),d=y[i]-fit;
        e+=d*d;++n;
    }
    return n?std::sqrt(double(e/n)):0.0;
}

double diffRms(const std::vector<double>& a,const std::vector<double>& b){
    const int start=8192;
    long double e=0.0;long long n=0;
    for(std::size_t i=start;i<a.size()&&i<b.size();++i){const double d=a[i]-b[i];e+=d*d;++n;}
    return n?std::sqrt(double(e/n)):0.0;
}
}

int main(){
    bool ok=true;
    for(double sr:{44100.0,48000.0,96000.0,192000.0,384000.0}){
        bool local=true;

        // 0% must be sample-exact neutral at every supported rate.
        {
            MixEngine::V3Research::TapeV3State st;
            for(int n=0;n<12000;++n){
                const double x=0.63*std::sin(0.017*n)+0.09*std::sin(0.113*n);
                const double y=MixEngine::V3Research::processTapeV3(x,st,sr,0.5,0.0,0.4,4);
                if(y!=x){local=false;break;}
            }
        }

        // Physical-time transport budget must not shrink/grow with sample rate.
        const int maxSamples=MixEngine::V3Research::tapeV3MaxTransportSamples(sr);
        const double maxSeconds=double(maxSamples)/sr;
        const double target=MixEngine::V3Research::tapeV3TransportCenterSeconds()
                           +MixEngine::V3Research::tapeV3TransportMaxExcursionSeconds();
        if(!(maxSeconds>=target && maxSeconds<=target+1.0/sr+1.0e-12))local=false;

        // Speed character must remain materially distinct.
        const auto slow=render(sr,8000.0,0.35,0.0,0.75,1.0,4);
        const auto fast=render(sr,8000.0,0.35,1.0,0.75,1.0,4);
        const double slow8=tone(slow,sr,8000.0),fast8=tone(fast,sr,8000.0);
        if(!(finite(slow)&&finite(fast)&&fast8>slow8*1.05))local=false;

        // Level dependence must survive rate changes.
        const auto low=render(sr,997.0,0.08,0.5,0.75,1.0,4);
        const auto high=render(sr,997.0,0.80,0.5,0.75,1.0,4);
        const double lowR=residual(low,sr,997.0),highR=residual(high,sr,997.0);
        if(!(finite(low)&&finite(high)&&highR>lowR*2.0))local=false;

        // Stability must remain a real time-domain parameter.
        const auto stable=render(sr,4000.0,0.30,0.5,0.65,1.0,4);
        const auto loose=render(sr,4000.0,0.30,0.5,0.65,0.0,4);
        const double motion=diffRms(stable,loose);
        if(!(finite(stable)&&finite(loose)&&motion>1.0e-4))local=false;

        std::cout<<"sr="<<sr
                 <<" maxTransportSamples="<<maxSamples
                 <<" maxTransportMs="<<(1000.0*maxSeconds)
                 <<" speedRatio="<<(fast8/std::max(1.0e-12,slow8))
                 <<" lowResidual="<<lowR
                 <<" highResidual="<<highR
                 <<" motionDelta="<<motion
                 <<" "<<(local?"PASS":"FAIL")<<"\n";
        ok&=local;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": V3 tape sample-rate matrix\n";
    return ok?0:1;
}
