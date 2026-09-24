#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

void setParam(ParameterChanges& changes, ParamID id, double value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    if (!queue) throw 10;
    int32 pointIndex = 0;
    if (queue->addPoint(0, value, pointIndex) != kResultTrue) throw 11;
}

template <typename Sample>
int runBypassImpulse(bool mixFx, int32 sampleSize, int channels, int blockSize) {
    constexpr int total = 160;
    auto processor = std::make_unique<MixEngine::Processor>();

    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = sampleSize;
    setup.maxSamplesPerBlock = blockSize;
    setup.sampleRate = 48000.0;
    if (processor->setupProcessing(setup) != kResultOk) throw 20;
    if (processor->setProcessing(true) != kResultOk) throw 21;
    if (processor->getLatencySamples() != MixEngine::v3ReportedLatencySamples(48000.0)) throw 22;
    if (processor->canProcessSampleSize(sampleSize) != kResultTrue) throw 23;

    if (mixFx) {
        SpeakerArrangement arrangement = channels == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo;
        if (processor->setMixChannelArrangements(&arrangement,1) != kResultOk) throw 24;
    }

    ParameterChanges changes{32};
    ParameterChanges outputChanges{16};
    setParam(changes, MixEngine::kParamBypass, 1.0);

    if (mixFx) {
        ProcessData control{};
        control.processMode = kRealtime;
        control.symbolicSampleSize = sampleSize;
        control.numSamples = blockSize;
        control.inputParameterChanges = &changes;
        control.outputParameterChanges = &outputChanges;
        if (processor->processMixControl(&control) != kResultOk) throw 25;
        changes.clearQueue();
    }

    std::vector<Sample> rendered(static_cast<std::size_t>(total), Sample{});
    bool first = true;

    for (int base=0; base<total; base+=blockSize) {
        const int count = std::min(blockSize,total-base);
        std::vector<Sample> inL(static_cast<std::size_t>(count), Sample{});
        std::vector<Sample> inR(static_cast<std::size_t>(count), Sample{});
        std::vector<Sample> outL(static_cast<std::size_t>(count), Sample{});
        std::vector<Sample> outR(static_cast<std::size_t>(count), Sample{});
        if (base == 0) {
            inL[0] = static_cast<Sample>(1.0);
            if (channels == 2) inR[0] = static_cast<Sample>(0.5);
        }

        AudioBusBuffers inBus{},outBus{};
        inBus.numChannels = channels;
        outBus.numChannels = channels;

        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = sampleSize;
        data.numSamples = count;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;
        data.outputParameterChanges = &outputChanges;

        if constexpr (std::is_same_v<Sample,float>) {
            float* inPtrs[2]{inL.data(),channels==2?inR.data():nullptr};
            float* outPtrs[2]{outL.data(),channels==2?outR.data():nullptr};
            inBus.channelBuffers32 = inPtrs;
            outBus.channelBuffers32 = outPtrs;
            if (!mixFx && first) data.inputParameterChanges = &changes;
            const auto result = mixFx ? processor->processMixChannel(0,&data) : processor->process(data);
            if (result != kResultOk) throw 26;
        } else {
            double* inPtrs[2]{inL.data(),channels==2?inR.data():nullptr};
            double* outPtrs[2]{outL.data(),channels==2?outR.data():nullptr};
            inBus.channelBuffers64 = inPtrs;
            outBus.channelBuffers64 = outPtrs;
            if (!mixFx && first) data.inputParameterChanges = &changes;
            const auto result = mixFx ? processor->processMixChannel(0,&data) : processor->process(data);
            if (result != kResultOk) throw 27;
        }

        first = false;
        changes.clearQueue();
        for (int i=0;i<count;++i)
            rendered[static_cast<std::size_t>(base+i)] = outL[static_cast<std::size_t>(i)];
    }

    int peakIndex=-1;
    double peak=0.0;
    for (int i=0;i<total;++i) {
        const double a=std::abs(static_cast<double>(rendered[static_cast<std::size_t>(i)]));
        if (a>peak) { peak=a; peakIndex=i; }
    }
    if (std::abs(peak-1.0)>1.0e-6) throw 28;
    return peakIndex;
}

