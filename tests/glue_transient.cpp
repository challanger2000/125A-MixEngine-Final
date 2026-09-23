#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kSr=48000.0;
constexpr int kBlock=256;
constexpr int kPre=12000;
constexpr int kBurst=4800;
constexpr int kPost=48000;

void setParam(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> render(double amount,double character){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges init{64},outChanges{8};
    const std::vector<std::pair<ParamID,double>> cfg={
      {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
      {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamTubeOn,0.0},
      {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,amount},
      {MixEngine::kParamGlueCharacter,character},{MixEngine::kParamVinylOn,0.0},
      {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
      {MixEngine::kParamQuality,0.5}
    };
    for(const auto& [id,v]:cfg)setParam(init,id,v);

    const int total=kPre+kBurst+kPost;
    std::vector<double> result(total,0.0);
    bool first=true;
    for(int base=0;base<total;base+=kBlock){
      const int count=std::min(kBlock,total-base);
      std::array<double,kBlock> in{},out{};
      for(int i=0;i<count;++i){
        const int n=base+i;
        const double amp=(n<kPre)?0.08:((n<kPre+kBurst)?0.72:0.08);
        in[i]=amp;
      }
      double* inP[1]{in.data()}; double* outP[1]{out.data()};
      AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
      ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
      d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob; d.outputParameterChanges=&outChanges;
      if(first)d.inputParameterChanges=&init;
      if(p->process(d)!=kResultOk)throw 22;
      first=false; init.clearQueue();
      for(int i=0;i<count;++i){if(!std::isfinite(out[i]))throw 23;result[base+i]=out[i];}
    }
    return result;
}

double avgAbs(const std::vector<double>& x,int start,int count){
    long double s=0.0; int n=0;
    for(int i=start;i<std::min(start+count,static_cast<int>(x.size()));++i){s+=std::abs(x[i]);++n;}
    return n?double(s/n):0.0;
}

double relDb(double value,double reference){
    return 20.0*std::log10(std::max(value,1e-12)/std::max(reference,1e-12));
}
}

int main(){
    try{
      bool ok=true;
      for(double amount:{0.25,0.50,0.75,1.0}){
        const auto smooth=render(amount,0.0);
        const auto punch =render(amount,1.0);

        const double baseS=avgAbs(smooth,kPre-2048,2048);
        const double baseP=avgAbs(punch ,kPre-2048,2048);
        const double burstEarlyS=avgAbs(smooth,kPre+32,96);
        const double burstEarlyP=avgAbs(punch ,kPre+32,96);
        const double burstLateS=avgAbs(smooth,kPre+kBurst-1024,1024);
        const double burstLateP=avgAbs(punch ,kPre+kBurst-1024,1024);
        const int postStart=kPre+kBurst;
        const double post50S=avgAbs(smooth,postStart+int(0.050*kSr),512);
        const double post50P=avgAbs(punch ,postStart+int(0.050*kSr),512);
        const double post500S=avgAbs(smooth,postStart+int(0.500*kSr),1024);
        const double post500P=avgAbs(punch ,postStart+int(0.500*kSr),1024);

        const double earlyGrS=20.0*std::log10(std::max(burstEarlyS,1e-12)/0.72);
        const double earlyGrP=20.0*std::log10(std::max(burstEarlyP,1e-12)/0.72);
        const double lateGrS=20.0*std::log10(std::max(burstLateS,1e-12)/0.72);
        const double lateGrP=20.0*std::log10(std::max(burstLateP,1e-12)/0.72);
        const double post50DbS=relDb(post50S,baseS),post50DbP=relDb(post50P,baseP);
        const double post500DbS=relDb(post500S,baseS),post500DbP=relDb(post500P,baseP);

        std::cout<<"Glue "<<amount
                 <<" smooth early="<<earlyGrS<<" late="<<lateGrS<<" post50="<<post50DbS<<" post500="<<post500DbS
                 <<" punch early="<<earlyGrP<<" late="<<lateGrP<<" post50="<<post50DbP<<" post500="<<post500DbP<<"\n";

        if(!std::isfinite(earlyGrS)||!std::isfinite(earlyGrP)||!std::isfinite(lateGrS)||!std::isfinite(lateGrP)
           ||!std::isfinite(post50DbS)||!std::isfinite(post50DbP)||!std::isfinite(post500DbS)||!std::isfinite(post500DbP))ok=false;
        if(lateGrS>-0.05||lateGrP>-0.05)ok=false;
        if(amount>=0.50 && std::abs(lateGrS-lateGrP)<0.20)ok=false;
        if(amount>=0.50 && amount<1.0 && !(earlyGrS>lateGrS+0.18))ok=false;
        if(amount>=0.50 && amount<1.0 && !(earlyGrP>lateGrP+0.10))ok=false;
        if(amount>=0.50 && post50DbS>-0.01)ok=false;
        if(amount>=0.50 && post50DbP>-0.01)ok=false;
        if(amount<1.0){
            if(std::abs(post500DbS)>0.75||std::abs(post500DbP)>0.20)ok=false;
        }else{
            if(std::abs(post500DbS)>3.0||std::abs(post500DbP)>0.50)ok=false;
        }
      }

      std::cout<<(ok?"PASS":"FAIL")<<": Glue transient/recovery audit\n";
      return ok?0:1;
    }catch(int c){std::cerr<<"Glue transient setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Glue transient unknown exception\n";return 90;}
}
