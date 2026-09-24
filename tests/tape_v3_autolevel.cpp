#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kBlock=256;
constexpr int kTotal=65536;
constexpr int kWarmup=16384;
constexpr double kSr=48000.0;
constexpr double kPi=3.14159265358979323846;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 qi=0;auto* q=changes.addParameterData(id,qi);if(!q)throw 10;
    int32 pi=0;if(q->addPoint(0,value,pi)!=kResultTrue)throw 11;
}

std::vector<double> inputSignal(){
    std::vector<double> x(kTotal);
    for(int n=0;n<kTotal;++n){
        const double t=double(n)/kSr;
        const double env=((n/4096)&1)?0.72:1.0;
        x[n]=env*(0.18*std::sin(2*kPi*83*t)
                 +0.12*std::sin(2*kPi*997*t+0.2)
                 +0.075*std::sin(2*kPi*4211*t+0.4)
                 +0.045*std::sin(2*kPi*9113*t+0.7));
    }
    return x;
}

std::vector<double> render(double amount,bool autoGain){
    const auto in=inputSignal();
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock;setup.sampleRate=kSr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges changes{64},outChanges{16};
    setParam(changes,MixEngine::kParamInput,0.5);
    setParam(changes,MixEngine::kParamOutput,0.5);
    setParam(changes,MixEngine::kParamCalibration,0.5);
    setParam(changes,MixEngine::kParamAutoGain,autoGain?1.0:0.0);
    setParam(changes,MixEngine::kParamConsoleOn,0.0);
    setParam(changes,MixEngine::kParamTubeOn,0.0);
    setParam(changes,MixEngine::kParamTapeOn,amount>0.0?1.0:0.0);
    setParam(changes,MixEngine::kParamTapeAmount,amount);
    setParam(changes,MixEngine::kParamTapeSpeed,0.5);
    setParam(changes,MixEngine::kParamTapeStability,0.75);
    setParam(changes,MixEngine::kParamTapeHiss,0.0);
    setParam(changes,MixEngine::kParamGlueOn,0.0);
    setParam(changes,MixEngine::kParamVinylOn,0.0);
    setParam(changes,MixEngine::kParamQuality,0.5);

    std::vector<double> out(kTotal);bool first=true;
    for(int base=0;base<kTotal;base+=kBlock){
        const int count=std::min(kBlock,kTotal-base);
        std::array<double,kBlock> ibuf{},obuf{};
        for(int i=0;i<count;++i)ibuf[i]=in[base+i];
        double* ip[1]{ibuf.data()};double* op[1]{obuf.data()};
        AudioBusBuffers ib{},ob{};ib.numChannels=1;ib.channelBuffers64=ip;ob.numChannels=1;ob.channelBuffers64=op;
        ProcessData d{};d.processMode=kRealtime;d.symbolicSampleSize=kSample64;d.numSamples=count;
        d.numInputs=1;d.numOutputs=1;d.inputs=&ib;d.outputs=&ob;d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&changes;
        if(p->process(d)!=kResultOk)throw 22;
        first=false;changes.clearQueue();
        for(int i=0;i<count;++i)out[base+i]=obuf[i];
    }
    return out;
}

double rms(const std::vector<double>& x){
    long double s=0;long long n=0;
    for(int i=kWarmup;i<kTotal;++i){s+=x[i]*x[i];++n;}
    return std::sqrt(double(s/n));
}
}

int main(){
    bool ok=true;
    const auto dry=render(0.0,true);
    const double dryR=rms(dry);
    for(double a:{0.25,0.50,0.75,1.0}){
        const auto wet=render(a,true);
        const double db=20.0*std::log10(std::max(rms(wet),1e-15)/std::max(dryR,1e-15));
        std::cout<<"Tape Auto-Level amount="<<a<<" delta="<<db<<" dB\n";
        // A level-match utility should be materially tighter than the old
        // broad sonic-impact guard. +/-0.6 dB is the V3 acceptance band.
        if(!std::isfinite(db)||std::abs(db)>0.60)ok=false;
    }
    std::cout<<(ok?"PASS":"FAIL")<<": V3 tape auto-level calibration\n";
    return ok?0:1;
}
