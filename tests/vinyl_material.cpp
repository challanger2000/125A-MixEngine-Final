#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kBlock=256;
constexpr int kTotal=65536;
constexpr int kWarm=8192;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> inputSignal(){
    std::vector<double> x(kTotal);
    for(int n=0;n<kTotal;++n){
        const double t=double(n)/kSr;
        const int phase=n%4096;
        const double env=phase<256?1.0:0.38;
        x[n]=env*(0.16*std::sin(2*kPi*91*t)
                 +0.13*std::sin(2*kPi*997*t+0.2)
                 +0.09*std::sin(2*kPi*4111*t+0.4)
                 +0.06*std::sin(2*kPi*9911*t+0.6)
                 +0.04*std::sin(2*kPi*15113*t+0.8));
    }
    return x;
}

std::vector<double> render(double color,double wear){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges init{64},outChanges{8};
    const std::vector<std::pair<ParamID,double>> cfg={
      {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
      {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamTubeOn,0.0},
      {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamGlueOn,0.0},
      {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,color},{MixEngine::kParamVinylWear,wear},
      {MixEngine::kParamVinylNoise,0.0},{MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},
      {MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,0.5}
    };
    for(const auto& [id,v]:cfg)setParam(init,id,v);

    const auto in=inputSignal();
    std::vector<double> out(kTotal,0.0); bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> ibuf{},obuf{};
        for(int i=0;i<count;++i)ibuf[i]=in[base+i];
        double* inP[1]{ibuf.data()}; double* outP[1]{obuf.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
        ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
        d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob; d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&init;
        if(p->process(d)!=kResultOk)throw 22;
        first=false; init.clearQueue();
        for(int i=0;i<count;++i){if(!std::isfinite(obuf[i]))throw 23;out[base+i]=obuf[i];}
    }
    return out;
}

std::vector<double> delta(const std::vector<double>& wet,const std::vector<double>& dry){
    std::vector<double> d; d.reserve(kTotal-kWarm);
    for(int i=kWarm;i<kTotal;++i)d.push_back(wet[i]-dry[i]);
    return d;
}

double cosine(const std::vector<double>& a,const std::vector<double>& b){
    long double ab=0,aa=0,bb=0;
    for(std::size_t i=0;i<a.size();++i){ab+=a[i]*b[i];aa+=a[i]*a[i];bb+=b[i]*b[i];}
    return double(ab/std::sqrt(std::max<long double>(aa*bb,1e-30L)));
}

double rms(const std::vector<double>& x){
    long double s=0; for(double v:x)s+=v*v; return std::sqrt(double(s/std::max<std::size_t>(1,x.size())));
}
}

int main(){
    try{
        const auto dry=render(0.0,0.0);
        const auto color=render(0.75,0.0);
        const auto wear=render(0.0,0.75);
        const auto both=render(0.75,0.75);
        const auto dc=delta(color,dry),dw=delta(wear,dry),db=delta(both,dry);
        const double corr=cosine(dc,dw);
        const double colorR=rms(dc),wearR=rms(dw),bothR=rms(db);

        std::cout<<"Vinyl Color deltaRMS="<<colorR
                 <<" Wear deltaRMS="<<wearR
                 <<" Both deltaRMS="<<bothR
                 <<" Color/Wear correlation="<<corr<<"\n";

        bool ok=true;
        if(!std::isfinite(corr)||!std::isfinite(colorR)||!std::isfinite(wearR)||!std::isfinite(bothR))ok=false;
        if(colorR<1e-5||wearR<1e-5)ok=false;
        if(std::abs(corr)>0.965)ok=false; // controls must not collapse to one axis
        if(bothR<std::max(colorR,wearR)*1.10)ok=false;

        std::cout<<(ok?"PASS":"FAIL")<<": Vinyl material-axis independence audit\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"Vinyl material setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Vinyl material unknown exception\n";return 90;}
}
