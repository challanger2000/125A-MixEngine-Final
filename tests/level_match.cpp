#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kBlock=256;
constexpr int kTotal=32768;
constexpr int kWarmup=8192;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 queueIndex=0;
    auto* queue=changes.addParameterData(id,queueIndex);
    if(!queue)throw 10;
    int32 pointIndex=0;
    if(queue->addPoint(0,value,pointIndex)!=kResultTrue)throw 11;
}

std::vector<double> makeSignal(){
    std::vector<double> x(kTotal);
    for(int n=0;n<kTotal;++n){
        const double t=static_cast<double>(n)/kSr;
        x[static_cast<std::size_t>(n)] =
            0.080*std::sin(2.0*kPi*97.0*t) +
            0.055*std::sin(2.0*kPi*997.0*t+0.17) +
            0.030*std::sin(2.0*kPi*4123.0*t+0.31) +
            0.018*std::sin(2.0*kPi*9031.0*t+0.53);
    }
    return x;
}

double rms(const std::vector<double>& x,int begin){
    long double sum=0.0;
    int count=0;
    for(int i=begin;i<static_cast<int>(x.size());++i){
        const double v=x[static_cast<std::size_t>(i)];
        sum+=static_cast<long double>(v)*v;
        ++count;
    }
    return std::sqrt(static_cast<double>(sum/static_cast<long double>(count)));
}

double renderRms(bool mixFx,bool levelMatch,const std::vector<std::pair<ParamID,double>>& cfg){
    const auto input=makeSignal();
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock;
    setup.sampleRate=kSr;
    if(processor->setupProcessing(setup)!=kResultOk)throw 20;
    if(processor->setProcessing(true)!=kResultOk)throw 21;
    if(mixFx){
        SpeakerArrangement arrangement=SpeakerArr::kMono;
        if(processor->setMixChannelArrangements(&arrangement,1)!=kResultOk)throw 22;
    }

    ParameterChanges changes{64};
    ParameterChanges outputChanges{16};

    // Deterministic baseline with all modules off, then enable only the case.
    setParam(changes,MixEngine::kParamInput,0.5);
    setParam(changes,MixEngine::kParamOutput,0.5);
    setParam(changes,MixEngine::kParamCalibration,0.5);
    setParam(changes,MixEngine::kParamAutoGain,levelMatch?1.0:0.0);
    setParam(changes,MixEngine::kParamConsoleOn,0.0);
    setParam(changes,MixEngine::kParamTubeOn,0.0);
    setParam(changes,MixEngine::kParamTapeOn,0.0);
    setParam(changes,MixEngine::kParamGlueOn,0.0);
    setParam(changes,MixEngine::kParamVinylOn,0.0);
    setParam(changes,MixEngine::kParamConsoleNoise,0.0);
    setParam(changes,MixEngine::kParamTapeHiss,0.0);
    setParam(changes,MixEngine::kParamVinylNoise,0.0);
    setParam(changes,MixEngine::kParamDepth,0.5);
    setParam(changes,MixEngine::kParamWidth,0.5);
    setParam(changes,MixEngine::kParamLowMono,0.0);
    setParam(changes,MixEngine::kParamQuality,0.5);
    for(const auto& [id,value]:cfg)setParam(changes,id,value);

    if(mixFx){
        ProcessData control{};
        control.processMode=kRealtime;
        control.symbolicSampleSize=kSample64;
        control.numSamples=kBlock;
        control.inputParameterChanges=&changes;
        control.outputParameterChanges=&outputChanges;
        if(processor->processMixControl(&control)!=kResultOk)throw 23;
        changes.clearQueue();
    }

    std::vector<double> out(kTotal,0.0);
    bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> in{},rendered{};
        for(int i=0;i<count;++i)in[static_cast<std::size_t>(i)]=input[static_cast<std::size_t>(base+i)];
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
        if(result!=kResultOk)throw 24;
        first=false;
        changes.clearQueue();
        for(int i=0;i<count;++i){
            const double y=rendered[static_cast<std::size_t>(i)];
            if(!std::isfinite(y))throw 25;
            out[static_cast<std::size_t>(base+i)]=y;
        }
    }
    return rms(out,kWarmup);
}

double dbRatio(double out,double in){
    return 20.0*std::log10(std::max(out,1.0e-15)/std::max(in,1.0e-15));
}

struct Case{
    const char* name;
    std::vector<std::pair<ParamID,double>> params;
};

}

