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
constexpr int block=256,total=32768,warm=4096;
constexpr double sr=48000.0;
void setP(ParameterChanges& c,ParamID id,double v){int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;}
std::vector<double> render(double amount,double hiss){
 auto p=std::make_unique<MixEngine::Processor>();ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=block;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 ParameterChanges c{64},o{8};
 setP(c,MixEngine::kParamInput,0.5);setP(c,MixEngine::kParamOutput,0.5);setP(c,MixEngine::kParamCalibration,0.5);
 setP(c,MixEngine::kParamAutoGain,0.0);setP(c,MixEngine::kParamConsoleOn,0.0);setP(c,MixEngine::kParamTubeOn,0.0);
 setP(c,MixEngine::kParamTapeOn,1.0);setP(c,MixEngine::kParamTapeAmount,amount);setP(c,MixEngine::kParamTapeSpeed,0.5);
 setP(c,MixEngine::kParamTapeStability,1.0);setP(c,MixEngine::kParamTapeHiss,hiss);
 setP(c,MixEngine::kParamGlueOn,0.0);setP(c,MixEngine::kParamVinylOn,0.0);setP(c,MixEngine::kParamDepth,0.5);setP(c,MixEngine::kParamWidth,0.5);setP(c,MixEngine::kParamLowMono,0.0);setP(c,MixEngine::kParamQuality,0.5);
 std::vector<double>out(total);bool first=true;
 for(int base=0;base<total;base+=block){const int n=std::min(block,total-base);std::array<double,block>ib{},ob{};double*ip[1]{ib.data()};double*op[1]{ob.data()};AudioBusBuffers inb{},outb{};inb.numChannels=1;inb.channelBuffers64=ip;outb.numChannels=1;outb.channelBuffers64=op;ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&inb;d.outputs=&outb;d.outputParameterChanges=&o;if(first)d.inputParameterChanges=&c;if(p->process(d)!=kResultOk)throw 21;first=false;c.clearQueue();for(int i=0;i<n;++i){if(!std::isfinite(ob[i]))throw 22;out[base+i]=ob[i];}}
 return out;
}
double rms(const std::vector<double>&x){long double s=0;long long n=0;for(int i=warm;i<total;++i){s+=x[i]*x[i];++n;}return std::sqrt(double(s/n));}
}
int main(){
 try{
  const double silent=rms(render(0.0,0.0));
  const double h25=rms(render(0.0,0.25));
  const double h50=rms(render(0.0,0.50));
  const double h100=rms(render(0.0,1.0));
  std::cout<<"Tape Hiss Amount0 RMS off/25/50/100="<<silent<<"/"<<h25<<"/"<<h50<<"/"<<h100<<"\n";
  bool ok=silent<1e-12 && h25>1e-7 && h50>h25*2.5 && h100>h50*2.5;
  std::cout<<(ok?"PASS":"FAIL")<<": Tape Hiss independent-control contract\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Tape Hiss setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Tape Hiss unknown exception\n";return 90;}
}
