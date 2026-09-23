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
    int32 qi=0; auto* q=changes.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,value,pi)!=kResultTrue)throw 11;
}

std::vector<double> makeSignal(bool dynamic=false){
    std::vector<double> x(kTotal);
    for(int n=0;n<kTotal;++n){
        const double t=double(n)/kSr;
        double v=0.17*std::sin(2*kPi*83*t)
            +0.12*std::sin(2*kPi*997*t+0.17)
            +0.07*std::sin(2*kPi*4211*t+0.31)
            +0.04*std::sin(2*kPi*9113*t+0.49);
        if(dynamic){
            const int phase=n%2048;
            const double env=phase<256?1.45:0.55;
            v*=env;
        }
        x[n]=v;
    }
    return x;
}

std::vector<double> render(const std::vector<std::pair<ParamID,double>>& cfg,bool dynamic=false){
    const auto input=makeSignal(dynamic);
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges changes{64}, outChanges{16};
    const std::vector<std::pair<ParamID,double>> base={
        {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
        {MixEngine::kParamAutoGain,1.0},{MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamTubeOn,0.0},
        {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},
        {MixEngine::kParamConsoleNoise,0.0},{MixEngine::kParamTapeHiss,0.0},{MixEngine::kParamVinylNoise,0.0},
        {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
        {MixEngine::kParamQuality,0.5}
    };
    for(const auto& [id,v]:base)setParam(changes,id,v);
    for(const auto& [id,v]:cfg)setParam(changes,id,v);

    std::vector<double> out(kTotal,0.0); bool first=true;
    for(int baseSample=0;baseSample<kTotal;baseSample+=kBlock){
        const int count=std::min(kBlock,kTotal-baseSample);
        std::array<double,kBlock> in{}, y{};
        for(int i=0;i<count;++i)in[i]=input[baseSample+i];
        double* inP[1]{in.data()}; double* outP[1]{y.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
        ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
        d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob; d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&changes;
        if(p->process(d)!=kResultOk)throw 22;
        first=false; changes.clearQueue();
        for(int i=0;i<count;++i){if(!std::isfinite(y[i]))throw 23;out[baseSample+i]=y[i];}
    }
    return out;
}

double diffRmsDb(const std::vector<double>& a,const std::vector<double>& b){
    long double s=0.0,ref=0.0; int n=0;
    for(int i=kWarmup;i<kTotal;++i){const double d=a[i]-b[i];s+=d*d;ref+=a[i]*a[i];++n;}
    const double dr=std::sqrt(double(s/n)),rr=std::sqrt(double(ref/n));
    return 20.0*std::log10(std::max(dr,1e-15)/std::max(rr,1e-15));
}

struct Module{
    const char* name;
    ParamID on;
    ParamID amount;
    std::vector<std::pair<ParamID,double>> extra;
};
}

int main(){
    try{
        const auto dry=render({});
        const auto dryDynamic=render({},true);
        const std::vector<Module> modules={
            {"Console",MixEngine::kParamConsoleOn,MixEngine::kParamConsoleDrive,{{MixEngine::kParamConsoleMode,1.0/3.0}}},
            {"Tube",MixEngine::kParamTubeOn,MixEngine::kParamTubeAmount,{{MixEngine::kParamTubeType,0.5}}},
            {"Tape",MixEngine::kParamTapeOn,MixEngine::kParamTapeAmount,{{MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.75}}},
            {"Glue",MixEngine::kParamGlueOn,MixEngine::kParamGlueAmount,{{MixEngine::kParamGlueCharacter,0.5}}},
            {"Vinyl",MixEngine::kParamVinylOn,MixEngine::kParamVinylCharacter,{{MixEngine::kParamVinylWear,0.25}}}
        };

        bool ok=true;
        for(const auto& m:modules){
            std::array<double,4> delta{};
            const std::array<double,4> amounts{{0.25,0.50,0.75,1.00}};
            for(std::size_t i=0;i<amounts.size();++i){
                auto cfg=m.extra; cfg.push_back({m.on,1.0}); cfg.push_back({m.amount,amounts[i]});
                const bool dynamic=std::string(m.name)=="Glue";
                delta[i]=diffRmsDb(dynamic?dryDynamic:dry,render(cfg,dynamic));
            }
            const double midToMax=delta[3]-delta[1];
            const double upperStep=delta[3]-delta[2];
            std::cout<<m.name
                     <<" 25="<<delta[0]
                     <<" 50="<<delta[1]
                     <<" 75="<<delta[2]
                     <<" 100="<<delta[3]
                     <<" 50->100="<<midToMax
                     <<" 75->100="<<upperStep<<" dB\n";

            // 20-50% remains the useful working zone: audible, but not already maxed out.
            if(delta[1] < -36.0 || delta[1] > -3.0) ok=false;

            // Upper settings must keep opening up, but without an artificial
            // knee or sudden "turbo" jump. The response should stay progressive.
            if(midToMax < 2.0) ok=false;
            if(upperStep < 0.40) ok=false;
            const double lowerStep=delta[2]-delta[1];
            if(upperStep > lowerStep*2.75 + 0.5) ok=false;

            // 100% may be excessive by taste, but must remain finite/bounded.
            if(!std::isfinite(delta[3]) || delta[3] < -24.0) ok=false;
        }

        // Fine-grained continuity scan: the audible change per 10% control
        // movement may grow toward the top, but adjacent steps must not explode.
        for(const auto& m:modules){
            std::vector<double> fine;
            for(int step=1;step<=10;++step){
                const double amount=0.1*step;
                auto cfg=m.extra; cfg.push_back({m.on,1.0}); cfg.push_back({m.amount,amount});
                const bool dynamic=std::string(m.name)=="Glue";
                fine.push_back(diffRmsDb(dynamic?dryDynamic:dry,render(cfg,dynamic)));
            }
            std::cout<<m.name<<" fine steps:";
            for(double v:fine)std::cout<<" "<<v;
            std::cout<<" dB\n";

            for(std::size_t i=1;i<fine.size();++i){
                const double stepNow=fine[i]-fine[i-1];
                if(stepNow < -0.10) ok=false;
                if(i>=2){
                    const double stepPrev=fine[i-1]-fine[i-2];
                    if(stepNow > stepPrev*2.2 + 0.65) ok=false;
                }
            }
        }

        std::cout<<(ok?"PASS":"FAIL")
                 <<": 125A range philosophy (sweet spot 20-50, strong/creative 75-100)\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"Range philosophy setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Range philosophy unknown exception\n";return 90;}
}