template <typename Sample>
std::vector<double> renderActive(bool mixFx,int32 sampleSize,int channels,int blockSize) {
    constexpr int total=4096;
    constexpr double sr=48000.0;
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=sampleSize;
    setup.maxSamplesPerBlock=blockSize;
    setup.sampleRate=sr;
    if(processor->setupProcessing(setup)!=kResultOk) throw 40;
    if(processor->setProcessing(true)!=kResultOk) throw 41;

    if(mixFx){
        SpeakerArrangement arrangement=channels==1?SpeakerArr::kMono:SpeakerArr::kStereo;
        if(processor->setMixChannelArrangements(&arrangement,1)!=kResultOk) throw 42;
    }

    ParameterChanges changes{64};
    ParameterChanges outputChanges{16};
    setParam(changes,MixEngine::kParamConsoleOn,1.0);
    setParam(changes,MixEngine::kParamConsoleDrive,0.55);
    setParam(changes,MixEngine::kParamConsoleCrosstalk,1.0); // retired slot must be inert
    setParam(changes,MixEngine::kParamConsoleNoise,0.0);
    setParam(changes,MixEngine::kParamTubeOn,1.0);
    setParam(changes,MixEngine::kParamTubeAmount,0.42);
    setParam(changes,MixEngine::kParamTapeOn,1.0);
    setParam(changes,MixEngine::kParamTapeAmount,0.36);
    setParam(changes,MixEngine::kParamTapeStability,1.0);
    setParam(changes,MixEngine::kParamTapeHiss,0.0);
    setParam(changes,MixEngine::kParamGlueOn,1.0);
    setParam(changes,MixEngine::kParamGlueAmount,0.30);
    setParam(changes,MixEngine::kParamVinylOn,1.0);
    setParam(changes,MixEngine::kParamVinylCharacter,0.25);
    setParam(changes,MixEngine::kParamVinylWear,0.0);
    setParam(changes,MixEngine::kParamVinylNoise,0.0);
    setParam(changes,MixEngine::kParamDepth,0.72);
    setParam(changes,MixEngine::kParamWidth,0.64);
    setParam(changes,MixEngine::kParamLowMono,0.35);
    setParam(changes,MixEngine::kParamQuality,1.0);

    if(mixFx){
        ProcessData control{};
        control.processMode=kRealtime;
        control.symbolicSampleSize=sampleSize;
        control.numSamples=blockSize;
        control.inputParameterChanges=&changes;
        control.outputParameterChanges=&outputChanges;
        if(processor->processMixControl(&control)!=kResultOk) throw 43;
        changes.clearQueue();
    }

    std::vector<double> rendered(static_cast<std::size_t>(total));
    bool first=true;
    for(int base=0;base<total;base+=blockSize){
        const int count=std::min(blockSize,total-base);
        std::vector<Sample> inL(static_cast<std::size_t>(count));
        std::vector<Sample> inR(static_cast<std::size_t>(count));
        std::vector<Sample> outL(static_cast<std::size_t>(count));
        std::vector<Sample> outR(static_cast<std::size_t>(count));
        for(int i=0;i<count;++i){
            const int n=base+i;
            const double x=0.19*std::sin(2.0*3.14159265358979323846*997.0*n/sr)
                         +0.07*std::sin(2.0*3.14159265358979323846*6113.0*n/sr);
            inL[static_cast<std::size_t>(i)]=static_cast<Sample>(x);
            inR[static_cast<std::size_t>(i)]=static_cast<Sample>(channels==2?0.73*x:x);
        }

        AudioBusBuffers inBus{},outBus{};
        inBus.numChannels=channels;outBus.numChannels=channels;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=sampleSize;data.numSamples=count;
        data.numInputs=1;data.numOutputs=1;data.inputs=&inBus;data.outputs=&outBus;
        data.outputParameterChanges=&outputChanges;

        if constexpr(std::is_same_v<Sample,float>){
            float* inPtrs[2]{inL.data(),channels==2?inR.data():nullptr};
            float* outPtrs[2]{outL.data(),channels==2?outR.data():nullptr};
            inBus.channelBuffers32=inPtrs;outBus.channelBuffers32=outPtrs;
            if(!mixFx&&first)data.inputParameterChanges=&changes;
            const auto result=mixFx?processor->processMixChannel(0,&data):processor->process(data);
            if(result!=kResultOk)throw 44;
        }else{
            double* inPtrs[2]{inL.data(),channels==2?inR.data():nullptr};
            double* outPtrs[2]{outL.data(),channels==2?outR.data():nullptr};
            inBus.channelBuffers64=inPtrs;outBus.channelBuffers64=outPtrs;
            if(!mixFx&&first)data.inputParameterChanges=&changes;
            const auto result=mixFx?processor->processMixChannel(0,&data):processor->process(data);
            if(result!=kResultOk)throw 45;
        }
        first=false;
        changes.clearQueue();
        for(int i=0;i<count;++i){
            const double y=static_cast<double>(outL[static_cast<std::size_t>(i)]);
            if(!std::isfinite(y))throw 46;
            rendered[static_cast<std::size_t>(base+i)]=y;
        }
    }
    return rendered;
}

bool activeParity(bool mixFx,int channels,int blockSize){
    const auto f=renderActive<float>(mixFx,kSample32,channels,blockSize);
    const auto d=renderActive<double>(mixFx,kSample64,channels,blockSize);
    double maxError=0.0;
    for(std::size_t i=128;i<f.size();++i)
        maxError=std::max(maxError,std::abs(f[i]-d[i]));
    std::cout<<(mixFx?"MixFX":"Channel")<<" active parity ch="<<channels
             <<" block="<<blockSize<<" max32vs64="<<maxError<<"\n";
    return maxError<2.0e-5;
}

} // namespace

int main(){
    try{
        bool ok=true;
        const std::array<int,9> blocks{{1,7,31,64,127,256,511,1024,2048}};
        for(bool mixFx:{false,true}){
            for(int channels:{1,2}){
                for(int block:blocks){
                    const int p32=runBypassImpulse<float>(mixFx,kSample32,channels,block);
                    const int p64=runBypassImpulse<double>(mixFx,kSample64,channels,block);
                    std::cout<<(mixFx?"MixFX":"Channel")<<" bypass ch="<<channels
                             <<" block="<<block<<" peak32="<<p32<<" peak64="<<p64<<"\n";
                    ok=ok && p32==MixEngine::v3ReportedLatencySamples(48000.0)
                          && p64==MixEngine::v3ReportedLatencySamples(48000.0);
                }
            }
        }

        for(bool mixFx:{false,true}){
            ok=activeParity(mixFx,1,31)&&ok;
            ok=activeParity(mixFx,2,127)&&ok;
            ok=activeParity(mixFx,2,511)&&ok;
            ok=activeParity(mixFx,2,1024)&&ok;
            ok=activeParity(mixFx,2,2048)&&ok;
        }

        if(!ok){
            std::cerr<<"Host matrix diagnostic FAILED\n";
            return 1;
        }
        std::cout<<"Host matrix diagnostic PASSED\n";
        return 0;
    }catch(int code){
        std::cerr<<"Host matrix setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Host matrix unknown exception\n";
        return 90;
    }
}
