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
        double v=0.16*std::sin(2*kPi*83*t)
            +0.11*std::sin(2*kPi*997*t+0.17)
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
    setParam(changes,MixEngine::kParamInput,0.5);
    setParam(changes,MixEngine::kParamOutput,0.5);
    setParam(changes,MixEngine::kParamCalibration,0.5);
    setParam(changes,MixEngine::kParamAutoGain,1.0);
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
    for(const auto& [id,v]:cfg)setParam(changes,id,v);

    std::vector<double> out(kTotal,0.0); bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> in{}, y{};
        for(int i=0;i<count;++i)in[i]=input[base+i];
        double* inP[1]{in.data()}; double* outP[1]{y.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
        ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=count;
        d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob; d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&changes;
        if(p->process(d)!=kResultOk)throw 22;
        first=false; changes.clearQueue();
        for(int i=0;i<count;++i){ if(!std::isfinite(y[i]))throw 23; out[base+i]=y[i];}
    }
    return out;
}

double rms(const std::vector<double>& x){
    long double s=0.0; int n=0;
    for(int i=kWarmup;i<kTotal;++i){s+=x[i]*x[i];++n;}
    return std::sqrt(double(s/n));
}

double diffRmsDb(const std::vector<double>& a,const std::vector<double>& b){
    long double s=0.0, ref=0.0; int n=0;
    for(int i=kWarmup;i<kTotal;++i){
        const double d=a[i]-b[i]; s+=d*d; ref+=a[i]*a[i]; ++n;
    }
    const double dr=std::sqrt(double(s/n)), rr=std::sqrt(double(ref/n));
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
            const bool dynamic=std::string(m.name)=="Glue";
            const auto& dryRef=dynamic?dryDynamic:dry;
            auto baseCfg=m.extra; baseCfg.push_back({m.on,1.0}); baseCfg.push_back({m.amount,0.0});
            const auto moduleBase=render(baseCfg,dynamic);
            double previousAmountDelta=-200.0;
            for(double amount:{0.25,0.50,0.75,1.0}){
                auto cfg=m.extra; cfg.push_back({m.on,1.0}); cfg.push_back({m.amount,amount});
                const auto wet=render(cfg,dynamic);
                const double absoluteDelta=diffRmsDb(dryRef,wet);
                const double amountDelta=diffRmsDb(moduleBase,wet);
                const double wetRmsDb=20.0*std::log10(std::max(rms(wet),1e-15)/std::max(rms(dryRef),1e-15));
                std::cout<<m.name<<" "<<int(amount*100)
                         <<"% absolute="<<absoluteDelta
                         <<" dBFSrel amount-from-base="<<amountDelta
                         <<" dBFSrel level="<<wetRmsDb<<" dB\n";
                if(!std::isfinite(absoluteDelta)||!std::isfinite(amountDelta)||!std::isfinite(wetRmsDb))ok=false;
                // Absolute impact remains measured against true module-OFF dry.
                if(amount==0.25 && absoluteDelta<-42.0) ok=false;
                if(amount==0.50 && absoluteDelta<-34.0) ok=false;
                if(amount==0.75 && absoluteDelta<-28.0) ok=false;
                if(amount==1.00 && absoluteDelta<-24.0) ok=false;
                // Control travel itself must progress relative to the enabled
                // module's deliberate 0 % base character.
                if(amountDelta+0.25<previousAmountDelta) ok=false;
                previousAmountDelta=amountDelta;
            }
        }
        // Character controls must not be cosmetic: their endpoints should create
        // clearly different, finite sonic signatures at a production amount.
        const auto tubeWarm=render({
            {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.70},{MixEngine::kParamTubeType,0.0}});
        const auto tubeHot=render({
            {MixEngine::kParamTubeOn,1.0},{MixEngine::kParamTubeAmount,0.70},{MixEngine::kParamTubeType,1.0}});
        const double tubeVoiceDelta=diffRmsDb(tubeWarm,tubeHot);
        std::cout<<"Tube voice endpoint delta="<<tubeVoiceDelta<<" dBFSrel\n";
        if(!std::isfinite(tubeVoiceDelta)||tubeVoiceDelta<-34.0)ok=false;

        const auto tapeSlow=render({
            {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},
            {MixEngine::kParamTapeSpeed,0.0},{MixEngine::kParamTapeStability,0.75}});
        const auto tapeFast=render({
            {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},
            {MixEngine::kParamTapeSpeed,1.0},{MixEngine::kParamTapeStability,0.75}});
        const double tapeSpeedDelta=diffRmsDb(tapeSlow,tapeFast);
        std::cout<<"Tape speed endpoint delta="<<tapeSpeedDelta<<" dBFSrel\n";
        if(!std::isfinite(tapeSpeedDelta)||tapeSpeedDelta<-34.0)ok=false;

        const auto tapeStable=render({
            {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},
            {MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,1.0}});
        const auto tapeLoose=render({
            {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},
            {MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.50}});
        const auto tapeWild=render({
            {MixEngine::kParamTapeOn,1.0},{MixEngine::kParamTapeAmount,0.70},
            {MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeStability,0.0}});
        const double looseDelta=diffRmsDb(tapeStable,tapeLoose);
        const double wildDelta=diffRmsDb(tapeStable,tapeWild);
        std::cout<<"Tape stability 50% delta="<<looseDelta<<" dBFSrel 0% delta="<<wildDelta<<" dBFSrel\n";
        if(!std::isfinite(looseDelta)||!std::isfinite(wildDelta))ok=false;
        if(looseDelta<-36.0||wildDelta<-28.0)ok=false;
        if(wildDelta<looseDelta+3.0)ok=false;

        std::cout<<(ok?"PASS":"FAIL")<<": sonic-impact audit\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"Sonic-impact setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Sonic-impact unknown exception\n";return 90;}
}
