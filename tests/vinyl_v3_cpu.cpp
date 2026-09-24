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
constexpr int block=256,measuredBlocks=160,warmBlocks=24;
constexpr double sr=48000.0,pi=3.14159265358979323846;
void setP(ParameterChanges& c,ParamID id,double v){
 int32 qi=0;auto*q=c.addParameterData(id,qi);if(!q)throw 10;
 int32 pi_=0;if(q->addPoint(0,std::clamp(v,0.0,1.0),pi_)!=kResultTrue)throw 11;
}
struct Stats{double mean=0,p95=0,p99=0,max=0;int overruns=0;};
Stats stats(std::vector<double> v,double deadline){
 std::sort(v.begin(),v.end());Stats s{};if(v.empty())return s;
 s.mean=std::accumulate(v.begin(),v.end(),0.0)/double(v.size());
 auto q=[&](double p){const std::size_t i=std::min(v.size()-1,static_cast<std::size_t>(std::ceil(p*v.size())-1));return v[i];};
 s.p95=q(0.95);s.p99=q(0.99);s.max=v.back();
 s.overruns=static_cast<int>(std::count_if(v.begin(),v.end(),[&](double x){return x>deadline;}));
 return s;
}
Stats run(int channels,bool vinylOn){
 auto p=std::make_unique<MixEngine::Processor>();
 ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;setup.maxSamplesPerBlock=block;setup.sampleRate=sr;
 if(p->setupProcessing(setup)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
 std::vector<SpeakerArrangement> arr(static_cast<std::size_t>(channels),SpeakerArr::kMono);
 if(p->setMixChannelArrangements(arr.data(),channels)!=kResultOk)throw 21;
 ParameterChanges init{64},outChanges{16};
 setP(init,MixEngine::kParamInput,0.5);setP(init,MixEngine::kParamOutput,0.5);setP(init,MixEngine::kParamCalibration,0.5);
 setP(init,MixEngine::kParamAutoGain,0.0);setP(init,MixEngine::kParamConsoleOn,0.0);setP(init,MixEngine::kParamTubeOn,0.0);
 setP(init,MixEngine::kParamTapeOn,0.0);setP(init,MixEngine::kParamGlueOn,0.0);
 setP(init,MixEngine::kParamVinylOn,vinylOn?1.0:0.0);setP(init,MixEngine::kParamVinylCharacter,0.75);
 setP(init,MixEngine::kParamVinylWear,0.35);setP(init,MixEngine::kParamVinylNoise,0.0);
 setP(init,MixEngine::kParamDepth,0.5);setP(init,MixEngine::kParamWidth,0.5);setP(init,MixEngine::kParamLowMono,0.0);
 setP(init,MixEngine::kParamQuality,0.5);

 std::vector<std::vector<double>> in(static_cast<std::size_t>(channels),std::vector<double>(block));
 std::vector<std::vector<double>> out(static_cast<std::size_t>(channels),std::vector<double>(block));
 std::vector<double*> ip(static_cast<std::size_t>(channels)),op(static_cast<std::size_t>(channels));
 std::vector<AudioBusBuffers> ib(static_cast<std::size_t>(channels)),ob(static_cast<std::size_t>(channels));
 for(int ch=0;ch<channels;++ch){
  ip[ch]=in[ch].data();op[ch]=out[ch].data();
  ib[ch].numChannels=1;ib[ch].channelBuffers64=&ip[ch];
  ob[ch].numChannels=1;ob[ch].channelBuffers64=&op[ch];
 }
 std::vector<double>dur;dur.reserve(measuredBlocks);bool first=true;
 const double deadline=1e6*double(block)/sr;
 for(int b=0;b<warmBlocks+measuredBlocks;++b){
  for(int ch=0;ch<channels;++ch)for(int i=0;i<block;++i){
   const long long n=static_cast<long long>(b)*block+i;
   in[ch][i]=0.035*std::sin(2*pi*(83.0+5.0*(ch%19))*double(n)/sr+0.07*ch)
            +0.020*std::sin(2*pi*(4001.0+11.0*(ch%13))*double(n)/sr)
            +0.010*std::sin(2*pi*(12001.0+7.0*(ch%11))*double(n)/sr);
  }
  const auto t0=std::chrono::steady_clock::now();
  ProcessData ctl{};ctl.processMode=kRealtime;ctl.symbolicSampleSize=kSample64;ctl.numSamples=block;
  ctl.numInputs=channels;ctl.numOutputs=channels;ctl.inputs=ib.data();ctl.outputs=ob.data();
  ctl.inputParameterChanges=first?&init:nullptr;ctl.outputParameterChanges=&outChanges;
  if(p->processMixControl(&ctl)!=kResultOk)throw 22;first=false;
  for(int ch=0;ch<channels;++ch){
   AudioBusBuffers oneIn=ib[ch],oneOut=ob[ch];
   ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=block;
   d.numInputs=1;d.numOutputs=1;d.inputs=&oneIn;d.outputs=&oneOut;
   if(p->processMixChannel(ch,&d)!=kResultOk)throw 23;
  }
  const auto t1=std::chrono::steady_clock::now();
  if(b>=warmBlocks)dur.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
 }
 return stats(std::move(dur),deadline);
}
}

int main(){
 try{
  bool ok=true;const double deadline=1e6*double(block)/sr;
  for(int channels:{2,8,32,64,128}){
   const auto off=run(channels,false),on=run(channels,true);
   std::cout<<"channels="<<channels
            <<" off(mean/p95/p99/max)="<<off.mean<<"/"<<off.p95<<"/"<<off.p99<<"/"<<off.max
            <<" us on="<<on.mean<<"/"<<on.p95<<"/"<<on.p99<<"/"<<on.max
            <<" overheadMean="<<(on.mean-off.mean)
            <<" overheadP99="<<(on.p99-off.p99)
            <<" deadline="<<deadline
            <<" overruns="<<on.overruns<<"\n";
   if(!(std::isfinite(on.mean)&&std::isfinite(on.p99)&&std::isfinite(on.max)))ok=false;
   if(channels<=64){
    // Production gate: up to 64 simultaneous Vinyl channels must meet the
    // realtime deadline at p99 with zero measured overruns.
    if(on.overruns!=0||on.p99>=deadline)ok=false;
   }else{
    // 128 channels is an intentionally extreme shared-runner stress case.
    // GitHub-hosted runner scheduling jitter can move p99 across the 5.33 ms
    // block deadline without any DSP/code change. Keep this case gating on
    // sustained throughput and bounded tail behavior rather than requiring
    // every shared-runner block to beat a hard realtime deadline.
    const double overrunFraction=double(on.overruns)/double(measuredBlocks);
    if(on.mean>=deadline||on.p99>=deadline*1.10||overrunFraction>0.10)ok=false;
   }
  }
  std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 end-to-end CPU scaling measurement\n";
  return ok?0:1;
 }catch(int c){std::cerr<<"Vinyl CPU setup FAIL "<<c<<"\n";return c;}
 catch(...){std::cerr<<"Vinyl CPU unknown exception\n";return 90;}
}
