#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int32 kBlockSize = 256;
constexpr int kTotal = 16384;
constexpr int kWarmup = 4096;
constexpr double kPi = 3.14159265358979323846;

void setParam(ParameterChanges& changes, ParamID id, double value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    if (!queue) throw 10;
    int32 pointIndex = 0;
    if (queue->addPoint(0, value, pointIndex) != kResultTrue) throw 11;
}

struct RenderResult {
    std::vector<float> l;
    std::vector<float> r;
};

RenderResult render(double sampleRate,
                    const std::vector<float>& inL,
                    const std::vector<float>& inR,
                    const std::vector<std::pair<ParamID,double>>& overrides) {
    if (inL.size() != inR.size() || inL.size() != static_cast<std::size_t>(kTotal))
        throw 12;

    auto processor = std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = sampleRate;
    if (processor->setupProcessing(setup) != kResultOk) throw 20;
    if (processor->setProcessing(true) != kResultOk) throw 21;
    if (processor->getLatencySamples() != MixEngine::v3ReportedLatencySamples(sampleRate)) throw 22;

    std::array<double, MixEngine::kParamCount> values{};
    std::array<bool, MixEngine::kParamCount> set{};

    auto put=[&](ParamID id,double value){
        if(id>=MixEngine::kParamCount) throw 23;
        values[static_cast<std::size_t>(id)] = value;
        set[static_cast<std::size_t>(id)] = true;
    };

    // Phase/mono baseline: disable coloration and noise so the stereo stage can
    // be tested independently through the actual Processor path.
    put(MixEngine::kParamConsoleOn, 0.0);
    put(MixEngine::kParamTubeOn, 0.0);
    put(MixEngine::kParamTapeOn, 0.0);
    put(MixEngine::kParamGlueOn, 0.0);
    put(MixEngine::kParamVinylOn, 0.0);
    put(MixEngine::kParamConsoleNoise, 0.0);
    put(MixEngine::kParamTapeHiss, 0.0);
    put(MixEngine::kParamVinylNoise, 0.0);
    put(MixEngine::kParamInput, 0.5);
    put(MixEngine::kParamOutput, 0.5);
    put(MixEngine::kParamWidth, 0.5);
    put(MixEngine::kParamDepth, 0.5);
    put(MixEngine::kParamLowMono, 0.0);
    put(MixEngine::kParamConsoleCrosstalk, 0.0);
    for (const auto& [id,value] : overrides) put(id,value);

    ParameterChanges inputChanges{64};
    ParameterChanges outputChanges{16};
    for (ParamID id=0; id<MixEngine::kParamCount; ++id)
        if (set[static_cast<std::size_t>(id)])
            setParam(inputChanges,id,values[static_cast<std::size_t>(id)]);

    RenderResult out{std::vector<float>(kTotal,0.f),std::vector<float>(kTotal,0.f)};
    std::array<float,kBlockSize> blockInL{},blockInR{},blockOutL{},blockOutR{};
    float* inPtrs[2]{blockInL.data(),blockInR.data()};
    float* outPtrs[2]{blockOutL.data(),blockOutR.data()};
    AudioBusBuffers inBus{},outBus{};
    inBus.numChannels=2; inBus.channelBuffers32=inPtrs;
    outBus.numChannels=2; outBus.channelBuffers32=outPtrs;

    bool first=true;
    for(int base=0;base<kTotal;base+=kBlockSize){
        const int count=std::min<int>(kBlockSize,kTotal-base);
        std::fill(blockInL.begin(),blockInL.end(),0.f);
        std::fill(blockInR.begin(),blockInR.end(),0.f);
        std::fill(blockOutL.begin(),blockOutL.end(),0.f);
        std::fill(blockOutR.begin(),blockOutR.end(),0.f);
        for(int i=0;i<count;++i){
            blockInL[static_cast<std::size_t>(i)]=inL[static_cast<std::size_t>(base+i)];
            blockInR[static_cast<std::size_t>(i)]=inR[static_cast<std::size_t>(base+i)];
        }

        outputChanges.clearQueue();
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=count;
        data.numInputs=1; data.numOutputs=1;
        data.inputs=&inBus; data.outputs=&outBus;
        data.inputParameterChanges=first?&inputChanges:nullptr;
        data.outputParameterChanges=&outputChanges;
        if(processor->process(data)!=kResultOk) throw 24;
        first=false;

        for(int i=0;i<count;++i){
            out.l[static_cast<std::size_t>(base+i)]=blockOutL[static_cast<std::size_t>(i)];
            out.r[static_cast<std::size_t>(base+i)]=blockOutR[static_cast<std::size_t>(i)];
        }
    }
    return out;
}

