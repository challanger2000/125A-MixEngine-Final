#include "vinyl_v3_model.h"
#include "vinyl_v3_tracing_oracle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=192000.0;
constexpr int N=65536;
constexpr int warm=8192;
constexpr double amplitude=0.50;

double thdModel(double frequency,const MixEngine::V3Research::VinylV3Physical& p){
    MixEngine::V3Research::VinylV3State st;
    std::vector<double> y(N);
    for(int n=0;n<N;++n){
        const double x=amplitude*std::sin(2*pi*frequency*double(n)/sr);
        y[static_cast<std::size_t>(n)]=MixEngine::V3Research::processVinylV3Tracing(x,st,sr,p,1.0);
    }
    auto tone=[&](double f){
        long double re=0,im=0;long long c=0;
        for(int n=warm;n<N;++n){
            const double ph=2*pi*f*double(n)/sr;
            re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
            im-=y[static_cast<std::size_t>(n)]*std::sin(ph);++c;
        }
        return c?2*std::sqrt(double(re*re+im*im))/double(c):0.0;
    };
    const double fundamental=tone(frequency);
    long double h2=0.0;
    for(int h=2;h<=9;++h){
        if(h*frequency>=0.5*sr)break;
        const double m=tone(h*frequency);h2+=m*m;
    }
    return std::sqrt(double(h2))/std::max(1e-30,fundamental);
}

double thdOracle(double frequency,const MixEngine::V3Research::VinylTracingParameters& p){
    MixEngine::V3Research::VinylTracingOracle oracle(p);
    std::vector<double> y(N);
    for(int n=0;n<N;++n){
        const double ph=2*pi*frequency*double(n)/sr;
        y[static_cast<std::size_t>(n)]=oracle.processSinePhase(ph,frequency);
    }
    auto tone=[&](double f){
        long double re=0,im=0;long long c=0;
        for(int n=warm;n<N;++n){
            const double ph=2*pi*f*double(n)/sr;
            re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
            im-=y[static_cast<std::size_t>(n)]*std::sin(ph);++c;
        }
        return c?2*std::sqrt(double(re*re+im*im))/double(c):0.0;
    };
    const double fundamental=tone(frequency);
    long double h2=0.0;
    for(int h=2;h<=9;++h){
        if(h*frequency>=0.5*sr)break;
        const double m=tone(h*frequency);h2+=m*m;
    }
    return std::sqrt(double(h2))/std::max(1e-30,fundamental);
}
}

int main(){
    bool ok=true;
    using namespace MixEngine::V3Research;

    VinylV3Physical modelNominal{};
    VinylTracingParameters oracleNominal{};

    std::array<double,4> f{{1000.0,5000.0,10000.0,15000.0}};
    std::array<double,4> m{},o{};
    for(std::size_t i=0;i<f.size();++i){
        m[i]=thdModel(f[i],modelNominal);
        o[i]=thdOracle(f[i],oracleNominal);
        std::cout<<"f="<<f[i]<<" modelTHD="<<m[i]<<" oracleTHD="<<o[i]
                 <<" ratio="<<(m[i]/std::max(1e-30,o[i]))<<"\n";
    }

    // The realtime surrogate should reproduce the oracle's broad frequency
    // trend and land near its nominal 10 kHz distortion magnitude.
    if(!(m[1]>m[0]*3.0&&m[2]>m[1]*1.5&&m[3]>m[2]*1.25))ok=false;
    if(!(m[2]>o[2]*0.65&&m[2]<o[2]*1.35))ok=false;

    auto modelOuter=modelNominal;modelOuter.grooveRadiusM=0.145;
    auto modelInner=modelNominal;modelInner.grooveRadiusM=0.060;
    auto oracleOuter=oracleNominal;oracleOuter.grooveRadiusM=0.145;
    auto oracleInner=oracleNominal;oracleInner.grooveRadiusM=0.060;
    const double modelGroove=thdModel(10000.0,modelInner)/thdModel(10000.0,modelOuter);
    const double oracleGroove=thdOracle(10000.0,oracleInner)/thdOracle(10000.0,oracleOuter);
    std::cout<<"groove ratio model="<<modelGroove<<" oracle="<<oracleGroove<<"\n";
    if(!(modelGroove>oracleGroove*0.70&&modelGroove<oracleGroove*1.30))ok=false;

    auto modelFine=modelNominal;modelFine.stylusRadiusM=3e-6;
    auto modelCoarse=modelNominal;modelCoarse.stylusRadiusM=10e-6;
    auto oracleFine=oracleNominal;oracleFine.stylusRadiusM=3e-6;
    auto oracleCoarse=oracleNominal;oracleCoarse.stylusRadiusM=10e-6;
    const double modelStylus=thdModel(10000.0,modelCoarse)/thdModel(10000.0,modelFine);
    const double oracleStylus=thdOracle(10000.0,oracleCoarse)/thdOracle(10000.0,oracleFine);
    std::cout<<"stylus ratio model="<<modelStylus<<" oracle="<<oracleStylus<<"\n";
    if(!(modelStylus>oracleStylus*0.75&&modelStylus<oracleStylus*1.25))ok=false;

    // Exact zero-intensity neutrality.
    {
        VinylV3State st;
        for(int n=0;n<10000;++n){
            const double x=0.8*std::sin(0.017*n)+0.1*std::sin(0.091*n);
            const double y=processVinylV3Tracing(x,st,48000.0,modelNominal,0.0);
            if(y!=x){ok=false;break;}
        }
    }

    std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 tracing surrogate vs physical oracle\n";
    return ok?0:1;
}
