#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kFs=48000.0;
constexpr int32 kBlock=256;
constexpr int kBlocks=16;
constexpr double kPi=3.14159265358979323846;
using Param=std::pair<ParamID,double>;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 q=0;auto* queue=changes.addParameterData(id,q);if(!queue)throw 10;
    int32 p=0;if(queue->addPoint(0,std::clamp(value,0.0,1.0),p)!=kResultTrue)throw 11;
}

struct Result{
    double boundaryPeak=0.0;
    double baselineRms=0.0;
    double ratio=0.0;
    double maxAbs=0.0;
};

Result measure(const std::vector<Param>& base,
               ParamID changed,double before,double after){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock;setup.sampleRate=kFs;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    std::vector<double> all(static_cast<std::size_t>(kBlocks*kBlock),0.0);
    long long samplePos=0;

    for(int block=0;block<kBlocks;++block){
        std::array<double,kBlock> in{},out{};
        for(int i=0;i<kBlock;++i,++samplePos){
            const double t=static_cast<double>(samplePos)/kFs;
            in[static_cast<std::size_t>(i)]=
                0.1258925412*std::sin(2.0*kPi*997.0*t)+
                0.028*std::sin(2.0*kPi*4013.0*t+0.3);
        }
        double* inPtr[1]{in.data()};double* outPtr[1]{out.data()};
        AudioBusBuffers inBus{},outBus{};
        inBus.numChannels=1;inBus.channelBuffers64=inPtr;
        outBus.numChannels=1;outBus.channelBuffers64=outPtr;

        ParameterChanges changes{64};
        IParameterChanges* changePtr=nullptr;
        if(block==0||block==8){
            for(const auto& q:std::vector<Param>{
                {MixEngine::kParamBypass,0.0},
                {MixEngine::kParamInput,0.5},
                {MixEngine::kParamOutput,0.5},
                {MixEngine::kParamCalibration,0.0},
                {MixEngine::kParamAutoGain,0.0},
                {MixEngine::kParamConsoleOn,0.0},
                {MixEngine::kParamTubeOn,0.0},
                {MixEngine::kParamTapeOn,0.0},
                {MixEngine::kParamGlueOn,0.0},
                {MixEngine::kParamVinylOn,0.0},
                {MixEngine::kParamConsoleNoise,0.0},
                {MixEngine::kParamTapeHiss,0.0},
                {MixEngine::kParamVinylNoise,0.0},
                {MixEngine::kParamWidth,0.5},
                {MixEngine::kParamDepth,0.5},
                {MixEngine::kParamLowMono,0.0},
                {MixEngine::kParamQuality,1.0}})
                setParam(changes,q.first,q.second);
            for(const auto&q:base)setParam(changes,q.first,q.second);
            setParam(changes,changed,block==0?before:after);
            changePtr=&changes;
        }

        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample64;
        data.numSamples=kBlock;data.numInputs=1;data.numOutputs=1;
        data.inputs=&inBus;data.outputs=&outBus;data.inputParameterChanges=changePtr;
        if(p->process(data)!=kResultOk)throw 22;
        for(int i=0;i<kBlock;++i)
            all[static_cast<std::size_t>(block*kBlock+i)]=out[static_cast<std::size_t>(i)];
    }

    Result r{};
    for(double v:all){
        if(!std::isfinite(v))throw 23;
        r.maxAbs=std::max(r.maxAbs,std::abs(v));
    }

    const int boundary=8*kBlock+MixEngine::kFixedLatencySamples;
    long double baseE=0.0;int baseN=0;
    for(int i=4*kBlock;i<7*kBlock;++i){
        const double d=all[static_cast<std::size_t>(i)]-
                       all[static_cast<std::size_t>(i-1)];
        baseE+=d*d;++baseN;
    }
    r.baselineRms=std::sqrt(static_cast<double>(baseE/baseN));

    for(int i=std::max(1,boundary-32);
        i<std::min(static_cast<int>(all.size()),boundary+96);++i){
        const double d=std::abs(all[static_cast<std::size_t>(i)]-
                                all[static_cast<std::size_t>(i-1)]);
        r.boundaryPeak=std::max(r.boundaryPeak,d);
    }
    r.ratio=r.boundaryPeak/std::max(r.baselineRms,1.0e-15);
    return r;
}

void report(const char* name,const Result&r){
    std::cout<<name<<" boundaryDerivative="<<r.boundaryPeak
             <<" baselineDerivativeRms="<<r.baselineRms
             <<" ratio="<<r.ratio
             <<" maxAbs="<<r.maxAbs<<"\n";
}
}

struct OffsetRampResult {
    double preGain=0.0;
    double rampGain=0.0;
    double postGain=0.0;
};

