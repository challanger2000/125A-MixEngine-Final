#include "tube_v3_model.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int N=131072;
constexpr int warm=16384;

std::vector<double> renderSine(double f,double amp,double type,double amount,int os){
    MixEngine::V3Research::TubeV3State st;
    std::vector<double> y(N);
    for(int n=0;n<N;++n){
        const double x=amp*std::sin(2.0*pi*f*double(n)/sr);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::processTubeV3(x,st,sr,type,amount,os);
    }
    return y;
}

bool finite(const std::vector<double>& y){
    return std::all_of(y.begin(),y.end(),[](double v){return std::isfinite(v);});
}

double fundamental(const std::vector<double>& y,double f){
    long double re=0.0,im=0.0;long long n=0;
    for(int i=warm;i<N;++i){
        const double p=2.0*pi*f*double(i)/sr;
        re+=y[static_cast<std::size_t>(i)]*std::cos(p);
        im-=y[static_cast<std::size_t>(i)]*std::sin(p);
        ++n;
    }
    return n?2.0*std::sqrt(double(re*re+im*im))/double(n):0.0;
}

double residual(const std::vector<double>& y,double f){
    long double ss=0,cc=0,sc=0,sy=0,cy=0;
    for(int i=warm;i<N;++i){
        const double p=2*pi*f*double(i)/sr,s=std::sin(p),c=std::cos(p);
        const double v=y[static_cast<std::size_t>(i)];
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*v;cy+=c*v;
    }
    const double det=double(ss*cc-sc*sc);
    const double a=double((sy*cc-cy*sc)/det),b=double((cy*ss-sy*sc)/det);
    long double e=0.0;long long n=0;
    for(int i=warm;i<N;++i){
        const double p=2*pi*f*double(i)/sr;
        const double fit=a*std::sin(p)+b*std::cos(p);
        const double d=y[static_cast<std::size_t>(i)]-fit;
        e+=d*d;++n;
    }
    return n?std::sqrt(double(e/n)):0.0;
}

double diffRms(const std::vector<double>& a,const std::vector<double>& b,int start){
    long double e=0.0;long long n=0;
    for(int i=start;i<N;++i){
        const double d=a[static_cast<std::size_t>(i)]-b[static_cast<std::size_t>(i)];
        e+=d*d;++n;
    }
    return n?std::sqrt(double(e/n)):0.0;
}
}

int main(){
    bool ok=true;

    // Exact 0% neutrality.
    {
        MixEngine::V3Research::TubeV3State st;
        for(int n=0;n<20000;++n){
            const double x=0.61*std::sin(0.013*n)+0.12*std::sin(0.171*n);
            if(MixEngine::V3Research::processTubeV3(x,st,sr,0.5,0.0,4)!=x){ok=false;break;}
        }
        std::cout<<"tube zero-neutrality "<<(ok?"PASS":"FAIL")<<"\n";
    }

    // Nonlinearity must scale materially with input level.
    const auto low=renderSine(997.0,0.07,0.5,0.75,4);
    const auto hot=renderSine(997.0,0.78,0.5,0.75,4);
    const double lowR=residual(low,997.0),hotR=residual(hot,997.0);
    std::cout<<"tube level residual low="<<lowR<<" high="<<hotR<<"\n";
    if(!(finite(low)&&finite(hot)&&hotR>lowR*3.0))ok=false;

    // Voices must not be cosmetic.
    const auto soft=renderSine(997.0,0.55,0.0,0.72,4);
    const auto hard=renderSine(997.0,0.55,1.0,0.72,4);
    const double voice=diffRms(soft,hard,warm);
    std::cout<<"tube voice endpoint delta="<<voice<<"\n";
    if(!(finite(soft)&&finite(hard)&&voice>1.0e-3))ok=false;

    // Miller behavior must be measured relative to each voice's own low-
    // frequency gain. Hot intentionally has more stage gain, so comparing the
    // absolute 15 kHz outputs would confound gain with bandwidth.
    const auto soft1=renderSine(1000.0,0.03,0.0,0.70,4);
    const auto hot1=renderSine(1000.0,0.03,1.0,0.70,4);
    const auto soft15=renderSine(15000.0,0.03,0.0,0.70,4);
    const auto hot15=renderSine(15000.0,0.03,1.0,0.70,4);
    const double softRatio=fundamental(soft15,15000.0)/std::max(1e-12,fundamental(soft1,1000.0));
    const double hotRatio=fundamental(hot15,15000.0)/std::max(1e-12,fundamental(hot1,1000.0));
    std::cout<<"tube HF normalized soft="<<softRatio<<" hot="<<hotRatio<<"\n";
    if(!(hotRatio<softRatio*0.97))ok=false;

    // Blocking/recovery: same low-level probe after a hot burst must initially
    // differ from a never-overdriven reference, then decay toward it.
    {
        MixEngine::V3Research::TubeV3State driven,reference;
        std::vector<double> a(N),b(N);
        for(int n=0;n<N;++n){
            const double probe=0.10*std::sin(2*pi*997.0*double(n)/sr);
            const double burst=(n<24000)?0.95*std::sin(2*pi*173.0*double(n)/sr):probe;
            a[static_cast<std::size_t>(n)]=MixEngine::V3Research::processTubeV3(burst,driven,sr,1.0,0.90,4);
            b[static_cast<std::size_t>(n)]=MixEngine::V3Research::processTubeV3(probe,reference,sr,1.0,0.90,4);
        }
        auto windowDiff=[&](int s,int e){
            long double q=0.0;long long n=0;
            for(int i=s;i<e;++i){const double d=a[i]-b[i];q+=d*d;++n;}
            return std::sqrt(double(q/n));
        };
        const double early=windowDiff(24500,28500);
        const double late=windowDiff(90000,120000);
        std::cout<<"tube recovery early="<<early<<" late="<<late<<"\n";
        if(!(early>late*2.0&&late<0.01))ok=false;
    }

    // Oversampling must reduce HF nonlinear residue.
    const auto one=renderSine(15000.0,0.82,1.0,0.90,1);
    const auto four=renderSine(15000.0,0.82,1.0,0.90,4);
    const double r1=residual(one,15000.0),r4=residual(four,15000.0);
    std::cout<<"tube HF residual 1x="<<r1<<" 4x="<<r4<<"\n";
    if(!(finite(one)&&finite(four)&&r4<r1*0.25))ok=false;

    // Extreme burst must stay bounded and recover.
    {
        MixEngine::V3Research::TubeV3State st;
        double peak=0.0,tail=0.0;
        for(int n=0;n<140000;++n){
            const double x=n<1024?((n&1)?5.0:-5.0):0.0;
            const double y=MixEngine::V3Research::processTubeV3(x,st,sr,1.0,1.0,4);
            if(!std::isfinite(y)){ok=false;break;}
            peak=std::max(peak,std::abs(y));
            if(n>120000)tail=std::max(tail,std::abs(y));
        }
        std::cout<<"tube extreme peak="<<peak<<" tail="<<tail<<"\n";
        if(!(peak<8.0&&tail<1e-5))ok=false;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": Tube V3 dynamic research model\n";
    return ok?0:1;
}
