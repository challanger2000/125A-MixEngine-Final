#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kBlock=127;
constexpr int kTotal=16384;
constexpr int kWarmup=4096;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 queueIndex=0;
    auto* queue=changes.addParameterData(id,queueIndex);
    if(!queue)throw 10;
    int32 pointIndex=0;
    if(queue->addPoint(0,value,pointIndex)!=kResultTrue)throw 11;
}

std::vector<double> render(bool mixFx,double quality,int mask){
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock;
    setup.sampleRate=kSr;
    if(processor->setupProcessing(setup)!=kResultOk)throw 20;
    if(processor->setProcessing(true)!=kResultOk)throw 21;
    if(processor->getLatencySamples()!=MixEngine::kFixedLatencySamples)throw 22;

    if(mixFx){
        SpeakerArrangement arrangement=SpeakerArr::kMono;
        if(processor->setMixChannelArrangements(&arrangement,1)!=kResultOk)throw 23;
    }

    const bool console=(mask&1)!=0;
    const bool tube=(mask&2)!=0;
    const bool tape=(mask&4)!=0;
    const bool vinyl=(mask&8)!=0;

    ParameterChanges changes{64};
    ParameterChanges outputChanges{16};
    setParam(changes,MixEngine::kParamInput,0.5);
    setParam(changes,MixEngine::kParamOutput,0.5);
    setParam(changes,MixEngine::kParamCalibration,0.5);
    setParam(changes,MixEngine::kParamAutoGain,0.0);
    setParam(changes,MixEngine::kParamQuality,quality);
    setParam(changes,MixEngine::kParamConsoleOn,console?1.0:0.0);
    setParam(changes,MixEngine::kParamConsoleMode,1.0/3.0);
    setParam(changes,MixEngine::kParamConsoleDrive,console?0.12:0.0);
    setParam(changes,MixEngine::kParamConsoleNoise,0.0);
    setParam(changes,MixEngine::kParamTubeOn,tube?1.0:0.0);
    setParam(changes,MixEngine::kParamTubeAmount,tube?0.10:0.0);
    setParam(changes,MixEngine::kParamTapeOn,tape?1.0:0.0);
    setParam(changes,MixEngine::kParamTapeAmount,tape?0.10:0.0);
    setParam(changes,MixEngine::kParamTapeStability,1.0);
    setParam(changes,MixEngine::kParamTapeHiss,0.0);
    setParam(changes,MixEngine::kParamGlueOn,0.0);
    setParam(changes,MixEngine::kParamVinylOn,vinyl?1.0:0.0);
    setParam(changes,MixEngine::kParamVinylCharacter,vinyl?0.10:0.0);
    setParam(changes,MixEngine::kParamVinylWear,0.0);
    setParam(changes,MixEngine::kParamVinylNoise,0.0);
    setParam(changes,MixEngine::kParamWidth,0.5);
    setParam(changes,MixEngine::kParamDepth,0.5);
    setParam(changes,MixEngine::kParamLowMono,0.0);

    if(mixFx){
        ProcessData control{};
        control.processMode=kRealtime;
        control.symbolicSampleSize=kSample64;
        control.numSamples=kBlock;
        control.inputParameterChanges=&changes;
        control.outputParameterChanges=&outputChanges;
        if(processor->processMixControl(&control)!=kResultOk)throw 24;
        changes.clearQueue();
    }

    std::vector<double> out(static_cast<std::size_t>(kTotal),0.0);
    bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> in{},rendered{};
        for(int i=0;i<count;++i){
            const int n=base+i;
            const double t=static_cast<double>(n)/kSr;
            in[static_cast<std::size_t>(i)] =
                0.015*std::sin(2.0*kPi*173.0*t) +
                0.011*std::sin(2.0*kPi*997.0*t+0.23) +
                0.008*std::sin(2.0*kPi*4009.0*t+0.51) +
                0.005*std::sin(2.0*kPi*9011.0*t+0.79);
        }

        double* inPtrs[1]{in.data()};
        double* outPtrs[1]{rendered.data()};
        AudioBusBuffers inBus{},outBus{};
        inBus.numChannels=1;inBus.channelBuffers64=inPtrs;
        outBus.numChannels=1;outBus.channelBuffers64=outPtrs;

        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample64;
        data.numSamples=count;
        data.numInputs=1;data.numOutputs=1;
        data.inputs=&inBus;data.outputs=&outBus;
        data.outputParameterChanges=&outputChanges;
        if(!mixFx&&first)data.inputParameterChanges=&changes;

        const auto result=mixFx?processor->processMixChannel(0,&data):processor->process(data);
        if(result!=kResultOk)throw 25;
        first=false;
        changes.clearQueue();

        for(int i=0;i<count;++i){
            const double y=rendered[static_cast<std::size_t>(i)];
            if(!std::isfinite(y))throw 26;
            out[static_cast<std::size_t>(base+i)]=y;
        }
    }
    return out;
}

