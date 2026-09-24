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

double rms(const std::vector<double>& x){
    long double s=0;long long n=0;
    for(int i=warm;i<nSamp;++i){s+=x[static_cast<std::size_t>(i)]*x[static_cast<std::size_t>(i)];++n;}
    return std::sqrt(double(s/n));
}
double diffRms(const std::vector<double>& a,const std::vector<double>& b){
    long double s=0;long long n=0;
    for(int i=warm;i<nSamp;++i){const double d=a[static_cast<std::size_t>(i)]-b[static_cast<std::size_t>(i)];s+=d*d;++n;}
    return std::sqrt(double(s/n));
}
bool finite(const std::vector<double>& x){
    return std::all_of(x.begin(),x.end(),[](double v){return std::isfinite(v);});
}

std::vector<double> pairedSum(int channels,double drive,int mode){
    std::vector<double> y(nSamp);
    const double scale=1.0/std::sqrt(double(channels));
    for(int n=0;n<nSamp;++n){
        const double t=double(n)/sr;
        double sum=0.0;
        for(int ch=0;ch<channels;++ch){
            const double f=83.0+37.0*ch;
            const double phase=0.17*ch;
            const double x=scale*(0.18*std::sin(2*pi*f*t+phase)+0.11*std::sin(2*pi*(997.0+13.0*ch)*t+0.11*ch));
            sum+=MixEngine::V3Research::consoleV3Encode(x,drive,mode);
        }
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::consoleV3Decode(sum,drive,mode);
    }
    return y;
}

std::vector<double> linearSum(int channels){
    std::vector<double> y(nSamp);
    const double scale=1.0/std::sqrt(double(channels));
    for(int n=0;n<nSamp;++n){
        const double t=double(n)/sr;
        double sum=0.0;
        for(int ch=0;ch<channels;++ch){
            const double f=83.0+37.0*ch;
            const double phase=0.17*ch;
            sum+=scale*(0.18*std::sin(2*pi*f*t+phase)+0.11*std::sin(2*pi*(997.0+13.0*ch)*t+0.11*ch));
        }
        y[static_cast<std::size_t>(n)]=sum;
    }
    return y;
}

std::vector<double> independentSaturationSum(int channels,double drive,int mode){
    std::vector<double> y(nSamp);
    const double scale=1.0/std::sqrt(double(channels));
    for(int n=0;n<nSamp;++n){
        const double t=double(n)/sr;
        double sum=0.0;
        for(int ch=0;ch<channels;++ch){
            const double f=83.0+37.0*ch;
            const double phase=0.17*ch;
            const double x=scale*(0.18*std::sin(2*pi*f*t+phase)+0.11*std::sin(2*pi*(997.0+13.0*ch)*t+0.11*ch));
            // Low/high are set to x/0 for an intentionally simple independent
            // comparator. This is not claimed to reproduce the full V2 state.
            sum+=MixEngine::processConsoleNonlinearCore(x,x,0.0,mode,drive);
        }
        y[static_cast<std::size_t>(n)]=sum;
    }
    return y;
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

    // At normal single-channel levels encode/decode should nearly cancel.
    for(int m=0;m<4;++m){
        double worst=0.0;
        for(int i=-800;i<=800;++i){
            const double x=double(i)/1000.0;
            worst=std::max(worst,std::abs(MixEngine::V3Research::consoleV3PairedSingle(x,0.75,m)-x));
        }
        std::cout<<"mode="<<m<<" singlePairWorst="<<worst<<"\n";
        if(worst>1.0e-9)ok=false;
    }

    // Multi-channel behavior must emerge from summation, and must not collapse
    // to either a linear sum or independent per-channel saturation.
    for(int channels:{2,8,32}){
        const auto lin=linearSum(channels);
        const auto paired=pairedSum(channels,0.80,1);
        const auto indep=independentSaturationSum(channels,0.80,1);
        const double pairVsLinear=diffRms(paired,lin);
        const double pairVsIndependent=diffRms(paired,indep);
        std::cout<<"channels="<<channels
                 <<" pairVsLinear="<<pairVsLinear
                 <<" pairVsIndependent="<<pairVsIndependent
                 <<" rms="<<rms(paired)<<"\n";
        if(!(finite(paired)&&pairVsLinear>1.0e-5&&pairVsIndependent>1.0e-5&&rms(paired)<2.0))ok=false;
    }

    // More simultaneous channels must change the interaction signature rather
    // than producing a fixed post-sum transfer curve.
    const auto p2=pairedSum(2,0.80,2);
    const auto p8=pairedSum(8,0.80,2);
    const auto p32=pairedSum(32,0.80,2);
    const double d28=diffRms(p2,p8),d832=diffRms(p8,p32);
    std::cout<<"channelCountSignature d2_8="<<d28<<" d8_32="<<d832<<"\n";
    if(!(d28>1.0e-4&&d832>1.0e-4))ok=false;

    std::cout<<(ok?"PASS":"FAIL")<<": Console V3 coupled-summing research model\n";
    return ok?0:1;
}