RenderResult renderMixFx(double sampleRate,
                         const std::vector<float>& inL,
                         const std::vector<float>& inR,
                         const std::vector<std::pair<ParamID,double>>& overrides) {
    if (inL.size() != inR.size() || inL.size() != static_cast<std::size_t>(kTotal))
        throw 112;

    auto processor = std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = sampleRate;
    if (processor->setupProcessing(setup) != kResultOk) throw 120;
    if (processor->setProcessing(true) != kResultOk) throw 121;
    if (processor->getLatencySamples() != MixEngine::v3ReportedLatencySamples(sampleRate)) throw 122;

    SpeakerArrangement arrangement = SpeakerArr::kStereo;
    if (processor->setMixChannelArrangements(&arrangement, 1) != kResultOk) throw 123;

    std::array<double, MixEngine::kParamCount> values{};
    std::array<bool, MixEngine::kParamCount> set{};
    auto put=[&](ParamID id,double value){
        if(id>=MixEngine::kParamCount) throw 124;
        values[static_cast<std::size_t>(id)] = value;
        set[static_cast<std::size_t>(id)] = true;
    };

    put(MixEngine::kParamConsoleOn, 0.0);
    put(MixEngine::kParamTubeOn, 0.0);
    put(MixEngine::kParamTapeOn, 0.0);
    put(MixEngine::kParamGlueOn, 0.0);
    put(MixEngine::kParamVinylOn, 0.0);
    put(MixEngine::kParamConsoleNoise, 0.0);
    put(MixEngine::kParamTapeHiss, 0.0);
    put(MixEngine::kParamVinylNoise, 0.0);
    put(MixEngine::kParamInput, 0.5);
    put(MixEngine::kParamOutput, 0.5);
    put(MixEngine::kParamWidth, 0.5);
    put(MixEngine::kParamDepth, 0.5);
    put(MixEngine::kParamLowMono, 0.0);
    put(MixEngine::kParamConsoleCrosstalk, 0.0);
    for (const auto& [id,value] : overrides) put(id,value);

    ParameterChanges controlChanges{64};
    ParameterChanges controlOutput{16};
    for (ParamID id=0; id<MixEngine::kParamCount; ++id)
        if (set[static_cast<std::size_t>(id)])
            setParam(controlChanges,id,values[static_cast<std::size_t>(id)]);

    ProcessData control{};
    control.processMode = kRealtime;
    control.symbolicSampleSize = kSample32;
    control.numSamples = kBlockSize;
    control.inputParameterChanges = &controlChanges;
    control.outputParameterChanges = &controlOutput;
    if (processor->processMixControl(&control) != kResultOk) throw 125;

    RenderResult out{std::vector<float>(kTotal,0.f),std::vector<float>(kTotal,0.f)};
    std::array<float,kBlockSize> blockInL{},blockInR{},blockOutL{},blockOutR{};
    float* inPtrs[2]{blockInL.data(),blockInR.data()};
    float* outPtrs[2]{blockOutL.data(),blockOutR.data()};
    AudioBusBuffers inBus{},outBus{};
    inBus.numChannels=2; inBus.channelBuffers32=inPtrs;
    outBus.numChannels=2; outBus.channelBuffers32=outPtrs;

    for(int base=0;base<kTotal;base+=kBlockSize){
        const int count=std::min<int>(kBlockSize,kTotal-base);
        std::fill(blockInL.begin(),blockInL.end(),0.f);
        std::fill(blockInR.begin(),blockInR.end(),0.f);
        std::fill(blockOutL.begin(),blockOutL.end(),0.f);
        std::fill(blockOutR.begin(),blockOutR.end(),0.f);
        for(int i=0;i<count;++i){
            blockInL[static_cast<std::size_t>(i)]=inL[static_cast<std::size_t>(base+i)];
            blockInR[static_cast<std::size_t>(i)]=inR[static_cast<std::size_t>(base+i)];
        }

        ProcessData channel{};
        channel.processMode=kRealtime;
        channel.symbolicSampleSize=kSample32;
        channel.numSamples=count;
        channel.numInputs=1; channel.numOutputs=1;
        channel.inputs=&inBus; channel.outputs=&outBus;
        if(processor->processMixChannel(0,&channel)!=kResultOk) throw 126;

        for(int i=0;i<count;++i){
            out.l[static_cast<std::size_t>(base+i)]=blockOutL[static_cast<std::size_t>(i)];
            out.r[static_cast<std::size_t>(base+i)]=blockOutR[static_cast<std::size_t>(i)];
        }
    }
    return out;
}