double correlationAtLag(const std::vector<double>& reference,
                        const std::vector<double>& candidate,
                        int lag){
    long double xy=0.0,xx=0.0,yy=0.0;
    const int start=kWarmup+std::max(0,-lag);
    const int end=kTotal-std::max(0,lag);
    for(int n=start;n<end;++n){
        const double x=reference[static_cast<std::size_t>(n)];
        const double y=candidate[static_cast<std::size_t>(n+lag)];
        xy+=static_cast<long double>(x)*y;
        xx+=static_cast<long double>(x)*x;
        yy+=static_cast<long double>(y)*y;
    }
    const long double denom=std::sqrt(xx*yy);
    return denom>0.0?static_cast<double>(xy/denom):0.0;
}

struct AlignmentResult {
    int lag=99;
    double correlation=-std::numeric_limits<double>::infinity();
};

AlignmentResult measureAlignment(const char* path,int mask,
                                 const std::vector<double>& reference,
                                 const std::vector<double>& candidate,
                                 const char* quality){
    AlignmentResult result;
    for(int lag=-4;lag<=4;++lag){
        const double c=correlationAtLag(reference,candidate,lag);
        if(c>result.correlation){result.correlation=c;result.lag=lag;}
    }
    std::cout<<path<<" mask="<<mask<<" "<<quality
             <<" vs 1x bestLag="<<result.lag
             <<" corr="<<result.correlation<<"\n";
    return result;
}

bool acceptable(const AlignmentResult& r){
    // HIIR is minimum-phase: active oversampling changes frequency-dependent
    // group delay, and stacked nonlinear/color stages also change spectrum.
    // Broadband correlation can therefore dip slightly while transport latency
    // is still correct. Keep lag strict and allow only a small spectral margin.
    return std::abs(r.lag)<=2 && r.correlation>0.985;
}
}

int main(){
    try{
        bool ok=true;
        for(int mask=0;mask<16;++mask){
            const auto ch1=render(false,0.0,mask);
            const auto ch2=render(false,0.5,mask);
            const auto ch4=render(false,1.0,mask);
            const auto mx1=render(true,0.0,mask);
            const auto mx2=render(true,0.5,mask);
            const auto mx4=render(true,1.0,mask);

            const auto ch2a=measureAlignment("Channel",mask,ch1,ch2,"2x");
            const auto ch4a=measureAlignment("Channel",mask,ch1,ch4,"4x");
            const auto mx2a=measureAlignment("MixFX",mask,mx1,mx2,"2x");
            const auto mx4a=measureAlignment("MixFX",mask,mx1,mx4,"4x");

            ok=acceptable(ch2a)&&acceptable(ch4a)&&acceptable(mx2a)&&acceptable(mx4a)&&ok;
            if(ch2a.lag!=mx2a.lag || ch4a.lag!=mx4a.lag ||
               std::abs(ch2a.correlation-mx2a.correlation)>1.0e-9 ||
               std::abs(ch4a.correlation-mx4a.correlation)>1.0e-9){
                std::cerr<<"Channel/MixFX latency-phase parity mismatch for mask="<<mask<<"\n";
                ok=false;
            }
        }
        if(!ok){
            std::cerr<<"Processor latency alignment matrix FAILED\n";
            return 1;
        }
        std::cout<<"Processor latency/phase matrix PASSED: 64 quality/path/module comparisons with exact Channel/MixFX parity, fixed host latency="
                 <<MixEngine::kFixedLatencySamples
                 <<" samples, residual minimum-phase correlation lag bounded to +/-2 samples\n";
        return 0;
    }catch(int code){
        std::cerr<<"Processor latency matrix setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Processor latency matrix unknown exception\n";
        return 90;
    }
}
