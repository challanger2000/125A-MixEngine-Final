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
constexpr int kChannels=8;
constexpr int kTotal=16384;
constexpr int kWarm=2048;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto* q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

struct Render{std::array<std::vector<double>,kChannels> ch;};

Render render(bool otherChannels,double drive,const std::array<int,kChannels>& order,int blockSize){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
 setup.maxSamplesPerBlock=256;setup.sampleRate=kSr;
 if(p->setupProcessing(setup)!=kResultOk)throw 20;
 if(p->setProcessing(true)!=kResultOk)throw 21;
 SpeakerArrangement arr[kChannels]{};
 for(auto& a:arr)a=SpeakerArr::kMono;
 if(p->setMixChannelArrangements(arr,kChannels)!=kResultOk)throw 22;

 ParameterChanges init{64},outChanges{16};
 setParam(init,MixEngine::kParamBypass,0.0);
 setParam(init,MixEngine::kParamInput,0.5);
 setParam(init,MixEngine::kParamOutput,0.5);
 setParam(init,MixEngine::kParamCalibration,0.5);
 setParam(init,MixEngine::kParamAutoGain,0.0);
 setParam(init,MixEngine::kParamConsoleOn,1.0);
 setParam(init,MixEngine::kParamConsoleMode,1.0/3.0);
 setParam(init,MixEngine::kParamConsoleDrive,drive);
 setParam(init,MixEngine::kParamConsoleCrosstalk,0.0);
 setParam(init,MixEngine::kParamConsoleNoise,0.0);
 setParam(init,MixEngine::kParamTubeOn,0.0);
 setParam(init,MixEngine::kParamTapeOn,0.0);
 setParam(init,MixEngine::kParamGlueOn,0.0);
 setParam(init,MixEngine::kParamVinylOn,0.0);
 setParam(init,MixEngine::kParamDepth,0.5);
 setParam(init,MixEngine::kParamWidth,0.5);
 setParam(init,MixEngine::kParamLowMono,0.0);
 setParam(init,MixEngine::kParamQuality,0.0);

 Render out;for(auto& v:out.ch)v.assign(kTotal,0.0);
 bool first=true;
 for(int base=0;base<kTotal;base+=blockSize){
  const int count=std::min(blockSize,kTotal-base);
  std::array<std::vector<double>,kChannels> in{},y{};
  for(int ch=0;ch<kChannels;++ch){in[ch].assign(count,0.0);y[ch].assign(count,0.0);}
  for(int i=0;i<count;++i){
   const int n=base+i;const double t=double(n)/kSr;
   in[0][static_cast<std::size_t>(i)]=0.13*std::sin(2*kPi*997.0*t)+0.055*std::sin(2*kPi*4211.0*t+0.2);
   if(otherChannels){
    for(int ch=1;ch<kChannels;++ch){
     const double f=131.0+73.0*ch;
     in[ch][static_cast<std::size_t>(i)]=0.075*std::sin(2*kPi*f*t+0.17*ch)
         +0.035*std::sin(2*kPi*(1103.0+29.0*ch)*t+0.11*ch);
    }
   }
  }

  std::array<AudioBusBuffers,kChannels> ib{},ob{};
  std::array<double*,kChannels> ip{},op{};
  for(int ch=0;ch<kChannels;++ch){
   ip[ch]=in[ch].data();op[ch]=y[ch].data();
   ib[ch].numChannels=1;ib[ch].channelBuffers64=&ip[ch];
   ob[ch].numChannels=1;ob[ch].channelBuffers64=&op[ch];
  }
  ProcessData control{};control.processMode=kRealtime;control.symbolicSampleSize=kSample64;
  control.numSamples=count;control.numInputs=kChannels;control.numOutputs=kChannels;
  control.inputs=ib.data();control.outputs=ob.data();
  control.inputParameterChanges=first?&init:nullptr;control.outputParameterChanges=&outChanges;
  if(p->processMixControl(&control)!=kResultOk)throw 23;
  first=false;

  for(int ch:order){
   AudioBusBuffers oneIn=ib[ch],oneOut=ob[ch];
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=count;
   d.numInputs=1;d.numOutputs=1;d.inputs=&oneIn;d.outputs=&oneOut;
   if(p->processMixChannel(ch,&d)!=kResultOk)throw 24;
  }
  for(int ch=0;ch<kChannels;++ch)
   for(int i=0;i<count;++i){
    const double v=y[ch][static_cast<std::size_t>(i)];
    if(!std::isfinite(v))throw 25;
    out.ch[ch][static_cast<std::size_t>(base+i)]=v;
   }
 }
 return out;
}

double diffRms(const std::vector<double>& a,const std::vector<double>& b){
 long double s=0;long long n=0;
 for(int i=kWarm;i<kTotal;++i){const double d=a[i]-b[i];s+=d*d;++n;}
 return std::sqrt(double(s/n));
}
double maxDiff(const Render& a,const Render& b){
 double m=0.0;
 for(int ch=0;ch<kChannels;++ch)
  for(int i=kWarm;i<kTotal;++i)m=std::max(m,std::abs(a.ch[ch][i]-b.ch[ch][i]));
 return m;
}
}

int main(){
 try{
  const std::array<int,kChannels> forward{{0,1,2,3,4,5,6,7}};
  const std::array<int,kChannels> reverse{{7,6,5,4,3,2,1,0}};

  const auto zeroSolo=render(false,0.0,forward,127);
  const auto zeroBusy=render(true,0.0,reverse,127);
  const double zeroCross=diffRms(zeroSolo.ch[0],zeroBusy.ch[0]);

  const auto solo=render(false,0.80,forward,127);
  const auto busy=render(true,0.80,forward,127);
  const auto busyReverse=render(true,0.80,reverse,127);
  const auto busy31=render(true,0.80,reverse,31);

  const double coupledDelta=diffRms(solo.ch[0],busy.ch[0]);
  const double orderDiff=maxDiff(busy,busyReverse);
  const double blockDiff=maxDiff(busy,busy31);

  std::cout<<"MixFX Console V3 zeroCross="<<zeroCross
           <<" coupledDelta="<<coupledDelta
           <<" orderDiff="<<orderDiff
           <<" blockDiff="<<blockDiff<<"\n";

  bool ok=true;
  // V3 Console ON + displayed Drive 0 retains a deliberate low-level base
  // character, including a small amount of coupled MixFX interaction.
  // The base interaction must be real but bounded, and higher Drive must
  // increase the coupling materially without introducing order/block dependence.
  ok=ok&&zeroCross>1.0e-5;
  ok=ok&&zeroCross<0.02;
  ok=ok&&coupledDelta>zeroCross*1.20;
  ok=ok&&orderDiff<1.0e-12;
  ok=ok&&blockDiff<1.0e-10;
  std::cout<<(ok?"PASS":"FAIL")<<": MixFX Console V3 live coupled interaction\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"MixFX Console V3 setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"MixFX Console V3 unknown exception\n";return 90;}
}
