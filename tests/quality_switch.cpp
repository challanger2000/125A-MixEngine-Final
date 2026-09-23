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
constexpr double kSr=48000.0;
constexpr int kTotal=12288;
constexpr int kSwitchSample=4096;
constexpr int kBlock=256;
constexpr double kPi=3.14159265358979323846;
using Setting=std::pair<ParamID,double>;

void addPoint(ParameterChanges& c,ParamID id,int32 offset,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(offset,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> render(bool mixfx,const std::vector<Setting>& extra){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;
    if(mixfx){
        SpeakerArrangement arr=SpeakerArr::kMono;
        if(p->setMixChannelArrangements(&arr,1)!=kResultOk)throw 22;
    }

    std::vector<double> input(kTotal),output(kTotal,0.0);
    for(int n=0;n<kTotal;++n){
        const double t=double(n)/kSr;
        input[n]=0.20*std::sin(2*kPi*997.0*t)
                +0.10*std::sin(2*kPi*5111.0*t+0.17)
                +0.05*std::sin(2*kPi*89.0*t+0.31);
    }

    int base=0; bool first=true;
    while(base<kTotal){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> in{},out{};
        for(int i=0;i<count;++i)in[i]=input[base+i];

        ParameterChanges changes{64},outputChanges{8};
        if(first){
            const std::vector<Setting> cfg={
                {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},
                {MixEngine::kParamCalibration,0.5},{MixEngine::kParamAutoGain,0.0},
                {MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamConsoleNoise,0.0},
                {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},{MixEngine::kParamTapeHiss,0.0},
                {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},{MixEngine::kParamVinylNoise,0.0},
                {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
                {MixEngine::kParamQuality,0.0}
            };
            for(const auto& [id,v]:cfg)addPoint(changes,id,0,v);
            for(const auto& [id,v]:extra)addPoint(changes,id,0,v);
        }
        if(kSwitchSample>=base&&kSwitchSample<base+count)addPoint(changes,MixEngine::kParamQuality,kSwitchSample-base,1.0);

        double* inP[1]{in.data()}; double* outP[1]{out.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;

        if(mixfx){
            ProcessData control{}; control.processMode=kRealtime; control.symbolicSampleSize=kSample64;
            control.numSamples=count; control.inputParameterChanges=&changes; control.outputParameterChanges=&outputChanges;
            if(p->processMixControl(&control)!=kResultOk)throw 23;

            ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
            d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;
            if(p->processMixChannel(0,&d)!=kResultOk)throw 24;
        }else{
            ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
            d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;
            d.inputParameterChanges=&changes; d.outputParameterChanges=&outputChanges;
            if(p->process(d)!=kResultOk)throw 25;
        }

        for(int i=0;i<count;++i){
            if(!std::isfinite(out[i]))throw 26;
            output[base+i]=out[i];
        }
        first=false; base+=count;
    }
    return output;
}

double maxDerivative(const std::vector<double>& y,int start,int end){
    double m=0.0;
    start=std::max(start,1); end=std::min(end,(int)y.size());
    for(int i=start;i<end;++i)m=std::max(m,std::abs(y[i]-y[i-1]));
    return m;
}
}

int main(){
    try{
        struct C{const char* name;std::vector<Setting> cfg;};
        const std::vector<C> cases={
            {"Console",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleMode,1.0/3.0},{MixEngine::kParamConsoleDrive,0.75}}},
            {"Tube",{{MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.75},{MixEngine::kParamTubeType,0.5}}},
            {"Tape",{{MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.75},{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.9}}},
            {"Vinyl",{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.65},{MixEngine::kParamVinylWear,0.35}}}
        };
        bool ok=true;
        for(bool mixfx:{false,true}){
            for(const auto& tc:cases){
                const auto y=render(mixfx,tc.cfg);
                const double local=maxDerivative(y,kSwitchSample-8,kSwitchSample+48);
                const double before=maxDerivative(y,1024,kSwitchSample-256);
                const double after=maxDerivative(y,kSwitchSample+512,kTotal-512);
                const double steady=std::max(before,after);
                const double ratio=local/std::max(steady,1.0e-12);
                std::cout<<(mixfx?"MixFX ":"Channel ")<<tc.name
                         <<" quality-switch localDerivative="<<local
                         <<" steadyDerivative="<<steady
                         <<" ratio="<<ratio<<"\n";
                if(!std::isfinite(local)||!std::isfinite(steady)||!std::isfinite(ratio))ok=false;
                // A mode change may alter phase/colour, but must not create a
                // gross impulse beyond normal signal slew.
                if(local>steady*6.0+0.03)ok=false;
            }
        }
        std::cout<<(ok?"PASS":"FAIL")<<": live Quality switching transient audit\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"Quality switch setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Quality switch unknown exception\n";return 90;}
}
