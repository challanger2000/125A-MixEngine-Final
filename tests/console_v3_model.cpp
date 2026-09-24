#include "console_v3_model.h"
#include "../source/nonlinear_cores.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int nSamp=65536;
constexpr int warm=8192;

double baseSignal(int n){
    const double t=double(n)/sr;
    return 0.24*std::sin(2*pi*83.0*t+0.13)
         +0.18*std::sin(2*pi*997.0*t+0.29)
         +0.10*std::sin(2*pi*4211.0*t+0.47);
}

double twoTone(int n){
    const double t=double(n)/sr;
    return 0.26*std::sin(2*pi*997.0*t)+0.26*std::sin(2*pi*1543.0*t+0.31);
}

double rms(const std::vector<double>& x){
    long double s=0;long long n=0;
    for(int i=warm;i<nSamp;++i){s+=x[static_cast<std::size_t>(i)]*x[static_cast<std::size_t>(i)];++n;}
    return std::sqrt(double(s/n));
}

double diffRms(const std::vector<double>& a,const std::vector<double>& b){
    long double s=0;long long n=0;
    for(int i=warm;i<nSamp;++i){
        const double d=a[static_cast<std::size_t>(i)]-b[static_cast<std::size_t>(i)];
        s+=d*d;++n;
    }
    return std::sqrt(double(s/n));
}

bool finite(const std::vector<double>& x){
    return std::all_of(x.begin(),x.end(),[](double v){return std::isfinite(v);});
}

std::vector<double> referenceSum(){
    std::vector<double> y(nSamp);
    for(int n=0;n<nSamp;++n)y[static_cast<std::size_t>(n)]=baseSignal(n);
    return y;
}

// The linear sum is intentionally IDENTICAL for all channel counts:
// each channel receives 1/N of the same source, so summing N channels always
// reconstructs baseSignal(n). Any output difference is therefore caused by
// the paired nonlinear encoding/summing/decoding architecture itself.
std::vector<double> pairedDistributed(int channels,double drive,int mode){
    std::vector<double> y(nSamp);
    for(int n=0;n<nSamp;++n){
        const double perChannel=baseSignal(n)/double(channels);
        double encodedSum=0.0;
        for(int ch=0;ch<channels;++ch)
            encodedSum+=MixEngine::V3Research::consoleV3Encode(perChannel,drive,mode);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encodedSum,drive,mode);
    }
    return y;
}

std::vector<double> independentDistributed(int channels,double drive,int mode){
    std::vector<double> y(nSamp);
    for(int n=0;n<nSamp;++n){
        const double perChannel=baseSignal(n)/double(channels);
        double sum=0.0;
        for(int ch=0;ch<channels;++ch)
            sum+=MixEngine::processConsoleNonlinearCore(perChannel,perChannel,0.0,mode,drive);
        y[static_cast<std::size_t>(n)]=sum;
    }
    return y;
}

std::vector<double> pairedTwoTone(int channels,double drive,int mode){
    std::vector<double> y(nSamp);
    for(int n=0;n<nSamp;++n){
        const double perChannel=twoTone(n)/double(channels);
        double encodedSum=0.0;
        for(int ch=0;ch<channels;++ch)
            encodedSum+=MixEngine::V3Research::consoleV3Encode(perChannel,drive,mode);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(encodedSum,drive,mode);
    }
    return y;
}

double fittedTwoToneResidual(const std::vector<double>& y){
    // Fit only the original 997/1543 Hz components. Everything left is
    // harmonic/intermodulation/nonlinear residual.
    constexpr std::array<double,2> freq{997.0,1543.0};
    std::array<long double,4> proj{};
    std::array<long double,4> norm{};
    for(int n=warm;n<nSamp;++n){
        const double t=double(n)/sr;
        for(int k=0;k<2;++k){
            const double s=std::sin(2*pi*freq[k]*t),c=std::cos(2*pi*freq[k]*t);
            proj[2*k]+=y[static_cast<std::size_t>(n)]*s;
            proj[2*k+1]+=y[static_cast<std::size_t>(n)]*c;
            norm[2*k]+=s*s;
            norm[2*k+1]+=c*c;
        }
    }
    std::array<double,4> coef{};
    for(int i=0;i<4;++i)coef[i]=double(proj[i]/std::max<long double>(norm[i],1e-30L));

    long double e=0;long long count=0;
    for(int n=warm;n<nSamp;++n){
        const double t=double(n)/sr;
        double fit=0.0;
        for(int k=0;k<2;++k)
            fit+=coef[2*k]*std::sin(2*pi*freq[k]*t)+coef[2*k+1]*std::cos(2*pi*freq[k]*t);
        const double d=y[static_cast<std::size_t>(n)]-fit;
        e+=d*d;++count;
    }
    return std::sqrt(double(e/count));
}
}

int main(){
    bool ok=true;

    // Drive=0 must be exactly linear.
    for(int m=0;m<4;++m){
        for(double x:{-0.9,-0.3,0.0,0.2,0.8}){
            if(MixEngine::V3Research::consoleV3PairedSingle(x,0.0,m)!=x)ok=false;
        }
    }

    // A single channel encode/decode pair must be transparent in its normal domain.
    for(int m=0;m<4;++m){
        double worst=0.0;
        for(int i=-800;i<=800;++i){
            const double x=double(i)/1000.0;
            worst=std::max(worst,std::abs(MixEngine::V3Research::consoleV3PairedSingle(x,0.75,m)-x));
        }
        std::cout<<"mode="<<m<<" singlePairWorst="<<worst<<"\n";
        if(worst>1.0e-9)ok=false;
    }

    const auto linear=referenceSum();
    std::array<double,3> nonlinearResidual{};
    int idx=0;

    for(int channels:{2,8,32}){
        const auto paired=pairedDistributed(channels,0.80,1);
        const auto independent=independentDistributed(channels,0.80,1);
        const double pairVsLinear=diffRms(paired,linear);
        const double pairVsIndependent=diffRms(paired,independent);
        nonlinearResidual[static_cast<std::size_t>(idx++)]=pairVsLinear;

        std::cout<<"fixedMix channels="<<channels
                 <<" pairVsLinear="<<pairVsLinear
                 <<" pairVsIndependent="<<pairVsIndependent
                 <<" rms="<<rms(paired)<<"\n";

        if(!(finite(paired)&&pairVsLinear>1.0e-5&&pairVsIndependent>1.0e-5&&rms(paired)<2.0))
            ok=false;
    }

    // Because the linear mix is identical, this monotonic increase is a true
    // channel-count interaction, not merely a different source mixture.
    if(!(nonlinearResidual[1]>nonlinearResidual[0]*1.05
      && nonlinearResidual[2]>nonlinearResidual[1]*1.01)) ok=false;

    // Two-tone IMD/nonlinear residual must emerge only once multiple encoded
    // channels interact. A single encode/decode pair should remain essentially
    // transparent; an 8-channel distribution should create a measurable residual.
    const auto tt1=pairedTwoTone(1,0.80,2);
    const auto tt8=pairedTwoTone(8,0.80,2);
    const double r1=fittedTwoToneResidual(tt1);
    const double r8=fittedTwoToneResidual(tt8);
    std::cout<<"twoTone residual single="<<r1<<" eight="<<r8<<"\n";
    if(!(r1<1.0e-4&&r8>r1*20.0&&r8>1.0e-4))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 coupled-summing controlled research model\n";
    return ok?0:1;
}
