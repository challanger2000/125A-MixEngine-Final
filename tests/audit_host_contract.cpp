#include "../source/processor.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr double sr=48000.0;
constexpr int block=64;
void require(bool ok,const char* message){if(!ok)throw message;}
void point(ParameterChanges& changes,ParamID id,double value){
 int32 qi=0,pi=0;auto* q=changes.addParameterData(id,qi);
 require(q && q->addPoint(0,value,pi)==kResultTrue,"parameter fixture");
}
struct Fixture {
 std::unique_ptr<MixEngine::Processor> p=std::make_unique<MixEngine::Processor>();
 bool mix;
 explicit Fixture(bool mixfx,double rate=sr):mix(mixfx){
  require(p->initialize(nullptr)==kResultOk,"initialize");
  SpeakerArrangement mono=SpeakerArr::kMono;
  require(p->setBusArrangements(&mono,1,&mono,1)==kResultOk,"bus");
  ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
  setup.maxSamplesPerBlock=block;setup.sampleRate=rate;
  require(p->setupProcessing(setup)==kResultOk,"setup");
#ifndef MIXENGINE_CHANNEL_BUILD
  if(mix)require(p->setMixChannelArrangements(&mono,1)==kResultOk,"mix bus");
#endif
  require(p->setActive(true)==kResultOk,"activate");
  require(p->setProcessing(true)==kResultOk,"processing");
 }
 ~Fixture(){p->setProcessing(false);p->setActive(false);p->terminate();}
 std::array<double,block> process(const std::array<double,block>& signal,ParameterChanges* changes=nullptr){
  auto input=signal;std::array<double,block> output{};
  double* ip[]{input.data()};double* op[]{output.data()};
  AudioBusBuffers ib{},ob{};ib.numChannels=ob.numChannels=1;ib.channelBuffers64=ip;ob.channelBuffers64=op;
  ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=block;
  d.numInputs=d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=changes;
#ifndef MIXENGINE_CHANNEL_BUILD
  if(mix){require(p->processMixControl(&d)==kResultOk,"mix control");d.inputParameterChanges=nullptr;require(p->processMixChannel(0,&d)==kResultOk,"mix channel");}
  else
#endif
  require(p->process(d)==kResultOk,"process");
  return output;
 }
};
void configuration(ParameterChanges& c,bool colour){
 const std::pair<ParamID,double> cfg[]={
  {MixEngine::kParamAutoGain,0},{MixEngine::kParamConsoleOn,colour?1.:0.},
  {MixEngine::kParamConsoleDrive,.55},{MixEngine::kParamTubeOn,colour?1.:0.},
  {MixEngine::kParamTubeAmount,.55},{MixEngine::kParamTapeOn,colour?1.:0.},
  {MixEngine::kParamTapeAmount,.55},{MixEngine::kParamTapeStability,.6},
  {MixEngine::kParamGlueOn,colour?1.:0.},{MixEngine::kParamGlueAmount,.55},
  {MixEngine::kParamVinylOn,colour?1.:0.},{MixEngine::kParamVinylCharacter,.55},
  {MixEngine::kParamQuality,.5}
 };for(auto [id,v]:cfg)point(c,id,v);
}
bool bypass(bool mix){
 Fixture reference(mix),tested(mix);double maxDiff=0,dryError=0;
 std::vector<double> inputHistory;
 for(int b=0;b<200;++b){
  ParameterChanges a{64},c{64};if(b==0){configuration(a,true);configuration(c,true);}
  if(b==20){require(tested.p->setProcessing(false)==kResultOk,"pause");require(tested.p->setProcessing(true)==kResultOk,"resume");}
  if(b==30)point(c,MixEngine::kParamBypass,1);
  if(b==130)point(c,MixEngine::kParamBypass,0);
  std::array<double,block> x{};
  for(int i=0;i<block;++i){const double t=double(b*block+i)/sr;x[i]=.2*std::sin(6.283185307179586*997*t)+.07*std::sin(6.283185307179586*73*t);inputHistory.push_back(x[i]);}
  auto expected=reference.process(x,&a),actual=tested.process(x,&c);
  for(int i=0;i<block;++i){
   if((b>=20&&b<30)||b>=134)maxDiff=std::max(maxDiff,std::abs(expected[i]-actual[i]));
   if(b>=35&&b<130){const int n=b*block+i-int(tested.p->getLatencySamples());dryError=std::max(dryError,std::abs(actual[i]-inputHistory[n]));}
  }
 }
 std::cout<<"bypass "<<mix<<" wetResumeMaxDiff="<<maxDiff<<" dryError="<<dryError<<"\n";
 return maxDiff<1e-12&&dryError<1e-12;
}
bool atomicState(){
 Fixture f(false);MemoryStream before;require(f.p->getState(&before)==kResultOk,"save");
 bool ok=true;
 for(int bytes:{1,7,8,16,199,207,209,215,217,223,225,231,233,239}){
  MemoryStream bad;IBStreamer s(&bad,kLittleEndian);
  for(int i=0;i<30;++i)require(s.writeDouble(.75),"write");
  // Restrict readable bytes without replacing the retained valid state.
  MemoryStream partial(bad.getData(),bytes);
  const auto result=f.p->setState(&partial);MemoryStream after;
  require(f.p->getState(&after)==kResultOk,"save after");
  const bool same=before.getSize()==after.getSize()&&std::equal(before.getData(),before.getData()+before.getSize(),after.getData());
  std::cout<<"truncated bytes="<<bytes<<" rejected="<<(result!=kResultOk)<<" atomic="<<same<<"\n";
  ok=ok&&result!=kResultOk&&same;
  before.seek(0,IBStream::kIBSeekSet,nullptr);require(f.p->setState(&before)==kResultOk,"restore baseline");
 }
 return ok;
}
bool tail(bool mix){
 bool ok=true;
 for(double rate:{44100.,48000.,96000.,192000.}){
  Fixture f(mix,rate);ParameterChanges c{64};configuration(c,true);
  std::array<double,block> x{};x[0]=.25;auto y=f.process(x,&c);
  int last=0;double peak=0;x.fill(0);
  for(int b=1;b<int(rate*2/block);++b){y=f.process(x);for(int i=0;i<block;++i){peak=std::max(peak,std::abs(y[i]));if(std::abs(y[i])>1e-9)last=b*block+i;}}
  const auto reported=f.p->getTailSamples();
  std::cout<<"tail "<<mix<<" sr="<<rate<<" reported="<<reported<<" lastAbove1e-9="<<last<<" peak="<<peak<<"\n";
  ok=ok&&last>block&&reported>=unsigned(last);
 }
 return ok;
}
bool extremeFinite(bool mix){
 Fixture f(mix);ParameterChanges c{64};configuration(c,true);
 std::array<double,block> x{};x[0]=std::numeric_limits<double>::max();x[1]=-x[0];
 bool ok=true;double peak=0;
 for(int b=0;b<100;++b){auto y=f.process(x,b==0?&c:nullptr);for(double v:y){ok=ok&&std::isfinite(v);if(std::isfinite(v))peak=std::max(peak,std::abs(v));}x.fill(0);}
 std::cout<<"extreme finite "<<mix<<" allFinite="<<ok<<" peak="<<peak<<"\n";return ok&&peak<1.0;
}
bool outputRamp(bool mix){
 Fixture f(mix);ParameterChanges c{64};configuration(c,false);
 std::array<double,block> x{};x.fill(.2);f.process(x,&c);
 for(int b=0;b<30;++b)f.process(x);
 ParameterChanges change{64};point(change,MixEngine::kParamOutput,1);
 double previous=.2,maxJump=0;for(int b=0;b<30;++b){auto y=f.process(x,b==0?&change:nullptr);for(double v:y){maxJump=std::max(maxJump,std::abs(v-previous));previous=v;}}
 const double g=std::pow(10.,12./20.);
 const double bound=.2*(g-1)*(1-std::exp(-1/(.005*sr)))+1e-12;
 std::cout<<"output "<<mix<<" maxJump="<<maxJump<<" 5msBound="<<bound<<" final="<<previous<<"\n";
 return maxJump<=bound&&std::abs(previous-.2*g)<.001;
}
}
int main(){try{
 bool ok=atomicState();
#ifdef MIXENGINE_CHANNEL_BUILD
 const std::array<bool,1> paths{false};
#else
 const std::array<bool,2> paths{false,true};
#endif
 for(bool mix:paths){ok=bypass(mix)&&ok;ok=tail(mix)&&ok;ok=outputRamp(mix)&&ok;ok=extremeFinite(mix)&&ok;}
 std::cout<<(ok?"PASS":"FAIL")<<": independent host contracts\n";return ok?0:1;
}catch(const char* m){std::cerr<<m<<"\n";return 2;}}
