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
constexpr int block=256,total=65536,warm=16384;
constexpr double sr=48000.0,pi=3.14159265358979323846;

void setP(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi_=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi_)!=kResultTrue)throw 11;
}
std::vector<double> signal(){
 std::vector<double>x(total);
 for(int n=0;n<total;++n){
  const double t=double(n)/sr,env=((n/4096)&1)?0.70:1.0;
  x[n]=env*(0.17*std::sin(2*pi*83*t)+0.115*std::sin(2*pi*997*t+0.2)
      +0.072*std::sin(2*pi*4211*t+0.4)+0.043*std::sin(2*pi*9113*t+0.7)
      +0.022*std::sin(2*pi*15013*t+0.5));
 }
 return x;
}
std::vector<double> render(double color,double wear,bool autoGain){
 const auto in=signal();
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=block;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 ParameterChanges c{64},o{16};
 setP(c,MixEngine::kParamInput,0.5);setP(c,MixEngine::kParamOutput,0.5);setP(c,MixEngine::kParamCalibration,0.5);
 setP(c,MixEngine::kParamAutoGain,autoGain?1.0:0.0);
 setP(c,MixEngine::kParamConsoleOn,0.0);setP(c,MixEngine::kParamTubeOn,0.0);
 setP(c,MixEngine::kParamTapeOn,0.0);setP(c,MixEngine::kParamGlueOn,0.0);
 setP(c,MixEngine::kParamVinylOn,(color>0.0||wear>0.0)?1.0:0.0);
 setP(c,MixEngine::kParamVinylCharacter,color);setP(c,MixEngine::kParamVinylWear,wear);setP(c,MixEngine::kParamVinylNoise,0.0);
 setP(c,MixEngine::kParamDepth,0.5);setP(c,MixEngine::kParamWidth,0.5);setP(c,MixEngine::kParamLowMono,0.0);
 setP(c,MixEngine::kParamQuality,0.5);
 std::vector<double>out(total);bool first=true;
 for(int base=0;base<total;base+=block){
  const int n=std::min(block,total-base);std::array<double,block>ib{},ob{};
  for(int i=0;i<n;++i)ib[i]=in[base+i];
  double*ip[1]{ib.data()};double*op[1]{ob.data()};AudioBusBuffers inb{},outb{};
  inb.numChannels=1;inb.channelBuffers64=ip;outb.numChannels=1;outb.channelBuffers64=op;
  ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;
  d.numInputs=1;d.numOutputs=1;d.inputs=&inb;d.outputs=&outb;d.outputParameterChanges=&o;
  if(first)d.inputParameterChanges=&c;
  if(p->process(d)!=kResultOk)throw 21;
  first=false;c.clearQueue();
  for(int i=0;i<n;++i){if(!std::isfinite(ob[i]))throw 22;out[base+i]=ob[i];}
 }
 return out;
}
double rms(const std::vector<double>&x){
 long double q=0;long long n=0;for(int i=warm;i<total;++i){q+=x[i]*x[i];++n;}
 return std::sqrt(double(q/n));
}
double db(double a,double b){return 20.0*std::log10(std::max(a,1e-15)/std::max(b,1e-15));}
}

int main(){
 bool ok=true;
 const double dry=rms(render(0.0,0.0,true));
 for(double c:{0.25,0.50,0.75,1.0}){
  const double d=db(rms(render(c,0.0,true)),dry);
  std::cout<<"Vinyl Auto-Level color="<<c<<" wear=0 delta="<<d<<" dB\n";
  if(!std::isfinite(d)||std::abs(d)>0.60)ok=false;
 }
 for(double w:{0.25,0.50,0.75,1.0}){
  const double d=db(rms(render(0.0,w,true)),dry);
  std::cout<<"Vinyl Auto-Level color=0 wear="<<w<<" delta="<<d<<" dB\n";
  if(!std::isfinite(d)||std::abs(d)>0.60)ok=false;
 }
 for(const auto& p:{std::pair<double,double>{0.25,0.10},{0.50,0.30},{0.75,0.50},{1.0,0.75}}){
  const double d=db(rms(render(p.first,p.second,true)),dry);
  std::cout<<"Vinyl Auto-Level color="<<p.first<<" wear="<<p.second<<" delta="<<d<<" dB\n";
  if(!std::isfinite(d)||std::abs(d)>0.75)ok=false;
 }
 std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 auto-level calibration\n";
 return ok?0:1;
}
