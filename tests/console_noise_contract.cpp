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
constexpr int kBlock=256;
constexpr int kTotal=32768;
constexpr int kWarm=4096;
constexpr double kSr=48000.0;

void setP(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q) throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue) throw 11;
}

double rmsDb(const std::vector<double>& x){
    long double q=0.0; long long n=0;
    for(int i=kWarm;i<static_cast<int>(x.size());++i){q+=x[static_cast<std::size_t>(i)]*x[static_cast<std::size_t>(i)];++n;}
    const double r=n?std::sqrt(static_cast<double>(q/n)):0.0;
    return 20.0*std::log10(std::max(r,1.0e-15));
}

void commonParams(ParameterChanges& c,double noise){
    setP(c,MixEngine::kParamInput,0.5);
    setP(c,MixEngine::kParamOutput,0.5);
    setP(c,MixEngine::kParamCalibration,0.5);
    setP(c,MixEngine::kParamAutoGain,0.0);
    setP(c,MixEngine::kParamConsoleOn,1.0);
    setP(c,MixEngine::kParamConsoleDrive,0.0);
    setP(c,MixEngine::kParamConsoleNoise,noise);
    setP(c,MixEngine::kParamTubeOn,0.0);
    setP(c,MixEngine::kParamTapeOn,0.0);
    setP(c,MixEngine::kParamGlueOn,0.0);
    setP(c,MixEngine::kParamVinylOn,0.0);
    setP(c,MixEngine::kParamDepth,0.5);
    setP(c,MixEngine::kParamWidth,0.5);
    setP(c,MixEngine::kParamLowMono,0.0);
    setP(c,MixEngine::kParamQuality,0.5);
}

std::vector<double> renderChannel(double noise){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk||p->setProcessing(true)!=kResultOk) throw 20;

    ParameterChanges changes{64}, outChanges{16}; commonParams(changes,noise);
    std::vector<double> out(kTotal,0.0); bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        std::array<double,kBlock> in{}, y{};
        double* ip[1]{in.data()}; double* op[1]{y.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1;ib.channelBuffers64=ip;ob.numChannels=1;ob.channelBuffers64=op;
        ProcessData d{}; d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=kBlock;
        d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&changes;
        if(p->process(d)!=kResultOk) throw 21;
        first=false;changes.clearQueue();
        for(int i=0;i<kBlock;++i)out[static_cast<std::size_t>(base+i)]=y[static_cast<std::size_t>(i)];
    }
    return out;
}

std::vector<double> renderMixFxSum(double noise,int channels){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk||p->setProcessing(true)!=kResultOk) throw 30;
    std::vector<SpeakerArrangement> arr(static_cast<std::size_t>(channels),SpeakerArr::kMono);
    if(p->setMixChannelArrangements(arr.data(),channels)!=kResultOk) throw 31;

    ParameterChanges changes{64},outChanges{16}; commonParams(changes,noise);
    ProcessData control{}; control.processMode=kRealtime;control.symbolicSampleSize=kSample64;control.numSamples=kBlock;
    control.inputParameterChanges=&changes;control.outputParameterChanges=&outChanges;
    if(p->processMixControl(&control)!=kResultOk) throw 32;

    std::vector<std::vector<double>> rendered(static_cast<std::size_t>(channels),std::vector<double>(kTotal,0.0));
    for(int ch=0;ch<channels;++ch){
        for(int base=0;base<kTotal;base+=kBlock){
            std::array<double,kBlock> in{}, y{};
            double* ip[1]{in.data()}; double* op[1]{y.data()};
            AudioBusBuffers ib{},ob{}; ib.numChannels=1;ib.channelBuffers64=ip;ob.numChannels=1;ob.channelBuffers64=op;
            ProcessData d{}; d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=kBlock;
            d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.outputParameterChanges=&outChanges;
            if(p->processMixChannel(ch,&d)!=kResultOk) throw 33;
            for(int i=0;i<kBlock;++i)rendered[static_cast<std::size_t>(ch)][static_cast<std::size_t>(base+i)]=y[static_cast<std::size_t>(i)];
        }
    }
    std::vector<double> sum(kTotal,0.0);
    for(int ch=0;ch<channels;++ch)
        for(int i=0;i<kTotal;++i)sum[static_cast<std::size_t>(i)]+=rendered[static_cast<std::size_t>(ch)][static_cast<std::size_t>(i)];
    return sum;
}
}

int main(){
    try{
        bool ok=true;
        const std::array<double,5> amounts{0.0,0.25,0.50,0.75,1.0};
        double prev=-300.0;
        for(double a:amounts){
            const double d=rmsDb(renderChannel(a));
            std::cout<<"Channel Console Noise "<<int(a*100)<<"% RMS="<<d<<" dBFS\n";
            if(a==0.0){ if(d>-140.0)ok=false; }
            else{
                if(!(std::isfinite(d)&&d>prev+4.0))ok=false;
                prev=d;
            }
        }

        const double c25=rmsDb(renderChannel(0.25));
        const double c50=rmsDb(renderChannel(0.50));
        const double c75=rmsDb(renderChannel(0.75));
        const double c100=rmsDb(renderChannel(1.0));
        if(c25<-86.0||c25>-73.0)ok=false;
        if(c50<-73.0||c50>-62.0)ok=false;
        if(c75<-65.0||c75>-56.0)ok=false;
        if(c100<-60.0||c100>-50.0)ok=false;

        const double mix8=rmsDb(renderMixFxSum(1.0,8));
        std::cout<<"MixFX summed 8ch Console Noise 100% RMS="<<mix8<<" dBFS\n";
        // 1/sqrt(N) per-channel scaling should keep the summed console floor
        // close to the single-channel reference, not vanish and not explode.
        if(!std::isfinite(mix8)||std::abs(mix8-c100)>2.0)ok=false;

        std::cout<<(ok?"PASS":"FAIL")<<": Console Noise audibility/scaling contract\n";
        return ok?0:1;
    }catch(int code){std::cerr<<"Console Noise setup FAIL "<<code<<"\n";return code;}
    catch(...){std::cerr<<"Console Noise unknown exception\n";return 90;}
}
