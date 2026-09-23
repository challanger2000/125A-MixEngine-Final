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
constexpr double kSr=48000.0;
constexpr int kBlock=256;
constexpr int kPre=12000;
constexpr int kProbe=12000;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& c,ParamID id,double v){
 int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
 int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}
std::vector<double> render(bool hotPre){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
 setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
 if(p->setupProcessing(setup)!=kResultOk)throw 20;
 if(p->setProcessing(true)!=kResultOk)throw 21;

 ParameterChanges init{64},outChanges{8};
 const std::vector<std::pair<ParamID,double>> cfg={
  {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
  {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,1.0},
  {MixEngine::kParamConsoleMode,2.0/3.0},{MixEngine::kParamConsoleDrive,0.85},
  {MixEngine::kParamConsoleNoise,0.0},{MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},
  {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},{MixEngine::kParamDepth,0.5},
  {MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,0.5}
 };
 for(const auto& [id,v]:cfg)setParam(init,id,v);

 const int total=kPre+kProbe;
 std::vector<double> result(total); bool first=true;
 for(int base=0;base<total;base+=kBlock){
  const int count=std::min(kBlock,total-base);
  std::array<double,kBlock> in{},out{};
  for(int i=0;i<count;++i){
   const int n=base+i;
   if(n<kPre){
    const double t=double(n)/kSr;
    in[i]=hotPre?(0.65*std::sin(2*kPi*137.0*t)+0.20*std::sin(2*kPi*997.0*t)):0.0;
   }else{
    const double t=double(n-kPre)/kSr;
    in[i]=0.10*std::sin(2*kPi*997.0*t);
   }
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
double diffRms(const std::vector<double>& a,const std::vector<double>& b,int start,int count){
 long double s=0; int n=0;
 for(int i=start;i<std::min(start+count,(int)a.size());++i){const double d=a[i]-b[i];s+=d*d;++n;}
 return n?std::sqrt(double(s/n)):0.0;
}
double mean(const std::vector<double>& a,int start,int count){
 long double s=0; int n=0;
 for(int i=start;i<std::min(start+count,(int)a.size());++i){s+=a[i];++n;}
 return n?double(s/n):0.0;
}
}

int main(){
 try{
  const auto cold=render(false),hot=render(true);
  const int probe=kPre+128;
  const double early=diffRms(cold,hot,probe,512);
  const double late=diffRms(cold,hot,kPre+kProbe-2048,2048);
  const double dc=std::abs(mean(hot,kPre+2048,kProbe-2048));
  std::cout<<"Vintage Console earlyMemory="<<early<<" lateMemory="<<late<<" dc="<<dc<<"\n";
  bool ok=std::isfinite(early)&&std::isfinite(late)&&std::isfinite(dc);
  if(early<1e-5)ok=false;
  if(late>early*0.80+1e-6)ok=false;
  if(dc>0.002)ok=false;
  std::cout<<(ok?"PASS":"FAIL")<<": bounded decaying Vintage Console memory\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Vintage memory setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Vintage memory unknown exception\n";return 90;}
}
