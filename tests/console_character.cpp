#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
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
        const double env=phase<320?1.0:0.42;
        x[n]=env*(0.15*std::sin(2*kPi*71*t)
                 +0.12*std::sin(2*kPi*997*t+0.2)
                 +0.08*std::sin(2*kPi*3971*t+0.4)
                 +0.05*std::sin(2*kPi*9011*t+0.6));
    }
    return x;
}
std::vector<double> render(int mode,double drive,bool enabled=true){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges init{64},outChanges{8};
    const std::vector<std::pair<ParamID,double>> cfg={
      {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
      {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,enabled?1.0:0.0},
      {MixEngine::kParamConsoleMode,double(mode)/3.0},{MixEngine::kParamConsoleDrive,drive},
      {MixEngine::kParamConsoleNoise,0.0},{MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},
      {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},{MixEngine::kParamDepth,0.5},
      {MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,0.5}
    };
    for(const auto& [id,v]:cfg)setParam(init,id,v);

    const auto in=inputSignal();
    std::vector<double> out(kTotal); bool first=true;
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
    const auto dry=render(0,0.0,false);
    bool ok=true;
    std::array<std::vector<double>,4> signatures;
    std::array<double,4> strengths{};

    for(int mode=0;mode<4;++mode){
      const auto wet=render(mode,0.72,true);
      signatures[mode]=delta(wet,dry);
      strengths[mode]=rms(signatures[mode]);
      std::cout<<"Console mode "<<mode<<" deltaRMS="<<strengths[mode]<<"\n";
      if(strengths[mode]<1e-5)ok=false;
    }

    for(int a=0;a<4;++a){
      for(int b=a+1;b<4;++b){
        const double corr=cosine(signatures[a],signatures[b]);
        std::cout<<"Console corr "<<a<<"/"<<b<<"="<<corr<<"\n";
        if(!std::isfinite(corr) || std::abs(corr)>0.985)ok=false;
      }
    }

    // Character/Drive is measured with Level Match disabled. Level Match has its
    // own dedicated QA and must not flatten this diagnostic's transfer metric.
    // Each mode must keep responding through the upper drive range.
    for(int mode=0;mode<4;++mode){
      std::array<double,4> d{};
      int idx=0;
      for(double drive:{0.25,0.50,0.75,1.0}){
        d[idx++]=rms(delta(render(mode,drive,true),dry));
      }
      std::cout<<"Console drive mode "<<mode<<": "<<d[0]<<" "<<d[1]<<" "<<d[2]<<" "<<d[3]<<"\n";
      if(!(d[1]>d[0]*1.03 && d[2]>d[1]*1.03 && d[3]>d[2]*1.01))ok=false;
    }

    std::cout<<(ok?"PASS":"FAIL")<<": Console mode identity and drive audit\n";
    return ok?0:1;
 }catch(int c){std::cerr<<"Console character setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Console character unknown exception\n";return 90;}
}
