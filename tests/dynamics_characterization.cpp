#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kPi=3.14159265358979323846;
constexpr double kFs=48000.0;
constexpr int32 kBlock=128;

using Param=std::pair<ParamID,double>;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 q=0; auto* queue=changes.addParameterData(id,q); if(!queue) throw 10;
    int32 p=0; if(queue->addPoint(0,std::clamp(value,0.0,1.0),p)!=kResultTrue) throw 11;
}

struct RenderHarness {
    std::unique_ptr<MixEngine::Processor> processor;
    ParameterChanges changes{64};
    bool first=true;
    std::array<float,kBlock> in{},out{};
    float* inPtr[1]{in.data()};
    float* outPtr[1]{out.data()};
    AudioBusBuffers inBus{},outBus{};

    explicit RenderHarness(const std::vector<Param>& params)
        : processor(std::make_unique<MixEngine::Processor>()) {
        ProcessSetup setup{};
        setup.processMode=kRealtime;
        setup.symbolicSampleSize=kSample32;
        setup.maxSamplesPerBlock=kBlock;
        setup.sampleRate=kFs;
        if(processor->setupProcessing(setup)!=kResultOk) throw 20;
        if(processor->setProcessing(true)!=kResultOk) throw 21;

        inBus.numChannels=1; inBus.channelBuffers32=inPtr;
        outBus.numChannels=1; outBus.channelBuffers32=outPtr;

        setParam(changes,MixEngine::kParamBypass,0.0);
        setParam(changes,MixEngine::kParamInput,0.5);
        setParam(changes,MixEngine::kParamOutput,0.5);
        setParam(changes,MixEngine::kParamCalibration,0.0);
        setParam(changes,MixEngine::kParamAutoGain,0.0);
        setParam(changes,MixEngine::kParamConsoleOn,0.0);
        setParam(changes,MixEngine::kParamTubeOn,0.0);
        setParam(changes,MixEngine::kParamTapeOn,0.0);
        setParam(changes,MixEngine::kParamGlueOn,0.0);
        setParam(changes,MixEngine::kParamVinylOn,0.0);
        setParam(changes,MixEngine::kParamConsoleNoise,0.0);
        setParam(changes,MixEngine::kParamTapeHiss,0.0);
        setParam(changes,MixEngine::kParamVinylNoise,0.0);
        setParam(changes,MixEngine::kParamWidth,0.5);
        setParam(changes,MixEngine::kParamDepth,0.5);
        setParam(changes,MixEngine::kParamLowMono,0.0);
        setParam(changes,MixEngine::kParamQuality,1.0);
        for(const auto&p:params) setParam(changes,p.first,p.second);
    }

    std::vector<float> process(const std::vector<float>& src){
        std::vector<float> dst(src.size(),0.0f);
        for(size_t base=0;base<src.size();base+=kBlock){
            const int count=static_cast<int>(std::min<size_t>(kBlock,src.size()-base));
            std::fill(in.begin(),in.end(),0.0f);
            std::fill(out.begin(),out.end(),0.0f);
            for(int i=0;i<count;++i) in[static_cast<size_t>(i)]=src[base+i];

            ProcessData data{};
            data.processMode=kRealtime;
            data.symbolicSampleSize=kSample32;
            data.numSamples=count;
            data.numInputs=1; data.numOutputs=1;
            data.inputs=&inBus; data.outputs=&outBus;
            data.inputParameterChanges=first?&changes:nullptr;
            if(processor->process(data)!=kResultOk) throw 22;
            first=false;
            for(int i=0;i<count;++i) dst[base+i]=out[static_cast<size_t>(i)];
        }
        return dst;
    }
};

std::vector<float> makeBurstProgram(){
    const int pre=24000;
    const int hot=12000;
    const int quiet=36000;
    std::vector<float> x(pre+hot+quiet,0.0f);
    for(int n=0;n<pre;++n)
        x[n]=static_cast<float>(0.0630957344*std::sin(2.0*kPi*997.0*n/kFs)); // -24 dBFS
    for(int n=0;n<hot;++n)
        x[pre+n]=static_cast<float>(0.6309573445*std::sin(2.0*kPi*997.0*(pre+n)/kFs)); // -4 dBFS
    for(int n=0;n<quiet;++n)
        x[pre+hot+n]=static_cast<float>(0.0630957344*std::sin(2.0*kPi*997.0*(pre+hot+n)/kFs));
    return x;
}

double rmsRange(const std::vector<float>& x,int start,int length){
    long double e=0.0;
    for(int i=0;i<length;++i){
        const double v=x[static_cast<size_t>(start+i)];
        e+=v*v;
    }
    return std::sqrt(static_cast<double>(e/static_cast<long double>(length)));
}

double dbRatio(double a,double b){
    return 20.0*std::log10(std::max(a,1.0e-15)/std::max(b,1.0e-15));
}

