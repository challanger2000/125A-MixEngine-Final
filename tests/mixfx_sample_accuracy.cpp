#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kSr=48000.0;
constexpr int kTotal=8192;
constexpr int kEvent=2176;
constexpr double kPi=3.14159265358979323846;
using Setting=std::pair<ParamID,double>;

struct Case {
    const char* name;
    ParamID eventId;
    double before;
    double after;
    std::vector<Setting> extra;
};

void addPoint(ParameterChanges& changes,ParamID id,int32 offset,double value){
    int32 qi=0; auto* q=changes.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(offset,std::clamp(value,0.0,1.0),pi)!=kResultTrue)throw 11;
}

void addBaseConfig(ParameterChanges& c,const Case& tc){
    const std::vector<Setting> base={
        {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},
        {MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,0.0},
        {MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamConsoleMode,1.0/3.0},{MixEngine::kParamConsoleDrive,0.25},
        {MixEngine::kParamConsoleNoise,0.0},
        {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTubeAmount,0.20},{MixEngine::kParamTubeType,0.5},
        {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamTapeAmount,0.20},{MixEngine::kParamTapeSpeed,0.5},
        {MixEngine::kParamTapeStability,0.9},{MixEngine::kParamTapeHiss,0.0},
        {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamGlueAmount,0.15},{MixEngine::kParamGlueCharacter,0.5},
        {MixEngine::kParamVinylOn,0.0},{MixEngine::kParamVinylCharacter,0.25},{MixEngine::kParamVinylWear,0.0},
        {MixEngine::kParamVinylNoise,0.0},
        {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
        {MixEngine::kParamQuality,0.0}
    };
    for(const auto& [id,v]:base)addPoint(c,id,0,v);
    for(const auto& [id,v]:tc.extra)addPoint(c,id,0,v);
    addPoint(c,tc.eventId,0,tc.before);
}

std::vector<double> render(const Case& tc,const std::vector<int>& pattern){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=256; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;
    SpeakerArrangement arr=SpeakerArr::kMono;
    if(p->setMixChannelArrangements(&arr,1)!=kResultOk)throw 22;

    std::vector<double> input(kTotal),output(kTotal,0.0);
    for(int n=0;n<kTotal;++n){
        const double t=double(n)/kSr;
        input[n]=0.17*std::sin(2.0*kPi*997.0*t)
                +0.09*std::sin(2.0*kPi*5111.0*t+0.2)
                +0.04*std::sin(2.0*kPi*89.0*t+0.4);
    }

    int base=0,bi=0; bool first=true;
    while(base<kTotal){
        const int requested=pattern[std::size_t(bi++%pattern.size())];
        const int count=std::min(requested,kTotal-base);
        std::vector<double> in(count),out(count);
        std::copy_n(input.data()+base,count,in.data());

        ParameterChanges changes{64},controlOut{8};
        if(first)addBaseConfig(changes,tc);
        if(kEvent>=base && kEvent<base+count)
            addPoint(changes,tc.eventId,kEvent-base,tc.after);

        ProcessData control{};
        control.processMode=kRealtime; control.symbolicSampleSize=kSample64; control.numSamples=count;
        control.inputParameterChanges=&changes; control.outputParameterChanges=&controlOut;
        if(p->processMixControl(&control)!=kResultOk)throw 23;

        double* inP[1]{in.data()}; double* outP[1]{out.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
        ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
        d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;
        if(p->processMixChannel(0,&d)!=kResultOk)throw 24;

        std::copy(out.begin(),out.end(),output.begin()+base);
        first=false; base+=count;
    }
    return output;
}

bool check(const Case& tc){
    const auto whole=render(tc,{256});
    const auto split=render(tc,{128});
    double maxDiff=0.0,rms=0.0;
    for(int i=0;i<kTotal;++i){
        const double d=whole[i]-split[i];
        maxDiff=std::max(maxDiff,std::abs(d));
        rms+=d*d;
    }
    rms=std::sqrt(rms/double(kTotal));
    std::cout<<tc.name<<" maxDiff="<<maxDiff<<" rmsDiff="<<rms<<"\n";
    return maxDiff<=1.0e-10;
}
}

int main(){
    try{
        const std::vector<Case> cases={
            {"Input",MixEngine::kParamInput,0.5,0.75,{}},
            {"Output",MixEngine::kParamOutput,0.5,0.75,{}},
            {"Calibration",MixEngine::kParamCalibration,0.0,1.0,
                {{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.65}}},
            {"Auto Gain",MixEngine::kParamAutoGain,0.0,1.0,
                {{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.70}}},
            {"Console Mode",MixEngine::kParamConsoleMode,0.0,1.0,
                {{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.70}}},
            {"Console Drive",MixEngine::kParamConsoleDrive,0.10,0.85,
                {{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,1.0/3.0}}},
            {"Console Noise",MixEngine::kParamConsoleNoise,0.0,0.80,
                {{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.50}}},
            {"Tube Amount",MixEngine::kParamTubeAmount,0.10,0.85,
                {{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeType,0.5}}},
            {"Tube Voice",MixEngine::kParamTubeType,0.0,1.0,
                {{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.70}}},
            {"Tape Amount",MixEngine::kParamTapeAmount,0.10,0.85,
                {{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.9}}},
            {"Tape Speed",MixEngine::kParamTapeSpeed,0.0,1.0,
                {{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},{MixEngine::kParamTapeStability,0.9}}},
            {"Tape Stability",MixEngine::kParamTapeStability,1.0,0.0,
                {{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},{MixEngine::kParamTapeSpeed,0.5}}},
            {"Tape Hiss",MixEngine::kParamTapeHiss,0.0,0.80,
                {{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.55},{MixEngine::kParamTapeSpeed,0.5}}},
            {"Glue Amount",MixEngine::kParamGlueAmount,0.10,0.80,
                {{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueCharacter,0.5}}},
            {"Glue Response",MixEngine::kParamGlueCharacter,0.0,1.0,
                {{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.60}}},
            {"Vinyl Color",MixEngine::kParamVinylCharacter,0.05,0.85,
                {{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylWear,0.20}}},
            {"Vinyl Wear",MixEngine::kParamVinylWear,0.0,0.80,
                {{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.50}}},
            {"Vinyl Noise",MixEngine::kParamVinylNoise,0.0,0.80,
                {{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.35},{MixEngine::kParamVinylWear,0.35}}},
            {"Depth",MixEngine::kParamDepth,0.25,0.85,{}},
            {"Width",MixEngine::kParamWidth,0.35,0.90,{}},
            {"Low Mono",MixEngine::kParamLowMono,0.0,0.85,{}},
            {"Quality Console",MixEngine::kParamQuality,0.0,1.0,
                {{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.65}}},
            {"Quality Tube",MixEngine::kParamQuality,0.0,1.0,
                {{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.65}}},
            {"Quality Tape",MixEngine::kParamQuality,0.0,1.0,
                {{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.65}}},
            {"Quality Vinyl",MixEngine::kParamQuality,0.0,1.0,
                {{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.65}}},
            {"Bypass",MixEngine::kParamBypass,0.0,1.0,
                {{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.65}}}
        };
        bool ok=true;
        for(const auto& tc:cases)ok=check(tc)&&ok;
        if(!ok){
            std::cerr<<"FAILED: at least one MixFX automation parameter depends on host block partitioning\n";
            return 1;
        }
        std::cout<<"PASSED: expanded MixFX sample-accurate automation matrix\n";
        return 0;
    }catch(int c){std::cerr<<"MixFX sample-accuracy setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"MixFX sample-accuracy unknown exception\n";return 90;}
}