std::vector<float> makeSignal(double sampleRate,double phaseOffset=0.0) {
    std::vector<float> x(kTotal);
    for(int n=0;n<kTotal;++n){
        const double t=static_cast<double>(n)/sampleRate;
        x[static_cast<std::size_t>(n)]=static_cast<float>(
            0.31*std::sin(2.0*kPi*97.0*t+phaseOffset) +
            0.23*std::sin(2.0*kPi*997.0*t+0.21+phaseOffset) +
            0.14*std::sin(2.0*kPi*6031.0*t+0.47+phaseOffset) +
            0.07*std::sin(2.0*kPi*12011.0*t+0.73+phaseOffset));
    }
    return x;
}

double maxMonoSumError(const std::vector<float>& inL,const std::vector<float>& inR,
                       const RenderResult& out,double sampleRate) {
    const int d=MixEngine::v3ReportedLatencySamples(sampleRate);
    double maxErr=0.0;
    for(int n=kWarmup;n<kTotal;++n){
        const int src=n-d;
        if(src<0) continue;
        const double expected=0.5*(static_cast<double>(inL[static_cast<std::size_t>(src)])+
                                   static_cast<double>(inR[static_cast<std::size_t>(src)]));
        const double actual=0.5*(static_cast<double>(out.l[static_cast<std::size_t>(n)])+
                                 static_cast<double>(out.r[static_cast<std::size_t>(n)]));
        maxErr=std::max(maxErr,std::abs(actual-expected));
    }
    return maxErr;
}

double maxDifference(const RenderResult& out) {
    double m=0.0;
    for(int n=kWarmup;n<kTotal;++n)
        m=std::max(m,std::abs(static_cast<double>(out.l[static_cast<std::size_t>(n)])-
                              static_cast<double>(out.r[static_cast<std::size_t>(n)])));
    return m;
}

double maxSum(const RenderResult& out) {
    double m=0.0;
    for(int n=kWarmup;n<kTotal;++n)
        m=std::max(m,std::abs(static_cast<double>(out.l[static_cast<std::size_t>(n)])+
                              static_cast<double>(out.r[static_cast<std::size_t>(n)])));
    return m;
}

double correlationAtLag(const std::vector<float>& a,const std::vector<float>& b,int lag) {
    long double ab=0.0,aa=0.0,bb=0.0;
    const int start=kWarmup+std::max(0,-lag);
    const int end=kTotal-std::max(0,lag);
    for(int n=start;n<end;++n){
        const double x=a[static_cast<std::size_t>(n)];
        const double y=b[static_cast<std::size_t>(n+lag)];
        ab+=static_cast<long double>(x)*y;
        aa+=static_cast<long double>(x)*x;
        bb+=static_cast<long double>(y)*y;
    }
    const long double denom=std::sqrt(aa*bb);
    return denom>0.0?static_cast<double>(ab/denom):0.0;
}

