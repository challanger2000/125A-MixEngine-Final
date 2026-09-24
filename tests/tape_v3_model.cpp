#include "tape_v3_model.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;

double rms(const std::vector<double>& x,int start=0){
    long double s=0.0; long long n=0;
    for(std::size_t i=static_cast<std::size_t>(start);i<x.size();++i){s+=x[i]*x[i];++n;}
    return n?std::sqrt(static_cast<double>(s/n)):0.0;
}

std::vector<double> renderSine(double f,double amp,double speed,double amount,double stability,int os){
    MixEngine::V3Research::TapeV3State st;
    std::vector<double> y(131072);
    for(std::size_t n=0;n<y.size();++n){
        const double x=amp*std::sin(2.0*pi*f*static_cast<double>(n)/sr);
        y[n]=MixEngine::V3Research::processTapeV3(x,st,sr,speed,amount,stability,os);
    }
    return y;
}

double toneMagnitude(const std::vector<double>& y,double f,int harmonic=1){
    const int start=16384;
    long double re=0.0,im=0.0; long long n=0;
    const double w=2.0*pi*f*harmonic/sr;
    for(std::size_t i=start;i<y.size();++i){
        const double ph=w*static_cast<double>(i);
        re+=y[i]*std::cos(ph); im-=y[i]*std::sin(ph); ++n;
    }
    return n?2.0*std::sqrt(static_cast<double>(re*re+im*im))/static_cast<double>(n):0.0;
}

double residualAboveFundamental(const std::vector<double>& y,double f){
    const int start=16384;
    long double ss=0,cc=0,sc=0,sy=0,cy=0;
    for(std::size_t i=start;i<y.size();++i){
        const double p=2*pi*f*i/sr,s=std::sin(p),c=std::cos(p);
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[i];cy+=c*y[i];
    }
    const double det=double(ss*cc-sc*sc);
    const double a=double((sy*cc-cy*sc)/det),b=double((cy*ss-sy*sc)/det);
    long double e=0; long long n=0;
    for(std::size_t i=start;i<y.size();++i){
        const double p=2*pi*f*i/sr;
        const double fit=a*std::sin(p)+b*std::cos(p);
        const double d=y[i]-fit; e+=d*d; ++n;
    }
    return std::sqrt(static_cast<double>(e/n));
}

bool finiteVector(const std::vector<double>& x){
    return std::all_of(x.begin(),x.end(),[](double v){return std::isfinite(v);});
}
}

int main(){
    bool ok=true;

    // Exact 0% neutrality is a non-negotiable V3 contract.
    {
        MixEngine::V3Research::TapeV3State st;
        for(int n=0;n<10000;++n){
            const double x=0.73*std::sin(0.019*n)+0.11*std::sin(0.171*n);
            const double y=MixEngine::V3Research::processTapeV3(x,st,sr,0.5,0.0,0.5,4);
            if(y!=x){ok=false;break;}
        }
        std::cout<<"zero-neutrality "<<(ok?"PASS":"FAIL")<<"\n";
    }

    // Speed must create genuinely different bandwidth/body behaviour.
    const auto slow=renderSine(9000.0,0.35,0.0,0.75,1.0,4);
    const auto fast=renderSine(9000.0,0.35,1.0,0.75,1.0,4);
    const double slow9=toneMagnitude(slow,9000.0),fast9=toneMagnitude(fast,9000.0);
    std::cout<<"speed HF slow="<<slow9<<" fast="<<fast9<<" ratio="<<(fast9/std::max(1e-12,slow9))<<"\n";
    if(!(finiteVector(slow)&&finiteVector(fast)&&fast9>slow9*1.08))ok=false;

    // Nonlinearity must be level-dependent, not a fixed EQ curve.
    const auto low=renderSine(997.0,0.08,0.5,0.75,1.0,4);
    const auto high=renderSine(997.0,0.80,0.5,0.75,1.0,4);
    const double lowResidual=residualAboveFundamental(low,997.0);
    const double highResidual=residualAboveFundamental(high,997.0);
    std::cout<<"level residual low="<<lowResidual<<" high="<<highResidual<<"\n";
    if(!(highResidual>lowResidual*2.0))ok=false;

    // Stability must actually modulate transport timing.
    const auto stable=renderSine(4000.0,0.3,0.5,0.65,1.0,4);
    const auto unstable=renderSine(4000.0,0.3,0.5,0.65,0.0,4);
    long double diff=0.0; long long count=0;
    for(std::size_t i=16384;i<stable.size();++i){const double d=stable[i]-unstable[i];diff+=d*d;++count;}
    const double motionDiff=std::sqrt(static_cast<double>(diff/count));
    std::cout<<"transport motion delta="<<motionDiff<<"\n";
    if(!(motionDiff>1.0e-4))ok=false;

    // Oversampling should reduce high-frequency nonlinear residual/alias burden.
    const auto one=renderSine(15000.0,0.85,0.5,0.95,1.0,1);
    const auto four=renderSine(15000.0,0.85,0.5,0.95,1.0,4);
    const double r1=residualAboveFundamental(one,15000.0);
    const double r4=residualAboveFundamental(four,15000.0);
    std::cout<<"HF nonlinear residual 1x="<<r1<<" 4x="<<r4<<"\n";
    if(!(finiteVector(one)&&finiteVector(four)&&r4<r1*1.02))ok=false;

    // Silence and extreme input must remain finite and decay.
    {
        MixEngine::V3Research::TapeV3State st;
        double peak=0.0,last=0.0;
        for(int n=0;n<120000;++n){
            const double x=n<512?((n&1)?4.0:-4.0):0.0;
            const double y=MixEngine::V3Research::processTapeV3(x,st,sr,0.0,1.0,0.0,4);
            if(!std::isfinite(y)){ok=false;break;}
            peak=std::max(peak,std::abs(y)); if(n>100000)last=std::max(last,std::abs(y));
        }
        std::cout<<"extreme peak="<<peak<<" late="<<last<<"\n";
        if(!(peak<10.0&&last<1.0e-5))ok=false;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": V3 tape research model\n";
    return ok?0:1;
}
