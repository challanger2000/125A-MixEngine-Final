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
constexpr int kBlock=256,kTotal=65536,kWarm=16384;
constexpr double sr=48000.0,pi=3.14159265358979323846;

void setP(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi_=0;if(q->addPoint(0,v,pi_)!=kResultTrue)throw 11;
}
std::vector<double> signal(){
 std::vector<double>x(kTotal);
 for(int n=0;n<kTotal;++n){
  const double t=double(n)/sr,env=((n/4096)&1)?0.68:1.0;
  x[n]=env*(0.18*std::sin(2*pi*83*t)+0.12*std::sin(2*pi*997*t+0.2)
      +0.075*std::sin(2*pi*4211*t+0.4)+0.045*std::sin(2*pi*9113*t+0.7));
 }
 return x;
}
std::vector<double> render(double type,double amount,bool autoGain){
 const auto in=signal();
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=kBlock;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 ParameterChanges c{64},o{16};
 setP(c,MixEngine::kParamInput,0.5);setP(c,MixEngine::kParamOutput,0.5);setP(c,MixEngine::kParamCalibration,0.5);
 setP(c,MixEngine::kParamAutoGain,autoGain?1.0:0.0);
 setP(c,MixEngine::kParamConsoleOn,0.0);setP(c,MixEngine::kParamTubeOn,amount>0?1.0:0.0);
 setP(c,MixEngine::kParamTubeAmount,amount);setP(c,MixEngine::kParamTubeType,type);
 setP(c,MixEngine::kParamTapeOn,0.0);setP(c,MixEngine::kParamGlueOn,0.0);setP(c,MixEngine::kParamVinylOn,0.0);
 setP(c,MixEngine::kParamQuality,0.5);
 std::vector<double>out(kTotal);bool first=true;
 for(int base=0;base<kTotal;base+=kBlock){
  const int count=std::min(kBlock,kTotal-base);std::array<double,kBlock>ib{},ob{};
  for(int i=0;i<count;++i)ib[i]=in[base+i];
  double*ip[1]{ib.data()};double*op[1]{ob.data()};AudioBusBuffers inb{},outb{};
  inb.numChannels=1;inb.channelBuffers64=ip;outb.numChannels=1;outb.channelBuffers64=op;
  ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=count;
  d.numInputs=1;d.numOutputs=1;d.inputs=&inb;d.outputs=&outb;d.outputParameterChanges=&o;
  if(first)d.inputParameterChanges=&c;
  if(p->process(d)!=kResultOk)throw 21;
  first=false;c.clearQueue();
  for(int i=0;i<count;++i){if(!std::isfinite(ob[i]))throw 22;out[base+i]=ob[i];}
 }
 return out;
}
double rms(const std::vector<double>&x){
 long double q=0;long long n=0;for(int i=kWarm;i<kTotal;++i){q+=x[i]*x[i];++n;}
 return std::sqrt(double(q/n));
}
}

int main(){
 bool finiteAll=true;
 const double dry=rms(render(0.5,0.0,true));
 for(double type:{0.0,0.5,1.0}){
  for(double amount:{0.25,0.5,0.75,1.0}){
   const double r=rms(render(type,amount,true));
   const double db=20.0*std::log10(std::max(r,1e-15)/std::max(dry,1e-15));
   std::cout<<"Tube Auto-Level type="<<type<<" amount="<<amount<<" delta="<<db<<" dB\n";
   finiteAll&=std::isfinite(db);
  }
 }
 // Measurement pass: only finiteness is gated. The next commit replaces the
 // old V2 compensation and will tighten this to a level-match acceptance band.
 std::cout<<(finiteAll?"PASS":"FAIL")<<": Tube V3 auto-level measurement grid\n";
 return finiteAll?0:1;
}