bool stereoStageChecks(double sr) {
    const auto l=makeSignal(sr,0.0);
    const auto r=makeSignal(sr,0.37);
    const std::vector<std::vector<std::pair<ParamID,double>>> configs={
        {{MixEngine::kParamWidth,0.5},{MixEngine::kParamDepth,0.5},{MixEngine::kParamLowMono,0.0}},
        {{MixEngine::kParamWidth,1.0},{MixEngine::kParamDepth,0.5},{MixEngine::kParamLowMono,0.0}},
        {{MixEngine::kParamWidth,0.5},{MixEngine::kParamDepth,1.0},{MixEngine::kParamLowMono,0.0}},
        {{MixEngine::kParamWidth,0.5},{MixEngine::kParamDepth,0.0},{MixEngine::kParamLowMono,0.0}},
        {{MixEngine::kParamWidth,0.5},{MixEngine::kParamDepth,0.5},{MixEngine::kParamLowMono,1.0}},
        {{MixEngine::kParamWidth,0.85},{MixEngine::kParamDepth,0.82},{MixEngine::kParamLowMono,0.73}}
    };

    bool ok=true;
    for(std::size_t i=0;i<configs.size();++i){
        const auto out=render(sr,l,r,configs[i]);
        const auto mix=renderMixFx(sr,l,r,configs[i]);
        const double monoErr=maxMonoSumError(l,r,out,sr);
        const double mixMonoErr=maxMonoSumError(l,r,mix,sr);
        std::cout<<"sr="<<sr<<" stereo-config="<<i
                 <<" channel mono-sum max error="<<monoErr
                 <<" mixfx mono-sum max error="<<mixMonoErr<<"\n";
        ok=ok && monoErr<2.0e-6 && mixMonoErr<2.0e-6;
    }

    const auto mono=makeSignal(sr,0.0);
    const auto outMono=render(sr,mono,mono,{
        {MixEngine::kParamWidth,1.0},{MixEngine::kParamDepth,1.0},{MixEngine::kParamLowMono,1.0}});
    const auto mixMono=renderMixFx(sr,mono,mono,{
        {MixEngine::kParamWidth,1.0},{MixEngine::kParamDepth,1.0},{MixEngine::kParamLowMono,1.0}});
    const double lrDiff=maxDifference(outMono);
    const double mixLrDiff=maxDifference(mixMono);
    std::cout<<"sr="<<sr<<" identical-LR channel diff="<<lrDiff
             <<" mixfx diff="<<mixLrDiff<<"\n";
    ok=ok && lrDiff<2.0e-6 && mixLrDiff<2.0e-6;

    std::vector<float> antiR=mono;
    for(auto& v:antiR)v=-v;
    const auto antiCfg=std::vector<std::pair<ParamID,double>>{
        {MixEngine::kParamWidth,1.0},{MixEngine::kParamDepth,0.0},{MixEngine::kParamLowMono,0.0}};
    const auto outAnti=render(sr,mono,antiR,antiCfg);
    const auto mixAnti=renderMixFx(sr,mono,antiR,antiCfg);
    const double lrSum=maxSum(outAnti);
    const double mixLrSum=maxSum(mixAnti);
    std::cout<<"sr="<<sr<<" anti-phase channel sum="<<lrSum
             <<" mixfx sum="<<mixLrSum<<"\n";
    ok=ok && lrSum<2.0e-6 && mixLrSum<2.0e-6;
    return ok;
}

bool fullChainZeroLag(double sr) {
    const auto mono=makeSignal(sr,0.0);
    const std::vector<std::pair<ParamID,double>> cfg={
        {MixEngine::kParamConsoleOn,1.0},
        {MixEngine::kParamConsoleDrive,0.65},
        {MixEngine::kParamConsoleCrosstalk,0.0},
        {MixEngine::kParamConsoleNoise,0.0},
        {MixEngine::kParamTubeOn,1.0},
        {MixEngine::kParamTubeAmount,0.60},
        {MixEngine::kParamTapeOn,1.0},
        {MixEngine::kParamTapeAmount,0.55},
        {MixEngine::kParamTapeStability,0.90},
        {MixEngine::kParamTapeHiss,0.0},
        {MixEngine::kParamGlueOn,1.0},
        {MixEngine::kParamGlueAmount,0.45},
        {MixEngine::kParamVinylOn,1.0},
        {MixEngine::kParamVinylCharacter,0.45},
        {MixEngine::kParamVinylWear,0.20},
        {MixEngine::kParamVinylNoise,0.0},
        {MixEngine::kParamQuality,1.0},
        {MixEngine::kParamWidth,0.5},
        {MixEngine::kParamDepth,0.5},
        {MixEngine::kParamLowMono,0.0}
    };
    const auto out=render(sr,mono,mono,cfg);
    const auto mix=renderMixFx(sr,mono,mono,cfg);

    auto bestLagFor=[&](const char* name,const RenderResult& x){
        int bestLag=99;
        double best=-std::numeric_limits<double>::infinity();
        for(int lag=-4;lag<=4;++lag){
            const double c=correlationAtLag(x.l,x.r,lag);
            if(c>best){best=c;bestLag=lag;}
            std::cout<<name<<" full-chain sr="<<sr<<" lag="<<lag<<" corr="<<c<<"\n";
        }
        std::cout<<name<<" full-chain best lag="<<bestLag<<" samples corr="<<best<<"\n";
        return std::pair<int,double>{bestLag,best};
    };
    const auto channelBest=bestLagFor("channel",out);
    const auto mixBest=bestLagFor("mixfx",mix);
    return channelBest.first==0 && channelBest.second>0.995 &&
           mixBest.first==0 && mixBest.second>0.995;
}
}

int main(){
    try{
        bool ok=true;
        for(double sr:{44100.0,48000.0,96000.0})
            ok=stereoStageChecks(sr)&&ok;
        ok=fullChainZeroLag(48000.0)&&ok;
        if(!ok){
            std::cerr<<"Phase/mono compatibility diagnostic FAILED\n";
            return 1;
        }
        std::cout<<"Phase/mono compatibility diagnostic PASSED\n";
        return 0;
    }catch(int code){
        std::cerr<<"Phase/mono diagnostic setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Phase/mono diagnostic unknown exception\n";
        return 90;
    }
}
