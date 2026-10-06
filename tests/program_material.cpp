#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double sr=48000.0,pi=3.14159265358979323846;
constexpr int block=256,total=48000;

void setP(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto*q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}
double noise(uint32_t& s){s=1664525u*s+1013904223u;return (double((s>>8)&0xFFFFFF)/8388607.5)-1.0;}

struct Stats{double peak=0,rms=0,crest=0,mean=0;};

std::vector<double> makeProgram(){
    std::vector<double> x(total);
    uint32_t rng=0x125A300u;
    for(int n=0;n<total;++n){
        const double t=double(n)/sr;
        const double beat=std::fmod(t,0.5);
        const double kickEnv=std::exp(-beat*18.0);
        const double kick=0.34*kickEnv*std::sin(2*pi*(52.0+36.0*std::exp(-beat*35.0))*beat);

        const double bass=0.16*std::sin(2*pi*55.0*t)+0.07*std::sin(2*pi*110.0*t+0.2);
        const double snarePhase=std::fmod(t+0.25,0.5);
        const double snareEnv=std::exp(-snarePhase*28.0);
        const double snare=0.11*snareEnv*noise(rng)+0.05*snareEnv*std::sin(2*pi*190.0*t);

        const double pad=0.05*std::sin(2*pi*220.0*t)+0.04*std::sin(2*pi*329.63*t+0.4)+0.03*std::sin(2*pi*440.0*t+0.7);
        const double pluckEnv=std::exp(-std::fmod(t,0.25)*10.0);
        const double synth=0.06*pluckEnv*(std::sin(2*pi*880.0*t)+0.35*std::sin(2*pi*1760.0*t));

        x[(size_t)n]=kick+bass+snare+pad+synth;
    }
    return x;
}

std::vector<double> render(bool mixfx,const std::vector<double>& in){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=kSample64;s.maxSamplesPerBlock=block;s.sampleRate=sr;
    if(p->setupProcessing(s)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;
    if(mixfx){SpeakerArrangement a=SpeakerArr::kStereo;if(p->setMixChannelArrangements(&a,1)!=kResultOk)throw 21;}

    ParameterChanges init{64},outChanges{16};
    const std::pair<ParamID,double> cfg[]={
      {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},
      {MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,1.0},
      {MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,2.0/3.0},{MixEngine::kParamConsoleDrive,0.52},{MixEngine::kParamConsoleNoise,0.0},
      {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.38},{MixEngine::kParamTubeType,0.5},
      {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.42},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.92},{MixEngine::kParamTapeHiss,0.0},
      {MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.28},{MixEngine::kParamGlueCharacter,0.55},
      {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.28},{MixEngine::kParamVinylWear,0.12},{MixEngine::kParamVinylNoise,0.0},
      {MixEngine::kParamDepth,0.55},{MixEngine::kParamWidth,0.58},{MixEngine::kParamLowMono,0.25},{MixEngine::kParamQuality,0.5}
    };
    for(auto [id,v]:cfg)setP(init,id,v);

    std::vector<double> y(in.size());
    bool first=true;
    for(int base=0;base<(int)in.size();base+=block){
        const int n=std::min(block,(int)in.size()-base);
        std::array<double,block> l{},r{},ol{},orr{};
        for(int i=0;i<n;++i){l[i]=in[(size_t)(base+i)];r[i]=0.91*l[i]+0.015*std::sin(2*pi*307.0*(base+i)/sr);}
        double*ip[2]{l.data(),r.data()},*op[2]{ol.data(),orr.data()};
        AudioBusBuffers ib{},ob{};ib.numChannels=2;ib.channelBuffers64=ip;ob.numChannels=2;ob.channelBuffers64=op;
        if(mixfx){
            ProcessData ctl{};ctl.processMode=kRealtime;ctl.symbolicSampleSize=kSample64;ctl.numSamples=n;ctl.inputParameterChanges=first?&init:nullptr;ctl.outputParameterChanges=&outChanges;
            if(p->processMixControl(&ctl)!=kResultOk)throw 22;
            ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;
            if(p->processMixChannel(0,&d)!=kResultOk)throw 23;
        }else{
            ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=n;d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.inputParameterChanges=first?&init:nullptr;d.outputParameterChanges=&outChanges;
            if(p->process(d)!=kResultOk)throw 24;
        }
        first=false;
        for(int i=0;i<n;++i){
            if(!std::isfinite(ol[i])||!std::isfinite(orr[i]))throw 25;
            y[(size_t)(base+i)]=0.5*(ol[i]+orr[i]);
        }
    }
    return y;
}

Stats stats(const std::vector<double>&x){
    Stats s; long double ss=0,sm=0;
    for(double v:x){s.peak=std::max(s.peak,std::abs(v));ss+=v*v;sm+=v;}
    s.rms=std::sqrt(double(ss/x.size()));s.mean=double(sm/x.size());s.crest=s.rms>1e-15?s.peak/s.rms:0;
    return s;
}
}

int main(){try{
    const auto in=makeProgram();
    const auto ch=render(false,in),mx=render(true,in);
    const auto si=stats(in),sc=stats(ch),sm=stats(mx);
    double maxDiff=0;for(size_t i=0;i<ch.size();++i)maxDiff=std::max(maxDiff,std::abs(ch[i]-mx[i]));
    std::cout<<"program input peak="<<si.peak<<" rms="<<si.rms<<" crest="<<si.crest<<"\n";
    std::cout<<"channel peak="<<sc.peak<<" rms="<<sc.rms<<" crest="<<sc.crest<<" mean="<<sc.mean<<"\n";
    std::cout<<"mixfx peak="<<sm.peak<<" rms="<<sm.rms<<" crest="<<sm.crest<<" mean="<<sm.mean<<" maxDiff="<<maxDiff<<"\n";
    const bool ok=sc.peak<4.0&&sm.peak<4.0&&sc.rms>0.01&&sm.rms>0.01&&std::abs(sc.mean)<0.1&&std::abs(sm.mean)<0.1&&maxDiff<1e-10;
    std::cout<<(ok?"PASS":"FAIL")<<": deterministic program-material fixture\n";
    return ok?0:1;
}catch(int c){std::cerr<<"Program fixture setup FAIL "<<c<<"\n";return c;}catch(...){return 90;}}