double rmsDiff(const std::vector<float>& a,const std::vector<float>& b,int start,int length){
    long double e=0.0;
    for(int i=0;i<length;++i){
        const double d=static_cast<double>(a[static_cast<size_t>(start+i)])-
                       static_cast<double>(b[static_cast<size_t>(start+i)]);
        e+=d*d;
    }
    return std::sqrt(static_cast<double>(e/static_cast<long double>(length)));
}

}

int main(){
    try{
        std::cout<<std::fixed<<std::setprecision(6);
        bool ok=true;
        const auto program=makeBurstProgram();
        const int pre=24000,hot=12000,quietStart=pre+hot;
        const int early=2400;   // first 50 ms after hot burst
        const int late=12000;   // final 250 ms after recovery
        const int lateStart=static_cast<int>(program.size())-late;

        // Tube memory: after a hot burst, the same quiet tone should initially
        // be processed differently, then recover toward its pre-burst state.
        for(int type=0;type<3;++type){
            RenderHarness h({
                {MixEngine::kParamTubeOn,1.0},
                {MixEngine::kParamTubeType,static_cast<double>(type)/2.0},
                {MixEngine::kParamTubeAmount,0.65}
            });
            const auto y=h.process(program);
            const double preR=rmsRange(y,pre-12000,12000);
            const double earlyR=rmsRange(y,quietStart,early);
            const double lateR=rmsRange(y,lateStart,late);
            const double memoryDb=dbRatio(earlyR,preR);
            const double recoveryDb=dbRatio(lateR,preR);
            std::cout<<"TubeMemory type="<<type
                     <<" early-vs-pre="<<memoryDb<<" dB"
                     <<" late-vs-pre="<<recoveryDb<<" dB\n";
            if(!std::isfinite(memoryDb)||!std::isfinite(recoveryDb)) ok=false;
            if(std::abs(memoryDb)<0.01) ok=false;
            if(std::abs(recoveryDb)>std::abs(memoryDb)+0.02) ok=false;
        }

        // Glue response: RESPONSE must materially change attack/recovery.
        std::array<double,3> attackDb{},releaseDb{};
        for(int ri=0;ri<3;++ri){
            const double response=0.5*ri;
            RenderHarness h({
                {MixEngine::kParamGlueOn,1.0},
                {MixEngine::kParamGlueAmount,0.65},
                {MixEngine::kParamGlueCharacter,response}
            });
            const auto y=h.process(program);
            const double hotEarly=rmsRange(y,pre,2400);
            const double hotLate=rmsRange(y,pre+hot-2400,2400);
            const double quietEarly=rmsRange(y,quietStart,2400);
            attackDb[ri]=dbRatio(hotEarly,hotLate);
            releaseDb[ri]=dbRatio(quietEarly,rmsRange(y,lateStart,late));
            std::cout<<"GlueDynamics response="<<response
                     <<" attackDelta="<<attackDb[ri]<<" dB"
                     <<" releaseDelta="<<releaseDb[ri]<<" dB\n";
            if(!std::isfinite(attackDb[ri])||!std::isfinite(releaseDb[ri])) ok=false;
        }
        if(std::max({attackDb[0],attackDb[1],attackDb[2]})-
           std::min({attackDb[0],attackDb[1],attackDb[2]})<0.03) ok=false;
        if(std::max({releaseDb[0],releaseDb[1],releaseDb[2]})-
           std::min({releaseDb[0],releaseDb[1],releaseDb[2]})<0.03) ok=false;

        // Tape Stability: at Stability=100% transport is deterministic/stable.
        // Reducing it must create an actual time-varying waveform deviation.
        std::vector<float> steady(48000);
        for(size_t n=0;n<steady.size();++n)
            steady[n]=static_cast<float>(0.1778279410*std::sin(2.0*kPi*3000.0*n/kFs)); // -15 dBFS

        RenderHarness stable({
            {MixEngine::kParamTapeOn,1.0},
            {MixEngine::kParamTapeSpeed,0.5},
            {MixEngine::kParamTapeAmount,0.45},
            {MixEngine::kParamTapeStability,1.0}
        });
        RenderHarness unstable({
            {MixEngine::kParamTapeOn,1.0},
            {MixEngine::kParamTapeSpeed,0.5},
            {MixEngine::kParamTapeAmount,0.45},
            {MixEngine::kParamTapeStability,0.0}
        });
        const auto ys=stable.process(steady);
        const auto yu=unstable.process(steady);
        const double diff=rmsDiff(ys,yu,12000,30000);
        const double ref=rmsRange(ys,12000,30000);
        const double diffDb=dbRatio(diff,ref);
        std::cout<<"TapeStability unstable-vs-stable residual="<<diffDb<<" dBFS-relative\n";
        if(!std::isfinite(diffDb)||diffDb<-80.0||diffDb>-6.0) ok=false;

        if(!ok){
            std::cerr<<"FAILED: V2 dynamics/memory characterization contract\n";
            return 1;
        }
        std::cout<<"PASSED: V2 dynamics/memory characterization contract\n";
        return 0;
    }catch(int code){
        std::cerr<<"Dynamics characterization setup failed: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Dynamics characterization unknown failure\n";
        return 90;
    }
}
