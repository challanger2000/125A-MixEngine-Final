#include "../source/processor.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(bool ok){if(!ok)throw 1;}
void point(ParameterChanges& c,ParamID id,int offset,double value){int32 qi=0,pi=0;auto*q=c.addParameterData(id,qi);require(q&&q->addPoint(offset,value,pi)==kResultOk);}
template<class Sample>double measure(bool mix,int chunk,bool jump){
 auto p=std::make_unique<MixEngine::Processor>();require(p->initialize(nullptr)==kResultOk);
 SpeakerArrangement stereo=SpeakerArr::kStereo;require(p->setBusArrangements(&stereo,1,&stereo,1)==kResultOk);
 ProcessSetup setup{};setup.sampleRate=48000.;setup.maxSamplesPerBlock=64;setup.symbolicSampleSize=sizeof(Sample)==4?kSample32:kSample64;setup.processMode=kRealtime;
 require(p->setupProcessing(setup)==kResultOk);
#ifndef MIXENGINE_CHANNEL_BUILD
 if(mix)require(p->setMixChannelArrangements(&stereo,1)==kResultOk);
#endif
 require(p->setActive(true)==kResultOk&&p->setProcessing(true)==kResultOk);
 std::vector<double> output;
 for(int phase=0;phase<3;++phase)for(int base=0;base<64;base+=chunk){
  const int count=std::min(chunk,64-base);ParameterChanges changes{64};
  if(phase==0&&base==0){
   for(auto id:{MixEngine::kParamConsoleOn,MixEngine::kParamTubeOn,MixEngine::kParamTapeOn,MixEngine::kParamGlueOn,MixEngine::kParamVinylOn,MixEngine::kParamAutoGain})point(changes,id,0,0.);
  }
  if(phase==1){
   if(!jump)point(changes,MixEngine::kParamWidth,count-1,.5+.5*(base+count)/64.);
   else{
    if(base<=31&&base+count>31)point(changes,MixEngine::kParamWidth,31-base,.5);
    if(base<=32&&base+count>32)point(changes,MixEngine::kParamWidth,32-base,1.);
   }
  }
  std::array<Sample,64> l{},r{},ol{},orr{};l.fill(Sample(.2));r.fill(Sample(-.2));Sample* ip[]{l.data(),r.data()};Sample* op[]{ol.data(),orr.data()};
  AudioBusBuffers ib{},ob{};ib.numChannels=ob.numChannels=2;
  if constexpr(sizeof(Sample)==4){ib.channelBuffers32=ip;ob.channelBuffers32=op;}else{ib.channelBuffers64=ip;ob.channelBuffers64=op;}
  ProcessData data{};data.numSamples=count;data.symbolicSampleSize=setup.symbolicSampleSize;data.processMode=kRealtime;data.numInputs=data.numOutputs=1;data.inputs=&ib;data.outputs=&ob;data.inputParameterChanges=&changes;
#ifndef MIXENGINE_CHANNEL_BUILD
  if(mix){require(p->processMixControl(&data)==kResultOk);data.inputParameterChanges=nullptr;require(p->processMixChannel(0,&data)==kResultOk);}
  else
#endif
  require(p->process(data)==kResultOk);
  for(int i=0;i<count;++i){require(std::isfinite(ol[i])&&std::isfinite(orr[i]));require(std::abs(double(ol[i])+double(orr[i]))<1e-6);output.push_back(double(ol[i]));}
 }
 double error=0;const int delay=int(p->getLatencySamples());
 for(int n=delay;n<int(output.size());++n){
  const int source=n-delay;double scale=1.;
  if(jump){if(source>=96)scale=2.;}
  else if(source>=64)scale=1.+std::min(64,source-63)/64.;
  // Pure Side: width 100% -> 200% means L=.2 -> .4. This independent
  // expected line uses the VST3 implicit -1 endpoint and fixed reported PDC.
  error=std::max(error,std::abs(output[n]-.2*scale));
 }
 p->setProcessing(false);p->setActive(false);p->terminate();
 std::cout<<"mix="<<mix<<" bits="<<sizeof(Sample)*8<<" chunk="<<chunk<<" jump="<<jump<<" lineError="<<error<<"\n";return error;
}
}
int main(){try{
 bool ok=true;
#ifdef MIXENGINE_CHANNEL_BUILD
 const std::array<bool,1> paths{false};
#else
 const std::array<bool,2> paths{false,true};
#endif
 for(bool mix:paths)for(int chunk:{64,16,1})for(bool jump:{false,true}){
  ok=measure<float>(mix,chunk,jump)<1e-6&&ok;ok=measure<double>(mix,chunk,jump)<1e-12&&ok;
 }
 std::cout<<(ok?"PASS":"FAIL")<<": VST3 linear automation / implicit -1 / adjacent-point jumps\n";return ok?0:1;
}catch(...){return 2;}}