int main(){
    try{
        const auto input=makeSignal();
        const double inRms=rms(input,kWarmup);
        const std::vector<Case> cases={
            {"Console",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,1.0/3.0},{MixEngine::kParamConsoleDrive,0.70}}},
            {"Tube",{{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeType,0.5},{MixEngine::kParamTubeAmount,0.70}}},
            {"Tape",{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},{MixEngine::kParamTapeStability,1.0}}},
            {"Glue",{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.60},{MixEngine::kParamGlueCharacter,0.5}}},
            {"Vinyl",{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.50},{MixEngine::kParamVinylWear,0.30}}},
            {"Full",{
                {MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,1.0/3.0},{MixEngine::kParamConsoleDrive,0.45},
                {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeType,0.5},{MixEngine::kParamTubeAmount,0.35},
                {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.35},{MixEngine::kParamTapeStability,1.0},
                {MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.30},{MixEngine::kParamGlueCharacter,0.5},
                {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.25},{MixEngine::kParamVinylWear,0.10}
            }}
        };

        bool ok=true;

        // Input trim contract: with every colour module disabled, Level Match
        // must remove the trivial +/-12 dB gain shift while leaving Level Match
        // OFF as a true input trim. This protects the intended "drive harder,
        // do not merely get louder" workflow.
        for(bool mixFx:{false,true}){
            const auto lowCfg=std::vector<std::pair<ParamID,double>>{{MixEngine::kParamInput,0.25}};
            const auto highCfg=std::vector<std::pair<ParamID,double>>{{MixEngine::kParamInput,0.75}};
            const double lowOffDb=dbRatio(renderRms(mixFx,false,lowCfg),inRms);
            const double highOffDb=dbRatio(renderRms(mixFx,false,highCfg),inRms);
            const double lowOnDb=dbRatio(renderRms(mixFx,true,lowCfg),inRms);
            const double highOnDb=dbRatio(renderRms(mixFx,true,highCfg),inRms);
            std::cout<<(mixFx?"MixFX":"Channel")
                     <<" Input trim OFF low/high="<<lowOffDb<<"/"<<highOffDb
                     <<" dB ON low/high="<<lowOnDb<<"/"<<highOnDb<<" dB\n";
            if(std::abs(lowOffDb+6.0)>0.15 || std::abs(highOffDb-6.0)>0.15)ok=false;
            if(std::abs(lowOnDb)>0.15 || std::abs(highOnDb)>0.15)ok=false;
        }

        // Enabled Tube/Tape at Amount=0 must now be subtly coloured,
        // never identical to an OFF module, and must not create a large level jump.
        for(bool mixFx:{false,true}){
            const double tube0Db=dbRatio(renderRms(mixFx,false,{
                {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeType,0.5},{MixEngine::kParamTubeAmount,0.0}
            }),inRms);
            const double tape0Db=dbRatio(renderRms(mixFx,false,{
                {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.0},{MixEngine::kParamTapeStability,1.0}
            }),inRms);
            std::cout<<(mixFx?"MixFX":"Channel")
                     <<" base colour Tube0="<<tube0Db<<" dB Tape0="<<tape0Db<<" dB\n";
            if(std::abs(tube0Db)<1.0e-5 || std::abs(tube0Db)>0.5)ok=false;
            if(std::abs(tape0Db)<1.0e-5 || std::abs(tape0Db)>0.5)ok=false;
        }

        for(bool mixFx:{false,true}){
            for(const auto& c:cases){
                const double off=renderRms(mixFx,false,c.params);
                const double on=renderRms(mixFx,true,c.params);
                const double offDb=dbRatio(off,inRms);
                const double onDb=dbRatio(on,inRms);
                std::cout<<(mixFx?"MixFX":"Channel")<<" "<<c.name
                         <<" level-match OFF="<<offDb<<" dB"
                         <<" ON="<<onDb<<" dB"
                         <<" improvement="<<(std::abs(offDb)-std::abs(onDb))<<" dB\n";

                // V3 Level Match is a slow programme-energy compensation stage.
                // It must remove obvious loudness bias without behaving like a
                // fast compressor. On this mixed-spectrum fixture, each module
                // and the full chain must settle close enough for fair A/B use.
                if(std::abs(onDb)>0.75)ok=false;
            }
        }

        if(!ok){
            std::cerr<<"Level Match objective diagnostic FAILED\n";
            return 1;
        }
        std::cout<<"Level Match objective diagnostic PASSED\n";
        return 0;
    }catch(int code){
        std::cerr<<"Level Match diagnostic setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Level Match diagnostic unknown exception\n";
        return 90;
    }
}
