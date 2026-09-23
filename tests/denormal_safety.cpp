#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <xmmintrin.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kBlock=256;
constexpr double kSr=48000.0;

void addPoint(ParameterChanges& changes,ParamID id,double value){
    int32 qi=0; auto* q=changes.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(value,0.0,1.0),pi)!=kResultTrue)throw 11;
}

void configure(ParameterChanges& c){
    addPoint(c,MixEngine::kParamBypass,0.0);
    addPoint(c,MixEngine::kParamInput,0.5);
    addPoint(c,MixEngine::kParamOutput,0.5);
    addPoint(c,MixEngine::kParamCalibration,0.5);
    addPoint(c,MixEngine::kParamAutoGain,1.0);
    addPoint(c,MixEngine::kParamConsoleOn,1.0);
    addPoint(c,MixEngine::kParamConsoleMode,2.0/3.0);
    addPoint(c,MixEngine::kParamConsoleDrive,0.70);
    addPoint(c,MixEngine::kParamConsoleNoise,0.0);
    addPoint(c,MixEngine::kParamTubeOn,1.0);
    addPoint(c,MixEngine::kParamTubeAmount,0.70);
    addPoint(c,MixEngine::kParamTubeType,0.5);
    addPoint(c,MixEngine::kParamTapeOn,1.0);
    addPoint(c,MixEngine::kParamTapeAmount,0.70);
    addPoint(c,MixEngine::kParamTapeSpeed,0.5);
    addPoint(c,MixEngine::kParamTapeStability,0.75);
    addPoint(c,MixEngine::kParamTapeHiss,0.0);
    addPoint(c,MixEngine::kParamGlueOn,1.0);
    addPoint(c,MixEngine::kParamGlueAmount,0.55);
    addPoint(c,MixEngine::kParamGlueCharacter,0.5);
    addPoint(c,MixEngine::kParamVinylOn,1.0);
    addPoint(c,MixEngine::kParamVinylCharacter,0.55);
    addPoint(c,MixEngine::kParamVinylWear,0.35);
    addPoint(c,MixEngine::kParamVinylNoise,0.0);
    addPoint(c,MixEngine::kParamDepth,0.5);
    addPoint(c,MixEngine::kParamWidth,0.5);
    addPoint(c,MixEngine::kParamLowMono,0.25);
    addPoint(c,MixEngine::kParamQuality,1.0);
}

bool clean(double v){
    return std::isfinite(v) && std::fpclassify(v)!=FP_SUBNORMAL;
}
bool clean(float v){
    return std::isfinite(v) && std::fpclassify(v)!=FP_SUBNORMAL;
}

template<class T>
bool runStandard(){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime;
    setup.symbolicSampleSize=std::is_same_v<T,float>?kSample32:kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 20;

    ParameterChanges changes{64},outChanges{16}; configure(changes);
    std::array<T,kBlock> in{},out{};
    const T tiny=std::is_same_v<T,float>
        ? static_cast<T>(std::numeric_limits<float>::denorm_min()*32.0f)
        : static_cast<T>(std::numeric_limits<double>::denorm_min()*1024.0);
    for(int i=0;i<kBlock;++i)in[i]=(i&1)?tiny:-tiny;

    T* inP[1]{in.data()}; T* outP[1]{out.data()};
    AudioBusBuffers ib{},ob{}; ib.numChannels=1; ob.numChannels=1;
    if constexpr(std::is_same_v<T,float>){ib.channelBuffers32=inP;ob.channelBuffers32=outP;}
    else {ib.channelBuffers64=inP;ob.channelBuffers64=outP;}
    ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=setup.symbolicSampleSize;
    d.numSamples=kBlock; d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;
    d.inputParameterChanges=&changes; d.outputParameterChanges=&outChanges;

    const unsigned int original=_mm_getcsr() & ~0x8040u;
    _mm_setcsr(original);
    if(p->process(d)!=kResultOk)throw 21;
    const unsigned int restored=_mm_getcsr();
    bool ok=restored==original;
    for(const auto v:out)ok=clean(v)&&ok;
    _mm_setcsr(original|0x8040u);
    return ok;
}

bool runMixFx(){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk||p->setProcessing(true)!=kResultOk)throw 30;
    SpeakerArrangement arr=SpeakerArr::kMono;
    if(p->setMixChannelArrangements(&arr,1)!=kResultOk)throw 31;

    ParameterChanges changes{64},controlOut{16}; configure(changes);
    ProcessData control{}; control.processMode=kRealtime; control.symbolicSampleSize=kSample64;
    control.numSamples=kBlock; control.inputParameterChanges=&changes; control.outputParameterChanges=&controlOut;
    if(p->processMixControl(&control)!=kResultOk)throw 32;

    std::array<double,kBlock> in{},out{};
    const double tiny=std::numeric_limits<double>::denorm_min()*1024.0;
    for(int i=0;i<kBlock;++i)in[i]=(i&1)?tiny:-tiny;
    double* inP[1]{in.data()}; double* outP[1]{out.data()};
    AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
    ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=kBlock;
    d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;

    const unsigned int original=_mm_getcsr() & ~0x8040u;
    _mm_setcsr(original);
    if(p->processMixChannel(0,&d)!=kResultOk)throw 33;
    const unsigned int restored=_mm_getcsr();
    bool ok=restored==original;
    for(const auto v:out)ok=clean(v)&&ok;
    _mm_setcsr(original|0x8040u);
    return ok;
}
}

int main(){
    try{
        const unsigned int hostBefore=_mm_getcsr();
        const bool f32=runStandard<float>();
        const bool f64=runStandard<double>();
        const bool mix=runMixFx();
        _mm_setcsr(hostBefore);
        std::cout<<"Standard32="<<(f32?"PASS":"FAIL")
                 <<" Standard64="<<(f64?"PASS":"FAIL")
                 <<" MixFX64="<<(mix?"PASS":"FAIL")<<"\n";
        if(!(f32&&f64&&mix)){
            std::cerr<<"FAIL: denormal hardening / MXCSR restoration audit\n";
            return 1;
        }
        std::cout<<"PASS: FTZ/DAZ scoped audio processing, no subnormal output, host MXCSR restored\n";
        return 0;
    }catch(int c){std::cerr<<"Denormal test setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Denormal test unknown exception\n";return 90;}
}
