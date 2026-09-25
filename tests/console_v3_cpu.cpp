#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int block=256;
constexpr int measuredBlocks=180;
constexpr int warmBlocks=24;
constexpr double sr=48000.0;
constexpr double pi=3.14159265358979323846;

void setParam(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto* q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

struct Stats{double mean=0,p95=0,p99=0,max=0;int overruns=0;};

Stats stats(std::vector<double> v,double deadline){
 std::sort(v.begin(),v.end());
 Stats s{};
 if(v.empty())return s;
 s.mean=std::accumulate(v.begin(),v.end(),0.0)/double(v.size());
 auto q=[&](double p){const std::size_t i=std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1));return v[i];};
 s.p95=q(0.95);s.p99=q(0.99);s.max=v.back();
 s.overruns=static_cast<int>(std::count_if(v.begin(),v.end(),[&](double x){return x>deadline;}));
 return s;
}

Stats run(int channels,double drive){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
 setup.maxSamplesPerBlock=block;setup.sampleRate=sr;
 if(p->setupProcessing(setup)!=kResultOk)throw 20;
 if(p->setProcessing(true)!=kResultOk)throw 21;
 std::vector<SpeakerArrangement> arr(static_cast<std::size_t>(channels),SpeakerArr::kMono);
 if(p->setMixChannelArrangements(arr.data(),channels)!=kResultOk)throw 22;

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

 std::vector<std::vector<double>> in(static_cast<std::size_t>(channels),std::vector<double>(block));
 std::vector<std::vector<double>> out(static_cast<std::size_t>(channels),std::vector<double>(block));
 std::vector<double*> ip(static_cast<std::size_t>(channels)),op(static_cast<std::size_t>(channels));
 std::vector<AudioBusBuffers> ib(static_cast<std::size_t>(channels)),ob(static_cast<std::size_t>(channels));
 for(int ch=0;ch<channels;++ch){
  ip[static_cast<std::size_t>(ch)]=in[static_cast<std::size_t>(ch)].data();
  op[static_cast<std::size_t>(ch)]=out[static_cast<std::size_t>(ch)].data();
  ib[static_cast<std::size_t>(ch)].numChannels=1;ib[static_cast<std::size_t>(ch)].channelBuffers64=&ip[static_cast<std::size_t>(ch)];
  ob[static_cast<std::size_t>(ch)].numChannels=1;ob[static_cast<std::size_t>(ch)].channelBuffers64=&op[static_cast<std::size_t>(ch)];
 }

 std::vector<double> durations;durations.reserve(measuredBlocks);
 bool first=true;
 const int total=warmBlocks+measuredBlocks;
 for(int b=0;b<total;++b){
  for(int ch=0;ch<channels;++ch)
   for(int i=0;i<block;++i){
    const long long n=static_cast<long long>(b)*block+i;
    const double f=83.0+7.0*(ch%23);
    in[static_cast<std::size_t>(ch)][static_cast<std::size_t>(i)]
      =0.035*std::sin(2*pi*f*double(n)/sr+0.11*ch)
      +0.018*std::sin(2*pi*(997.0+3.0*(ch%17))*double(n)/sr);
   }

  auto t0=std::chrono::steady_clock::now();

  ProcessData control{};control.processMode=kRealtime;control.symbolicSampleSize=kSample64;
  control.numSamples=block;control.numInputs=channels;control.numOutputs=channels;
  control.inputs=ib.data();control.outputs=ob.data();
  control.inputParameterChanges=first?&init:nullptr;control.outputParameterChanges=&outChanges;
  if(p->processMixControl(&control)!=kResultOk)throw 23;
  first=false;

  for(int ch=0;ch<channels;++ch){
   AudioBusBuffers oneIn=ib[static_cast<std::size_t>(ch)],oneOut=ob[static_cast<std::size_t>(ch)];
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=block;
   d.numInputs=1;d.numOutputs=1;d.inputs=&oneIn;d.outputs=&oneOut;
   if(p->processMixChannel(ch,&d)!=kResultOk)throw 24;
  }

  auto t1=std::chrono::steady_clock::now();
  if(b>=warmBlocks)
   durations.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
 }
 const double deadlineUs=1.0e6*double(block)/sr;
 return stats(std::move(durations),deadlineUs);
}
}

int main(){
 try{
  bool ok=true;
  for(int channels:{2,8,32,64,128}){
   const auto off=run(channels,0.0);
   const auto on=run(channels,0.80);
   const double overheadMean=on.mean-off.mean;
   const double overheadP99=on.p99-off.p99;
   const double deadlineUs=1.0e6*double(block)/sr;
   std::cout<<"channels="<<channels
            <<" off(mean/p95/p99/max)="<<off.mean<<"/"<<off.p95<<"/"<<off.p99<<"/"<<off.max
            <<" us on="<<on.mean<<"/"<<on.p95<<"/"<<on.p99<<"/"<<on.max
            <<" overheadMean="<<overheadMean
            <<" overheadP99="<<overheadP99
            <<" deadline="<<deadlineUs
            <<" overruns="<<on.overruns<<"\n";
   if(!(std::isfinite(on.mean)&&std::isfinite(on.p99)&&std::isfinite(on.max)))ok=false;
   if(channels<=64){
    // Production gate: up to 64 simultaneous Console channels must meet the
    // realtime block deadline at p99 with zero measured overruns.
    if(on.overruns!=0||on.p99>=deadlineUs)ok=false;
   }else{
    // 128 channels is an intentionally extreme shared-runner stress case.
    // Gate sustained throughput and bounded tail behavior without treating
    // isolated hosted-runner scheduling stalls as DSP regressions.
    const double overrunFraction=double(on.overruns)/double(measuredBlocks);
    if(on.mean>=deadlineUs||on.p95>=deadlineUs||overrunFraction>0.05)ok=false;
   }
  }
  std::cout<<(ok?"PASS":"FAIL")<<": Console V3 CPU scaling measurement\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Console CPU setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Console CPU unknown exception\n";return 90;}
}
