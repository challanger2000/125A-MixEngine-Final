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
constexpr double sr=48000.0,pi=3.14159265358979323846;
constexpr int total=12288,eventSample=4096,block=256;
using Setting=std::pair<ParamID,double>;
struct C{const char*name;ParamID id;double before,after;std::vector<Setting> extra;};
void add(ParameterChanges& c,ParamID id,int32 off,double v){int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;int32 pi=0;if(q->addPoint(off,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;}
std::vector<double> render(bool mixfx,const C&tc){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=block;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 if(mixfx){SpeakerArrangement a=SpeakerArr::kMono;if(p->setMixChannelArrangements(&a,1)!=kResultOk)throw 21;}
 std::vector<double> y(total);bool first=true;
 for(int base=0;base<total;base+=block){
  const int n=std::min(block,total-base);std::array<double,block> in{},out{};
  for(int i=0;i<n;++i){const double t=double(base+i)/sr;in[i]=0.16*std::sin(2*pi*997*t)+0.07*std::sin(2*pi*5111*t+0.21)+0.03*std::sin(2*pi*89*t+0.4);}
  ParameterChanges ch{64},och{8};
  if(first){
   const Setting baseCfg[]={
    {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,1.0},
    {MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamConsoleMode,1.0/3.0},{MixEngine::kParamConsoleDrive,0.35},{MixEngine::kParamConsoleNoise,0.0},
    {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTubeAmount,0.35},{MixEngine::kParamTubeType,0.5},
    {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamTapeAmount,0.35},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.9},{MixEngine::kParamTapeHiss,0.0},
    {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamGlueAmount,0.35},{MixEngine::kParamGlueCharacter,0.5},
    {MixEngine::kParamVinylOn,0.0},{MixEngine::kParamVinylCharacter,0.35},{MixEngine::kParamVinylWear,0.2},{MixEngine::kParamVinylNoise,0.0},
    {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,0.5}
   };
   for(auto [id,v]:baseCfg)add(ch,id,0,v);
   for(auto [id,v]:tc.extra)add(ch,id,0,v);
   add(ch,tc.id,0,tc.before);
  }
  if(eventSample>=base&&eventSample<base+n)add(ch,tc.id,eventSample-base,tc.after);
  double*ip[1]{in.data()},*op[1]{out.data()};AudioBusBuffers ib{},ob{};ib.numChannels=1;ib.channelBuffers64=ip;ob.numChannels=1;ob.channelBuffers64=op;
  if(mixfx){
   ProcessData ctl{};ctl.processMode=kRealtime;ctl.symbolicSampleSize=kSample64;ctl.numSamples=n;ctl.inputParameterChanges=&ch;ctl.outputParameterChanges=&och;if(p->processMixControl(&ctl)!=kResultOk)throw 22;
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;if(p->processMixChannel(0,&d)!=kResultOk)throw 23;
  }else{
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=&ch;d.outputParameterChanges=&och;if(p->process(d)!=kResultOk)throw 24;
  }
  for(int i=0;i<n;++i){if(!std::isfinite(out[i]))throw 25;y[(size_t)(base+i)]=out[i];}
  first=false;
 }
 return y;
}
double deriv(const std::vector<double>&y,int a,int b){double m=0;a=std::max(a,1);b=std::min(b,(int)y.size());for(int i=a;i<b;++i)m=std::max(m,std::abs(y[(size_t)i]-y[(size_t)(i-1)]));return m;}
}
int main(){try{
 const std::vector<C> cases={
  {"Input",MixEngine::kParamInput,0.45,0.60,{}},{"Output",MixEngine::kParamOutput,0.45,0.60,{}},
  {"ConsoleOn",MixEngine::kParamConsoleOn,0.0,1.0,{}},
  {"TubeOn",MixEngine::kParamTubeOn,0.0,1.0,{{MixEngine::kParamTubeAmount,0.35}}},
  {"TapeOn",MixEngine::kParamTapeOn,0.0,1.0,{{MixEngine::kParamTapeAmount,0.35},{MixEngine::kParamTapeStability,0.9}}},
  {"GlueOn",MixEngine::kParamGlueOn,0.0,1.0,{{MixEngine::kParamGlueAmount,0.35}}},
  {"VinylOn",MixEngine::kParamVinylOn,0.0,1.0,{{MixEngine::kParamVinylCharacter,0.35},{MixEngine::kParamVinylWear,0.2}}},
  {"ConsoleDrive",MixEngine::kParamConsoleDrive,0.25,0.75,{{MixEngine::kParamConsoleOn,1.0}}},
  {"TubeAmount",MixEngine::kParamTubeAmount,0.25,0.75,{{MixEngine::kParamTubeOn,1.0}}},
  {"TapeAmount",MixEngine::kParamTapeAmount,0.25,0.75,{{MixEngine::kParamTapeOn,1.0}}},
  {"TapeStability",MixEngine::kParamTapeStability,1.0,0.35,{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.65}}},
  {"GlueAmount",MixEngine::kParamGlueAmount,0.25,0.75,{{MixEngine::kParamGlueOn,1.0}}},
  {"GlueResponse",MixEngine::kParamGlueCharacter,0.2,0.8,{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.6}}},
  {"VinylColor",MixEngine::kParamVinylCharacter,0.2,0.8,{{MixEngine::kParamVinylOn,1.0}}},
  {"VinylWear",MixEngine::kParamVinylWear,0.1,0.7,{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.6}}},
  {"Depth",MixEngine::kParamDepth,0.35,0.75,{}},{"Width",MixEngine::kParamWidth,0.4,0.8,{}},{"LowMono",MixEngine::kParamLowMono,0.0,0.8,{}}
 };
 bool ok=true;
 for(bool mixfx:{false,true})for(const auto&tc:cases){
  const auto y=render(mixfx,tc);const double local=deriv(y,eventSample-8,eventSample+64),before=deriv(y,1024,eventSample-256),after=deriv(y,eventSample+512,total-512),steady=std::max(before,after),ratio=local/std::max(steady,1e-12);
  std::cout<<(mixfx?"MixFX ":"Channel ")<<tc.name<<" local="<<local<<" steady="<<steady<<" ratio="<<ratio<<"\n";
  if(!std::isfinite(ratio)||local>steady*6.0+0.03)ok=false;
 }
 std::cout<<(ok?"PASS":"FAIL")<<": continuous-control automation transient audit\n";
 return ok?0:1;
}catch(int c){std::cerr<<"Transition audit setup FAIL "<<c<<"\n";return c;}catch(...){return 90;}}
