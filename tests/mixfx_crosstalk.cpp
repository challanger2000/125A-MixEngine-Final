#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kChannels=3;
constexpr int kTotal=8192;
constexpr int kWarmup=1024;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 queueIndex=0;
    auto* q=changes.addParameterData(id,queueIndex);
    if(!q)throw 10;
    int32 pointIndex=0;
    if(q->addPoint(0,value,pointIndex)!=kResultTrue)throw 11;
}

struct Render {
    std::array<std::vector<double>,kChannels> left;
    std::array<std::vector<double>,kChannels> right;
};

template<typename Sample>
Render render(bool crosstalk,const std::array<int,kChannels>& order,int blockSize){
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=std::is_same_v<Sample,float>?kSample32:kSample64;
    setup.maxSamplesPerBlock=blockSize;
    setup.sampleRate=kSr;
    if(processor->setupProcessing(setup)!=kResultOk)throw 20;
    if(processor->setProcessing(true)!=kResultOk)throw 21;

    SpeakerArrangement arrangements[kChannels]{SpeakerArr::kStereo,SpeakerArr::kStereo,SpeakerArr::kStereo};
    if(processor->setMixChannelArrangements(arrangements,kChannels)!=kResultOk)throw 22;

    ParameterChanges changes{64};
    ParameterChanges outputChanges{16};
    setParam(changes,MixEngine::kParamBypass,0.0);
    setParam(changes,MixEngine::kParamInput,0.5);
    setParam(changes,MixEngine::kParamCalibration,0.5);
    setParam(changes,MixEngine::kParamAutoGain,0.0);
    setParam(changes,MixEngine::kParamConsoleOn,1.0);
    setParam(changes,MixEngine::kParamConsoleMode,0.0);
    setParam(changes,MixEngine::kParamConsoleDrive,0.0);
    setParam(changes,MixEngine::kParamConsoleCrosstalk,crosstalk?1.0:0.0);
    setParam(changes,MixEngine::kParamConsoleNoise,0.0);
    setParam(changes,MixEngine::kParamTubeOn,0.0);
    setParam(changes,MixEngine::kParamTapeOn,0.0);
    setParam(changes,MixEngine::kParamGlueOn,0.0);
    setParam(changes,MixEngine::kParamVinylOn,0.0);
    setParam(changes,MixEngine::kParamDepth,0.5);
    setParam(changes,MixEngine::kParamWidth,0.5);
    setParam(changes,MixEngine::kParamLowMono,0.0);
    setParam(changes,MixEngine::kParamQuality,0.0);
    setParam(changes,MixEngine::kParamOutput,0.5);

    Render out;
    for(auto& v:out.left)v.assign(kTotal,0.0);
    for(auto& v:out.right)v.assign(kTotal,0.0);

    bool first=true;
    for(int base=0;base<kTotal;base+=blockSize){
        const int count=std::min(blockSize,kTotal-base);
        std::array<std::vector<Sample>,kChannels> inL,inR,outL,outR;
        for(int ch=0;ch<kChannels;++ch){
            inL[ch].assign(static_cast<std::size_t>(count),Sample{});
            inR[ch].assign(static_cast<std::size_t>(count),Sample{});
            outL[ch].assign(static_cast<std::size_t>(count),Sample{});
            outR[ch].assign(static_cast<std::size_t>(count),Sample{});
        }

        // Only the centre console channel carries audio, and only on its left
        // lane. Real channel crosstalk should reach adjacent channel LEFT lanes
        // without inventing L/R bleed.
        for(int i=0;i<count;++i){
            const int n=base+i;
            const double x=0.05*std::sin(2.0*kPi*997.0*static_cast<double>(n)/kSr);
            inL[1][static_cast<std::size_t>(i)]=static_cast<Sample>(x);
        }

        std::array<AudioBusBuffers,kChannels> controlIn{},controlOut{};
        std::array<std::array<Sample*,2>,kChannels> inPtrs{},outPtrs{};
        for(int ch=0;ch<kChannels;++ch){
            inPtrs[ch]={inL[ch].data(),inR[ch].data()};
            outPtrs[ch]={outL[ch].data(),outR[ch].data()};
            controlIn[ch].numChannels=2;
            controlOut[ch].numChannels=2;
            if constexpr(std::is_same_v<Sample,float>){
                controlIn[ch].channelBuffers32=inPtrs[ch].data();
                controlOut[ch].channelBuffers32=outPtrs[ch].data();
            }else{
                controlIn[ch].channelBuffers64=inPtrs[ch].data();
                controlOut[ch].channelBuffers64=outPtrs[ch].data();
            }
        }

        ProcessData control{};
        control.processMode=kRealtime;
        control.symbolicSampleSize=setup.symbolicSampleSize;
        control.numSamples=count;
        control.numInputs=kChannels;
        control.numOutputs=kChannels;
        control.inputs=controlIn.data();
        control.outputs=controlOut.data();
        control.inputParameterChanges=first?&changes:nullptr;
        control.outputParameterChanges=&outputChanges;
        if(processor->processMixControl(&control)!=kResultOk)throw 23;
        first=false;

        for(const int ch:order){
            AudioBusBuffers oneIn=controlIn[ch];
            AudioBusBuffers oneOut=controlOut[ch];
            ProcessData channel{};
            channel.processMode=kRealtime;
            channel.symbolicSampleSize=setup.symbolicSampleSize;
            channel.numSamples=count;
            channel.numInputs=1;
            channel.numOutputs=1;
            channel.inputs=&oneIn;
            channel.outputs=&oneOut;
            if(processor->processMixChannel(ch,&channel)!=kResultOk)throw 24;
        }

        for(int ch=0;ch<kChannels;++ch){
            for(int i=0;i<count;++i){
                const double l=static_cast<double>(outL[ch][static_cast<std::size_t>(i)]);
                const double r=static_cast<double>(outR[ch][static_cast<std::size_t>(i)]);
                if(!std::isfinite(l)||!std::isfinite(r))throw 25;
                out.left[ch][static_cast<std::size_t>(base+i)]=l;
                out.right[ch][static_cast<std::size_t>(base+i)]=r;
            }
        }
    }
    return out;
}