OffsetRampResult measureInBlockInputRamp(){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock;setup.sampleRate=kFs;
    if(p->setupProcessing(setup)!=kResultOk)throw 30;
    if(p->setProcessing(true)!=kResultOk)throw 31;

    auto runBlock=[&](ParameterChanges* changes,std::array<double,kBlock>& out){
        std::array<double,kBlock> in{};
        in.fill(0.10);
        out.fill(0.0);
        double* inPtr[1]{in.data()};double* outPtr[1]{out.data()};
        AudioBusBuffers inBus{},outBus{};
        inBus.numChannels=1;inBus.channelBuffers64=inPtr;
        outBus.numChannels=1;outBus.channelBuffers64=outPtr;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample64;
        data.numSamples=kBlock;data.numInputs=1;data.numOutputs=1;
        data.inputs=&inBus;data.outputs=&outBus;
        data.inputParameterChanges=changes;
        if(p->process(data)!=kResultOk)throw 32;
    };

    ParameterChanges init{64};
    setParam(init,MixEngine::kParamBypass,0.0);
    setParam(init,MixEngine::kParamInput,0.5);
    setParam(init,MixEngine::kParamOutput,0.5);
    setParam(init,MixEngine::kParamCalibration,0.0);
    setParam(init,MixEngine::kParamAutoGain,0.0);
    setParam(init,MixEngine::kParamConsoleOn,0.0);
    setParam(init,MixEngine::kParamTubeOn,0.0);
    setParam(init,MixEngine::kParamTapeOn,0.0);
    setParam(init,MixEngine::kParamGlueOn,0.0);
    setParam(init,MixEngine::kParamVinylOn,0.0);
    setParam(init,MixEngine::kParamWidth,0.5);
    setParam(init,MixEngine::kParamDepth,0.5);
    setParam(init,MixEngine::kParamLowMono,0.0);
    setParam(init,MixEngine::kParamQuality,1.0);

    std::array<double,kBlock> tmp{};
    runBlock(&init,tmp);
    for(int i=0;i<3;++i)runBlock(nullptr,tmp);

    ParameterChanges automation{8};
    int32 qIndex=0;
    auto* q=automation.addParameterData(MixEngine::kParamInput,qIndex);
    if(!q)throw 33;
    int32 pi=0;
    if(q->addPoint(0,0.5,pi)!=kResultTrue)throw 34;
    if(q->addPoint(64,0.5,pi)!=kResultTrue)throw 35;
    if(q->addPoint(128,0.75,pi)!=kResultTrue)throw 36;
    if(q->addPoint(192,0.75,pi)!=kResultTrue)throw 37;

    std::array<double,kBlock> out{};
    runBlock(&automation,out);

    const int latency=MixEngine::kFixedLatencySamples;
    auto meanAbs=[&](int start,int end){
        long double sum=0.0;int n=0;
        for(int i=start;i<end;++i){
            if(i<0||i>=kBlock)continue;
            sum+=std::abs(out[static_cast<std::size_t>(i)]);++n;
        }
        return n>0?static_cast<double>(sum/n):0.0;
    };

    OffsetRampResult r;
    // Delay windows by the fixed plugin latency so they refer to the automation
    // target positions at the DSP input.
    r.preGain=meanAbs(24+latency,56+latency)/0.10;
    r.rampGain=meanAbs(104+latency,128+latency)/0.10;
    r.postGain=meanAbs(208,240)/0.10;
    return r;
}

int main(){
    try{
        bool ok=true;
        const std::vector<std::tuple<const char*,std::vector<Param>,ParamID,double,double>> cases={
            {"Input",{},MixEngine::kParamInput,0.35,0.65},
            {"Output",{},MixEngine::kParamOutput,0.35,0.65},
            {"ConsoleDrive",{{MixEngine::kParamConsoleOn,1.0}},MixEngine::kParamConsoleDrive,0.10,0.90},
            {"ConsoleNoise",{{MixEngine::kParamConsoleOn,1.0}},MixEngine::kParamConsoleNoise,0.0,1.0},
            {"TubeAmount",{{MixEngine::kParamTubeOn,1.0}},MixEngine::kParamTubeAmount,0.0,1.0},
            {"TapeAmount",{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,1.0}},MixEngine::kParamTapeAmount,0.0,1.0},
            {"TapeHiss",{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeSpeed,0.5}},MixEngine::kParamTapeHiss,0.0,1.0},
            {"GlueAmount",{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueCharacter,0.5}},MixEngine::kParamGlueAmount,0.10,0.90},
            {"VinylColor",{{MixEngine::kParamVinylOn,1.0}},MixEngine::kParamVinylCharacter,0.0,1.0},
            {"VinylWear",{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.5}},MixEngine::kParamVinylWear,0.0,1.0},
            {"VinylSurface",{{MixEngine::kParamVinylOn,1.0}},MixEngine::kParamVinylNoise,0.0,1.0}
        };

        for(const auto& item:cases){
            const auto r=measure(std::get<1>(item),std::get<2>(item),
                                 std::get<3>(item),std::get<4>(item));
            report(std::get<0>(item),r);
            if(!std::isfinite(r.ratio)||r.maxAbs>8.0)ok=false;
        }

        const auto offset=measureInBlockInputRamp();
        std::cout<<"InBlockInputRamp preGain="<<offset.preGain
                 <<" rampGain="<<offset.rampGain
                 <<" postGain="<<offset.postGain<<"\n";
        // 0.5 is unity. The target stays flat through sample 64, ramps upward
        // through 128 and remains higher afterwards. Internal smoothing may
        // soften the response but must not move it ahead of the host curve.
        if(!std::isfinite(offset.preGain)||!std::isfinite(offset.rampGain)
           ||!std::isfinite(offset.postGain))ok=false;
        if(std::abs(offset.preGain-1.0)>0.03)ok=false;
        if(!(offset.rampGain>offset.preGain+0.01))ok=false;
        if(!(offset.postGain>offset.rampGain+0.05))ok=false;

        if(!ok){
            std::cerr<<"FAILED: automation transition characterization produced invalid output\n";
            return 1;
        }
        std::cout<<"PASSED: automation transition characterization measurement\n";
        return 0;
    }catch(int e){
        std::cerr<<"Automation transition setup failure: "<<e<<"\n";
        return e;
    }catch(...){
        std::cerr<<"Automation transition unknown failure\n";
        return 90;
    }
}
