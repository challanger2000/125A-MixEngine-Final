#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kPi=3.14159265358979323846;
constexpr int kMaxBlock=257;
constexpr int kBlocks=96;

void setParam(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

template<typename T>
double runOne(double sr,bool mixfx){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{};setup.processMode=kRealtime;
 setup.symbolicSampleSize=std::is_same_v<T,float>?kSample32:kSample64;
 setup.maxSamplesPerBlock=kMaxBlock;setup.sampleRate=sr;
 if(p->setupProcessing(setup)!=kResultOk)throw 20;
 if(p->setProcessing(true)!=kResultOk)throw 21;
 if(mixfx){SpeakerArrangement a=SpeakerArr::kStereo;if(p->setMixChannelArrangements(&a,1)!=kResultOk)throw 22;}

 ParameterChanges init{64},outChanges{16};
 const std::vector<std::pair<ParamID,double>> cfg={
  {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.53},{MixEngine::kParamOutput,0.48},
  {MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,1.0},
  {MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,2.0/3.0},{MixEngine::kParamConsoleDrive,0.62},{MixEngine::kParamConsoleNoise,0.15},
  {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.48},{MixEngine::kParamTubeType,0.7},
  {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.52},{MixEngine::kParamTapeSpeed,0.35},{MixEngine::kParamTapeStability,0.82},{MixEngine::kParamTapeHiss,0.12},
  {MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.46},{MixEngine::kParamGlueCharacter,0.65},
  {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.40},{MixEngine::kParamVinylWear,0.27},{MixEngine::kParamVinylNoise,0.10},
  {MixEngine::kParamDepth,0.62},{MixEngine::kParamWidth,0.70},{MixEngine::kParamLowMono,0.35},
  {MixEngine::kParamQuality,1.0}
 };
 for(const auto& [id,v]:cfg)setParam(init,id,v);

 long long pos=0;double maxAbs=0.0;bool first=true;
 const std::array<int,7> sizes{{1,7,31,64,127,193,257}};
 for(int b=0;b<kBlocks;++b){
  const int n=sizes[b%sizes.size()];
  std::array<T,kMaxBlock> inL{},inR{},outL{},outR{};
  for(int i=0;i<n;++i,++pos){
   const double t=double(pos)/sr;
   inL[i]=T(0.13*std::sin(2*kPi*83*t)+0.08*std::sin(2*kPi*1003*t+0.2)+0.04*std::sin(2*kPi*9001*t+0.4));
   inR[i]=T(0.11*std::sin(2*kPi*137*t+0.3)+0.07*std::sin(2*kPi*3001*t+0.5)+0.035*std::sin(2*kPi*11003*t+0.7));
  }

  T* inP[2]{inL.data(),inR.data()};T* outP[2]{outL.data(),outR.data()};
  AudioBusBuffers ib{},ob{};ib.numChannels=2;ob.numChannels=2;
  if constexpr(std::is_same_v<T,float>){ib.channelBuffers32=inP;ob.channelBuffers32=outP;}
  else{ib.channelBuffers64=inP;ob.channelBuffers64=outP;}

  if(mixfx){
   ProcessData ctl{};ctl.processMode=kRealtime;ctl.symbolicSampleSize=setup.symbolicSampleSize;ctl.numSamples=n;
   ctl.inputParameterChanges=first?&init:nullptr;ctl.outputParameterChanges=&outChanges;
   if(p->processMixControl(&ctl)!=kResultOk)throw 23;
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=setup.symbolicSampleSize;d.numSamples=n;
   d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;
   if(p->processMixChannel(0,&d)!=kResultOk)throw 24;
  }else{
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=setup.symbolicSampleSize;d.numSamples=n;
   d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=first?&init:nullptr;d.outputParameterChanges=&outChanges;
   if(p->process(d)!=kResultOk)throw 25;
  }
  first=false;
  for(int i=0;i<n;++i){
   for(double v:{double(outL[i]),double(outR[i])}){if(!std::isfinite(v))throw 26;maxAbs=std::max(maxAbs,std::abs(v));}
  }
 }
 return maxAbs;
}
}

int main(){
 try{
  bool ok=true;
  for(double sr:{44100.0,48000.0,96000.0,192000.0}){
   for(bool mixfx:{false,true}){
    const double p32=runOne<float>(sr,mixfx);
    const double p64=runOne<double>(sr,mixfx);
    std::cout<<(mixfx?"MixFX ":"Channel ")<<sr<<" Hz peak32="<<p32<<" peak64="<<p64<<"\n";
    if(!std::isfinite(p32)||!std::isfinite(p64)||p32>8.0||p64>8.0)ok=false;
    if(std::abs(p32-p64)>0.05)ok=false;
   }
  }
  std::cout<<(ok?"PASS":"FAIL")<<": sample-rate/sample-format matrix\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Sample-rate matrix setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Sample-rate matrix unknown exception\n";return 90;}
}
