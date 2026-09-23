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
constexpr double kFs=48000.0;
constexpr int32 kBlock=256;
constexpr int kSamples=96000;
using Param=std::pair<ParamID,double>;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 q=0; auto* queue=changes.addParameterData(id,q); if(!queue) throw 10;
    int32 p=0; if(queue->addPoint(0,std::clamp(value,0.0,1.0),p)!=kResultTrue) throw 11;
}

std::vector<double> renderNoise(const std::vector<Param>& params){
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kFs;
    if(processor->setupProcessing(setup)!=kResultOk) throw 20;
    if(processor->setProcessing(true)!=kResultOk) throw 21;

    ParameterChanges changes{64};
    for(const auto&p:std::vector<Param>{
        {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},
        {MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.0},
        {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,0.0},
        {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},
        {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},
        {MixEngine::kParamConsoleNoise,0.0},{MixEngine::kParamTapeHiss,0.0},
        {MixEngine::kParamVinylNoise,0.0},{MixEngine::kParamWidth,0.5},
        {MixEngine::kParamDepth,0.5},{MixEngine::kParamLowMono,0.0},
        {MixEngine::kParamQuality,1.0}})
        setParam(changes,p.first,p.second);
    for(const auto&p:params) setParam(changes,p.first,p.second);

    std::vector<double> output(kSamples,0.0);
    std::array<double,kBlock> in{},out{};
    double* inPtr[1]{in.data()}; double* outPtr[1]{out.data()};
    AudioBusBuffers inBus{},outBus{};
    inBus.numChannels=1;inBus.channelBuffers64=inPtr;
    outBus.numChannels=1;outBus.channelBuffers64=outPtr;

    bool first=true;
    for(int base=0;base<kSamples;base+=kBlock){
        const int count=std::min<int>(kBlock,kSamples-base);
        std::fill(in.begin(),in.end(),0.0);
        std::fill(out.begin(),out.end(),0.0);
        ProcessData data{};
        data.processMode=kRealtime; data.symbolicSampleSize=kSample64;
        data.numSamples=count; data.numInputs=1;data.numOutputs=1;
        data.inputs=&inBus;data.outputs=&outBus;
        data.inputParameterChanges=first?&changes:nullptr;
        if(processor->process(data)!=kResultOk) throw 22;
        first=false;
        for(int i=0;i<count;++i) output[static_cast<size_t>(base+i)]=out[static_cast<size_t>(i)];
    }
    return output;
}

struct Stats{
    double rms=0.0,mean=0.0,lowRms=0.0,highRms=0.0;
};

Stats analyze(const std::vector<double>& x){
    const int start=12000;
    double lp=0.0;
    const double coeff=1.0-std::exp(-2.0*3.14159265358979323846*1200.0/kFs);
    long double e=0.0,lowE=0.0,highE=0.0,sum=0.0;
    int n=0;
    for(int i=start;i<static_cast<int>(x.size());++i){
        const double v=x[static_cast<size_t>(i)];
        lp+=coeff*(v-lp);
        const double hp=v-lp;
        e+=v*v;lowE+=lp*lp;highE+=hp*hp;sum+=v;++n;
    }
    Stats s;
    s.rms=std::sqrt(static_cast<double>(e/n));
    s.lowRms=std::sqrt(static_cast<double>(lowE/n));
    s.highRms=std::sqrt(static_cast<double>(highE/n));
    s.mean=static_cast<double>(sum/n);
    return s;
}

double db(double v){return 20.0*std::log10(std::max(v,1.0e-15));}

void report(const std::string& name,const Stats&s){
    std::cout<<name
             <<" rms="<<db(s.rms)<<" dBFS"
             <<" mean="<<s.mean
             <<" low="<<db(s.lowRms)<<" dBFS"
             <<" high="<<db(s.highRms)<<" dBFS"
             <<" high-low="<<db(s.highRms/std::max(s.lowRms,1.0e-15))<<" dB\n";
}
}

int main(){
    try{
        std::cout<<std::fixed<<std::setprecision(6);
        bool ok=true;

        const Stats silent=analyze(renderNoise({}));
        report("AllNoiseOff",silent);
        if(silent.rms>1.0e-12) ok=false;

        for(double n:{0.25,0.50,1.0}){
            const auto s=analyze(renderNoise({
                {MixEngine::kParamConsoleOn,1.0},
                {MixEngine::kParamConsoleDrive,0.0},
                {MixEngine::kParamConsoleNoise,n}
            }));
            report("ConsoleNoise "+std::to_string(n),s);
            if(!std::isfinite(s.rms)||s.rms<=0.0||std::abs(s.mean)>0.01) ok=false;
        }

        for(int speed=0;speed<3;++speed){
            const auto s=analyze(renderNoise({
                {MixEngine::kParamTapeOn,1.0},
                {MixEngine::kParamTapeAmount,0.0},
                {MixEngine::kParamTapeSpeed,static_cast<double>(speed)/2.0},
                {MixEngine::kParamTapeHiss,0.5}
            }));
            report("TapeHiss speed="+std::to_string(speed),s);
            if(!std::isfinite(s.rms)||s.rms<=0.0||std::abs(s.mean)>0.01) ok=false;
        }

        for(double wear:{0.0,0.5,1.0}){
            const auto s=analyze(renderNoise({
                {MixEngine::kParamVinylOn,1.0},
                {MixEngine::kParamVinylCharacter,0.0},
                {MixEngine::kParamVinylWear,wear},
                {MixEngine::kParamVinylNoise,0.5}
            }));
            report("VinylSurface wear="+std::to_string(wear),s);
            if(!std::isfinite(s.rms)||s.rms<=0.0||std::abs(s.mean)>0.01) ok=false;
        }

        if(!ok){
            std::cerr<<"FAILED: V2 noise characterization contract\n";
            return 1;
        }
        std::cout<<"PASSED: V2 noise characterization contract\n";
        return 0;
    }catch(int e){
        std::cerr<<"Noise characterization setup failure: "<<e<<"\n";
        return e;
    }catch(...){
        std::cerr<<"Noise characterization unknown failure\n";
        return 90;
    }
}
