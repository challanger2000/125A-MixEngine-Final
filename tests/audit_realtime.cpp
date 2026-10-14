#include "../source/processor.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <thread>
#include <string>
#include <vector>
#if defined(__linux__)
#include <time.h>
#endif

namespace {thread_local bool track=false;thread_local std::size_t allocations=0;}
void* operator new(std::size_t n){if(track)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
using namespace Steinberg;
using namespace Steinberg::Vst;
using Clock=std::chrono::steady_clock;
namespace {
void check(bool value){if(!value)throw 1;}
void set(ParameterChanges& c,ParamID id,double value,int offset=0){int32 qi=0,pi=0;auto*q=c.addParameterData(id,qi);check(q&&q->addPoint(offset,value,pi)==kResultTrue);}
template<class Sample>bool run(double sr,int block,int channels,bool paced,bool dense=false){
 auto p=std::make_unique<MixEngine::Processor>();
 check(p->initialize(nullptr)==kResultOk);
 SpeakerArrangement arrangement=channels==1?SpeakerArr::kMono:SpeakerArr::kStereo;
 check(p->setBusArrangements(&arrangement,1,&arrangement,1)==kResultOk);
 ProcessSetup s{};s.sampleRate=sr;s.maxSamplesPerBlock=block;s.symbolicSampleSize=sizeof(Sample)==4?kSample32:kSample64;s.processMode=kRealtime;
 check(p->setupProcessing(s)==kResultOk&&p->setActive(true)==kResultOk);
 const auto start=Clock::now();check(p->setProcessing(true)==kResultOk);
 const double startUs=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
 ParameterChanges c{64};
 for(auto id:{MixEngine::kParamConsoleOn,MixEngine::kParamTubeOn,MixEngine::kParamTapeOn,MixEngine::kParamGlueOn,MixEngine::kParamVinylOn})set(c,id,1);
 for(auto id:{MixEngine::kParamConsoleDrive,MixEngine::kParamTubeAmount,MixEngine::kParamTapeAmount,MixEngine::kParamGlueAmount,MixEngine::kParamVinylCharacter})set(c,id,.75);
 set(c,MixEngine::kParamQuality,1);
 std::array<Sample,256> l{},r{},ol{},orr{};Sample* ip[]{l.data(),r.data()};Sample* op[]{ol.data(),orr.data()};
 AudioBusBuffers ib{},ob{};ib.numChannels=ob.numChannels=channels;
 if constexpr(sizeof(Sample)==4){ib.channelBuffers32=ip;ob.channelBuffers32=op;}else{ib.channelBuffers64=ip;ob.channelBuffers64=op;}
 ProcessData d{};d.numSamples=block;d.symbolicSampleSize=s.symbolicSampleSize;d.processMode=kRealtime;d.numInputs=d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;
 std::vector<double> cpuTimings;cpuTimings.reserve(600);
 std::vector<double> timings;timings.reserve(600);int overruns=0;bool finite=true;
 const double deadline=1e6*block/sr;auto next=Clock::now();allocations=0;
 for(int b=0;b<620;++b){
  for(int i=0;i<block;++i){const double t=double(b*block+i)/sr;l[i]=Sample(.18*std::sin(6.283185307179586*997*t));r[i]=Sample(.12*std::sin(6.283185307179586*73*t));}
  if(dense&&b>0){
   c.clearQueue();
   for(auto id:{MixEngine::kParamInput,MixEngine::kParamOutput,MixEngine::kParamConsoleDrive,
                MixEngine::kParamTubeAmount,MixEngine::kParamTapeAmount,MixEngine::kParamGlueAmount,
                MixEngine::kParamVinylCharacter,MixEngine::kParamVinylWear,MixEngine::kParamWidth,
                MixEngine::kParamDepth,MixEngine::kParamLowMono})
    set(c,id,b%2?.6:.75,block-1);
  }
  d.inputParameterChanges=(b==0||dense)?&c:nullptr;
  if(paced){next+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(block/sr));std::this_thread::sleep_until(next);}
#if defined(__linux__)
  timespec ca{},cz{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ca);
#endif
  track=true;const auto a=Clock::now();const auto result=p->process(d);const auto z=Clock::now();track=false;check(result==kResultOk);
#if defined(__linux__)
  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cz);
  if(b>=20)cpuTimings.push_back((cz.tv_sec-ca.tv_sec)*1e6+(cz.tv_nsec-ca.tv_nsec)*.001);
#endif
  const double us=std::chrono::duration<double,std::micro>(z-a).count();
  if(b>=20){timings.push_back(us);if(us>deadline)++overruns;}
  for(int i=0;i<block;++i)finite=finite&&std::isfinite(ol[i])&&std::isfinite(orr[i]);
 }
 std::sort(timings.begin(),timings.end());double mean=0;for(double t:timings)mean+=t;mean/=timings.size();
 std::cout<<"sr="<<sr<<" block="<<block<<" channels="<<channels<<" bits="<<sizeof(Sample)*8<<" paced="<<paced
 <<" denseAutomation="<<dense<<" mean/p95/p99/maxUs="<<mean<<"/"<<timings[570]<<"/"<<timings[594]<<"/"<<timings.back()
 <<" deadlineUs="<<deadline<<" overruns="<<overruns<<" newCalls="<<allocations<<" setProcessingUs="<<startUs<<"\n";
 if(!cpuTimings.empty()){std::sort(cpuTimings.begin(),cpuTimings.end());
 std::cout<<"threadCpuP99/maxUs="<<cpuTimings[594]<<"/"<<cpuTimings.back()<<" (separate CPU timer instrumentation included)\n";}
 p->setProcessing(false);p->setActive(false);p->terminate();
 return finite&&allocations==0&&(!paced||overruns==0);
}
}
int main(int argc,char** argv){try{
 const bool dense=argc==2&&std::string(argv[1])=="--automation";
 double overhead=0;for(int i=0;i<10000;++i){auto a=Clock::now();auto b=Clock::now();overhead+=std::chrono::duration<double,std::micro>(b-a).count();}
 std::cout<<"Clock-pair mean overheadUs="<<overhead/10000<<"\n";
 bool ok=true;
 if(dense){
  for(double sr:{48000.,192000.})ok=run<double>(sr,64,2,true,true)&&ok;
  std::cout<<(ok?"PASS":"FAIL")<<": dense continuous automation callback timing and C++ allocations\n";return ok?0:1;
 }
 for(double sr:{44100.,48000.,96000.,192000.})for(int channels:{1,2}){
  ok=run<float>(sr,64,channels,false)&&ok;ok=run<double>(sr,64,channels,true)&&ok;
 }
 for(int block:{1,7,127,256})ok=run<double>(48000.,block,2,false)&&ok;
 std::cout<<(ok?"PASS":"FAIL")<<": measured full-chain callback timing and C++ allocations\n";return ok?0:1;
}catch(...){track=false;return 2;}}