double rms(const std::vector<double>& x){
    long double sum=0.0;int count=0;
    for(int i=kWarmup;i<kTotal;++i){
        const double v=x[static_cast<std::size_t>(i)];
        sum+=static_cast<long double>(v)*v;++count;
    }
    return std::sqrt(static_cast<double>(sum/static_cast<long double>(count)));
}

double maxAbs(const std::vector<double>& x){
    double m=0.0;
    for(int i=kWarmup;i<kTotal;++i)m=std::max(m,std::abs(x[static_cast<std::size_t>(i)]));
    return m;
}

double maxDiff(const Render& a,const Render& b){
    double m=0.0;
    for(int ch=0;ch<kChannels;++ch)
        for(int i=kWarmup;i<kTotal;++i){
            m=std::max(m,std::abs(a.left[ch][static_cast<std::size_t>(i)]-b.left[ch][static_cast<std::size_t>(i)]));
            m=std::max(m,std::abs(a.right[ch][static_cast<std::size_t>(i)]-b.right[ch][static_cast<std::size_t>(i)]));
        }
    return m;
}
}

int main(){
    try{
        const std::array<int,kChannels> forward{{0,1,2}};
        const std::array<int,kChannels> reverse{{2,1,0}};
        const std::array<int,kChannels> mixed{{1,0,2}};

        const auto off=render<double>(false,forward,127);
        const auto on=render<double>(true,forward,127);
        const auto onReverse=render<double>(true,reverse,127);
        const auto onMixed=render<double>(true,mixed,127);
        const auto on32=render<float>(true,reverse,31);

        const double source=rms(on.left[1]);
        const double left0=rms(on.left[0]);
        const double left2=rms(on.left[2]);
        const double ratio0=source>0.0?left0/source:0.0;
        const double ratio2=source>0.0?left2/source:0.0;
        const double offLeak=std::max(rms(off.left[0]),rms(off.left[2]));
        const double rightLeak=std::max({maxAbs(on.right[0]),maxAbs(on.right[1]),maxAbs(on.right[2])});
        const double orderDiff=std::max(maxDiff(on,onReverse),maxDiff(on,onMixed));

        // Block-size/sample-format parity is compared by RMS because HIIR/state
        // boundaries are identical in time but float rounding need not be bit-exact.
        const double ratio32=rms(on32.left[0])/std::max(rms(on32.left[1]),1.0e-15);

        std::cout<<"MixFX Crosstalk ratios: left-adjacent="<<ratio0
                 <<" right-adjacent="<<ratio2
                 <<" float32="<<ratio32
                 <<" offLeak="<<offLeak
                 <<" rightLaneLeak="<<rightLeak
                 <<" orderDiff="<<orderDiff<<"\n";

        bool ok=true;
        ok=ok && source>1.0e-4;
        ok=ok && ratio0>0.012 && ratio0<0.024;
        ok=ok && ratio2>0.012 && ratio2<0.024;
        ok=ok && ratio32>0.012 && ratio32<0.024;
        ok=ok && offLeak<1.0e-12;
        ok=ok && rightLeak<1.0e-12;
        ok=ok && orderDiff<1.0e-12;

        if(!ok){
            std::cerr<<"Mix FX real channel Crosstalk diagnostic FAILED\n";
            return 1;
        }
        std::cout<<"Mix FX real channel Crosstalk diagnostic PASSED: adjacent-channel, lane-preserving, callback-order independent\n";
        return 0;
    }catch(int code){
        std::cerr<<"Mix FX Crosstalk diagnostic setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Mix FX Crosstalk diagnostic unknown exception\n";
        return 90;
    }
}
