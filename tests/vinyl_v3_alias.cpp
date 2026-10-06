#include "vinyl_v3_model.h"
#include "oversampling.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double sr=48000.0;
constexpr int count=131072;
constexpr int warm=16384;
constexpr double amplitude=0.50;

struct Measurement {
    double fundamental=0.0;
    double alias=0.0;
    double peak=0.0;
    bool finite=true;
};

double toneMag(const std::vector<double>& y,double f){
    long double re=0.0,im=0.0;
    long long nCount=0;
    for(int n=warm;n<count;++n){
        const double ph=2.0*pi*f*double(n)/sr;
        re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
        im-=y[static_cast<std::size_t>(n)]*std::sin(ph);
        ++nCount;
    }
    return nCount?2.0*std::sqrt(double(re*re+im*im))/double(nCount):0.0;
}

Measurement render(double frequency,int factor,const MixEngine::V3Research::VinylV3Physical& p){
    MixEngine::OversamplingEngine os;
    MixEngine::V3Research::VinylV3State state;
    std::vector<double> y(count);
    Measurement m;
    const double internalRate=sr*double(factor);
    for(int n=0;n<count;++n){
        const double x=amplitude*std::sin(2.0*pi*frequency*double(n)/sr);
        const double out=os.process(x,factor,[&](double v){
            return MixEngine::V3Research::processVinylV3Tracing(v,state,internalRate,p,1.0);
        });
        y[static_cast<std::size_t>(n)]=out;
        m.peak=std::max(m.peak,std::abs(out));
        if(!std::isfinite(out))m.finite=false;
    }
    // The leading tracing term is predominantly second-order.  For a host-rate
    // tone above fs/4 its second harmonic folds to fs-2f at 1x.
    const double aliasFrequency=sr-2.0*frequency;
    m.fundamental=toneMag(y,frequency);
    m.alias=toneMag(y,aliasFrequency);
    return m;
}

double dbRatio(double a,double b){
    return 20.0*std::log10(std::max(a,1.0e-30)/std::max(b,1.0e-30));
}
}

int main(){
    bool ok=true;
    using MixEngine::V3Research::VinylV3Physical;

    struct Case { const char* name; VinylV3Physical p; };
    VinylV3Physical nominal{};
    auto inner=nominal; inner.grooveRadiusM=0.060;
    auto coarse=nominal; coarse.stylusRadiusM=10.0e-6;
    const std::array<Case,3> cases{{{"nominal",nominal},{"inner-groove",inner},{"coarse-stylus",coarse}}};

    std::cout<<std::fixed<<std::setprecision(3);
    for(const auto& c:cases){
        for(double f:{15000.0,17000.0}){
            const auto m1=render(f,1,c.p);
            const auto m2=render(f,2,c.p);
            const auto m4=render(f,4,c.p);
            const double a1=dbRatio(m1.alias,m1.fundamental);
            const double a2=dbRatio(m2.alias,m2.fundamental);
            const double a4=dbRatio(m4.alias,m4.fundamental);
            std::cout<<c.name<<" f="<<f
                     <<" alias_dBc 1x="<<a1
                     <<" 2x="<<a2
                     <<" 4x="<<a4
                     <<" reduction_2x="<<(a1-a2)
                     <<" reduction_4x="<<(a1-a4)
                     <<" peaks="<<m1.peak<<"/"<<m2.peak<<"/"<<m4.peak<<"\n";
            if(!(m1.finite&&m2.finite&&m4.finite))ok=false;
            if(!(m1.fundamental>1.0e-4&&m2.fundamental>1.0e-4&&m4.fundamental>1.0e-4))ok=false;
            if(!(m1.peak<4.0&&m2.peak<4.0&&m4.peak<4.0))ok=false;
        }
    }

    // This first harness is intentionally measurement-only: factor selection is
    // made after observing the data, not encoded as an assumption in the test.
    std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 oversampling alias measurement harness\n";
    return ok?0:1;
}
