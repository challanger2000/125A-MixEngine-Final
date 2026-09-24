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
constexpr int kBlock=256,kTotal=32768,kWarm=4096;
constexpr double kSr=48000.0,kPi=3.14159265358979323846;
using Setting=std::pair<ParamID,double>;

void setP(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> render(const std::vector<Setting>& cfg,bool silence){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=kBlock;s.sampleRate=kSr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 ParameterChanges c{64},o{8};
 const std::vector<Setting> base={
  {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
  {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamTubeOn,0.0},
  {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},
  {MixEngine::kParamConsoleNoise,0.0},{MixEngine::kParamTapeHiss,0.0},{MixEngine::kParamVinylNoise,0.0},
  {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
  {MixEngine::kParamQuality,0.5}
 };
 for(auto [id,v]:base)setP(c,id,v);for(auto [id,v]:cfg)setP(c,id,v);
 std::vector<double>out(kTotal);bool first=true;
 for(int baseSample=0;baseSample<kTotal;baseSample+=kBlock){
  const int n=std::min(kBlock,kTotal-baseSample);std::array<double,kBlock>ib{},ob{};
  if(!silence)for(int i=0;i<n;++i){const double t=double(baseSample+i)/kSr;ib[i]=0.16*std::sin(2*kPi*997*t)+0.07*std::sin(2*kPi*4211*t+0.2);}
  double*ip[1]{ib.data()};double*op[1]{ob.data()};AudioBusBuffers inb{},outb{};inb.numChannels=1;inb.channelBuffers64=ip;outb.numChannels=1;outb.channelBuffers64=op;
  ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&inb;d.outputs=&outb;d.outputParameterChanges=&o;if(first)d.inputParameterChanges=&c;
  if(p->process(d)!=kResultOk)throw 21;first=false;c.clearQueue();
  for(int i=0;i<n;++i){if(!std::isfinite(ob[i]))throw 22;out[baseSample+i]=ob[i];}
 }
 return out;
}

double rms(const std::vector<double>&x){long double s=0;long long n=0;for(int i=kWarm;i<kTotal;++i){s+=x[i]*x[i];++n;}return std::sqrt(double(s/n));}
double diffRms(const std::vector<double>&a,const std::vector<double>&b){long double s=0;long long n=0;for(int i=kWarm;i<kTotal;++i){double d=a[i]-b[i];s+=d*d;++n;}return std::sqrt(double(s/n));}
}

int main(){
 try{
  bool ok=true;

  const auto c0=render({{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.0},{MixEngine::kParamConsoleNoise,0.0}},true);
  const auto c25=render({{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.0},{MixEngine::kParamConsoleNoise,0.25}},true);
  const auto c50=render({{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.0},{MixEngine::kParamConsoleNoise,0.50}},true);
  const auto c100=render({{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.0},{MixEngine::kParamConsoleNoise,1.0}},true);
  const double cr0=rms(c0),cr25=rms(c25),cr50=rms(c50),cr100=rms(c100);
  std::cout<<"Console Noise RMS 0/25/50/100="<<cr0<<"/"<<cr25<<"/"<<cr50<<"/"<<cr100<<"\n";
  // Console ON + Drive 0 retains V3 base character and may leave a tiny
  // floating-point residual on digital silence. Noise=0 must remain effectively
  // silent; the independent Noise control must then scale progressively.
  ok=ok&&cr0<1e-8&&cr25>1e-8&&cr50>cr25*2.5&&cr100>cr50*2.5;

  const auto v0=render({{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.0},{MixEngine::kParamVinylWear,0.0},{MixEngine::kParamVinylNoise,0.0}},true);
  const auto v25=render({{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.0},{MixEngine::kParamVinylWear,0.0},{MixEngine::kParamVinylNoise,0.25}},true);
  const auto v50=render({{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.0},{MixEngine::kParamVinylWear,0.0},{MixEngine::kParamVinylNoise,0.50}},true);
  const auto v100=render({{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.0},{MixEngine::kParamVinylWear,0.0},{MixEngine::kParamVinylNoise,1.0}},true);
  const double vr0=rms(v0),vr25=rms(v25),vr50=rms(v50),vr100=rms(v100);
  std::cout<<"Vinyl Surface RMS 0/25/50/100="<<vr0<<"/"<<vr25<<"/"<<vr50<<"/"<<vr100<<"\n";
  ok=ok&&vr0<1e-12&&vr25>1e-7&&vr50>vr25*2.5&&vr100>vr50*2.5;

  const auto stable=render({{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.65},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,1.0}},false);
  const auto mid=render({{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.65},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.5}},false);
  const auto loose=render({{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.65},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.0}},false);
  const double dMid=diffRms(stable,mid),dLoose=diffRms(stable,loose);
  std::cout<<"Tape Stability delta stable->50="<<dMid<<" stable->0="<<dLoose<<"\n";
  ok=ok&&dMid>1e-5&&dLoose>dMid*1.10;

  std::cout<<(ok?"PASS":"FAIL")<<": secondary control independence/scaling audit\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Secondary controls setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Secondary controls unknown exception\n";return 90;}
}
