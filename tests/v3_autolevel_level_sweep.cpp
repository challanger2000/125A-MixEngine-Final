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
constexpr int block=256,total=49152,warm=12288;
constexpr double sr=48000.0,pi=3.14159265358979323846;

void setP(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi_=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi_)!=kResultTrue)throw 11;
}
std::vector<double> signal(double scale){
 std::vector<double>x(total);
 for(int n=0;n<total;++n){
  const double t=double(n)/sr;
  x[n]=scale*(0.080*std::sin(2*pi*97*t)+0.055*std::sin(2*pi*997*t+0.17)
       +0.030*std::sin(2*pi*4123*t+0.31)+0.018*std::sin(2*pi*9031*t+0.53));
 }
 return x;
}
enum class M{Dry,Tube,Tape};
double render(double scale,M m,bool autoGain){
 const auto in=signal(scale);
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=block;s.sampleRate=sr;
 if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 ParameterChanges c{64},o{16};
 setP(c,MixEngine::kParamInput,0.5);setP(c,MixEngine::kParamOutput,0.5);setP(c,MixEngine::kParamCalibration,0.5);
 setP(c,MixEngine::kParamAutoGain,autoGain?1.0:0.0);
 setP(c,MixEngine::kParamConsoleOn,0.0);
 setP(c,MixEngine::kParamTubeOn,m==M::Tube?1.0:0.0);setP(c,MixEngine::kParamTubeType,0.5);setP(c,MixEngine::kParamTubeAmount,0.70);
 setP(c,MixEngine::kParamTapeOn,m==M::Tape?1.0:0.0);setP(c,MixEngine::kParamTapeAmount,0.70);setP(c,MixEngine::kParamTapeSpeed,0.5);setP(c,MixEngine::kParamTapeStability,1.0);setP(c,MixEngine::kParamTapeHiss,0.0);
 setP(c,MixEngine::kParamGlueOn,0.0);setP(c,MixEngine::kParamVinylOn,0.0);
 setP(c,MixEngine::kParamDepth,0.5);setP(c,MixEngine::kParamWidth,0.5);setP(c,MixEngine::kParamLowMono,0.0);setP(c,MixEngine::kParamQuality,0.5);
 long double q=0;long long nCount=0;bool first=true;
 for(int base=0;base<total;base+=block){
  const int n=std::min(block,total-base);std::array<double,block>ib{},ob{};
  for(int i=0;i<n;++i)ib[i]=in[base+i];
  double*ip[1]{ib.data()};double*op[1]{ob.data()};AudioBusBuffers inb{},outb{};
  inb.numChannels=1;inb.channelBuffers64=ip;outb.numChannels=1;outb.channelBuffers64=op;
  ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&inb;d.outputs=&outb;d.outputParameterChanges=&o;
  if(first)d.inputParameterChanges=&c;
  if(p->process(d)!=kResultOk)throw 21;first=false;c.clearQueue();
  for(int i=0;i<n;++i){if(!std::isfinite(ob[i]))throw 22;if(base+i>=warm){q+=ob[i]*ob[i];++nCount;}}
 }
 return std::sqrt(double(q/nCount));
}
double db(double a,double b){return 20*std::log10(std::max(a,1e-15)/std::max(b,1e-15));}
}
int main(){
 try{
  bool ok=true;
  for(double scale:{0.35,0.50,0.75,1.0,1.25,1.50,2.0}){
   const double dry=render(scale,M::Dry,false);
   const double tubeOff=db(render(scale,M::Tube,false),dry);
   const double tubeOn=db(render(scale,M::Tube,true),dry);
   const double tapeOff=db(render(scale,M::Tape,false),dry);
   const double tapeOn=db(render(scale,M::Tape,true),dry);
   std::cout<<"scale="<<scale
            <<" tubeOff="<<tubeOff<<" tubeOn="<<tubeOn
            <<" tapeOff="<<tapeOff<<" tapeOn="<<tapeOn<<" dB\n";
   if(!(std::isfinite(tubeOff)&&std::isfinite(tubeOn)&&std::isfinite(tapeOff)&&std::isfinite(tapeOn)))ok=false;
  }
  std::cout<<(ok?"PASS":"FAIL")<<": V3 auto-level level-dependence measurement\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Level sweep setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Level sweep unknown exception\n";return 90;}
}
