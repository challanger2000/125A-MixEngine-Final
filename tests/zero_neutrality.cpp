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
constexpr double kSr=48000.0;
constexpr int kBlock=127;
constexpr int kTotal=16384;
constexpr int kWarm=4096;
constexpr double kPi=3.14159265358979323846;
using Setting=std::pair<ParamID,double>;

void setParam(ParameterChanges& c,ParamID id,double v){
 int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
 int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> render(bool mixfx,const std::vector<Setting>& extra){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
 setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
 if(p->setupProcessing(setup)!=kResultOk)throw 20;
 if(p->setProcessing(true)!=kResultOk)throw 21;
 if(mixfx){SpeakerArrangement a=SpeakerArr::kMono;if(p->setMixChannelArrangements(&a,1)!=kResultOk)throw 22;}

 ParameterChanges changes{64},outChanges{8};
 const std::vector<Setting> base={
  {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},
  {MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,1.0},
  {MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamConsoleNoise,0.0},
  {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},{MixEngine::kParamTapeHiss,0.0},
  {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},{MixEngine::kParamVinylNoise,0.0},
  {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
  {MixEngine::kParamQuality,0.5}
 };
 for(const auto& [id,v]:base)setParam(changes,id,v);
 for(const auto& [id,v]:extra)setParam(changes,id,v);

 std::vector<double> output(kTotal); bool first=true;
 for(int baseSample=0;baseSample<kTotal;baseSample+=kBlock){
  const int count=std::min(kBlock,kTotal-baseSample);
  std::array<double,kBlock> in{},out{};
  for(int i=0;i<count;++i){
   const int n=baseSample+i; const double t=double(n)/kSr;
   in[i]=0.16*std::sin(2*kPi*73*t)+0.10*std::sin(2*kPi*997*t+0.2)+0.06*std::sin(2*kPi*7001*t+0.4);
  }
  double* inP[1]{in.data()};double* outP[1]{out.data()};
  AudioBusBuffers ib{},ob{};ib.numChannels=1;ib.channelBuffers64=inP;ob.numChannels=1;ob.channelBuffers64=outP;

  if(mixfx){
   ProcessData control{};control.processMode=kRealtime;control.symbolicSampleSize=kSample64;control.numSamples=count;
   control.inputParameterChanges=first?&changes:nullptr;control.outputParameterChanges=&outChanges;
   if(p->processMixControl(&control)!=kResultOk)throw 23;
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=count;
   d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;
   if(p->processMixChannel(0,&d)!=kResultOk)throw 24;
  }else{
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=count;
   d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=first?&changes:nullptr;d.outputParameterChanges=&outChanges;
   if(p->process(d)!=kResultOk)throw 25;
  }
  for(int i=0;i<count;++i){if(!std::isfinite(out[i]))throw 26;output[baseSample+i]=out[i];}
  first=false;
 }
 return output;
}

double diffRms(const std::vector<double>& a,const std::vector<double>& b){
 long double s=0,ref=0;int n=0;
 for(int i=kWarm;i<kTotal;++i){const double d=a[i]-b[i];s+=d*d;ref+=a[i]*a[i];++n;}
 const double dr=std::sqrt(double(s/n)),rr=std::sqrt(double(ref/n));
 return 20.0*std::log10(std::max(dr,1e-15)/std::max(rr,1e-15));
}
}

int main(){
 try{
  struct C{const char*name;std::vector<Setting> cfg;};
  const std::vector<C> cases={
   {"Console base",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.0},{MixEngine::kParamConsoleNoise,0.0}}},
   {"Tube base",{{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.0}}},
   {"Tape base",{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.0},{MixEngine::kParamTapeHiss,0.0}}},
   {"Glue base",{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.0}}},
   {"Vinyl base",{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.0},{MixEngine::kParamVinylWear,0.0},{MixEngine::kParamVinylNoise,0.0}}}
  };
  bool ok=true;
  for(bool mixfx:{false,true}){
   const auto dry=render(mixfx,{});
   for(const auto& tc:cases){
    const auto wet=render(mixfx,tc.cfg);
    const double d=diffRms(dry,wet);
    std::cout<<(mixfx?"MixFX ":"Channel ")<<tc.name<<" residual="<<d<<" dBFSrel\n";
    // Enabled character modules must already do something at a displayed 0 %,
    // but the base character should remain subtle rather than becoming a second
    // hidden high-intensity setting.
    if(d<-60.0 || d>-6.0)ok=false;
   }
  }
  std::cout<<(ok?"PASS":"FAIL")<<": enabled-module base-character audit\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Neutrality setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Neutrality unknown exception\n";return 90;}
}
