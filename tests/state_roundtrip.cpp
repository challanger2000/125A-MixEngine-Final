#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
void addPoint(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

void applyAll(MixEngine::Processor& p,const std::array<double,MixEngine::kParamCount>& values){
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=16; setup.sampleRate=48000.0;
    if(p.setupProcessing(setup)!=kResultOk)throw 20;
    if(p.setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges changes{64},outChanges{8};
    for(ParamID id=0;id<MixEngine::kParamCount;++id)addPoint(changes,id,values[id]);

    std::array<double,16> in{},out{};
    double* ip[1]{in.data()}; double* op[1]{out.data()};
    AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=ip; ob.numChannels=1; ob.channelBuffers64=op;
    ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=16;
    d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob;
    d.inputParameterChanges=&changes; d.outputParameterChanges=&outChanges;
    if(p.process(d)!=kResultOk)throw 22;
}

std::array<double,MixEngine::kParamCount> readState(MixEngine::Processor& p){
    MemoryStream stream;
    if(p.getState(&stream)!=kResultOk)throw 30;
    if(stream.getSize()!=static_cast<TSize>(MixEngine::kParamCount*sizeof(double)))throw 31;
    if(stream.seek(0,IBStream::kIBSeekSet,nullptr)!=kResultOk)throw 32;
    IBStreamer reader(&stream,kLittleEndian);
    std::array<double,MixEngine::kParamCount> values{};
    for(auto& v:values)if(!reader.readDouble(v))throw 33;
    return values;
}
}

int main(){
    try{
        std::array<double,MixEngine::kParamCount> expected{};
        for(ParamID id=0;id<MixEngine::kParamCount;++id)
            expected[id]=std::fmod(0.137+0.271*static_cast<double>(id),1.0);

        // Keep discrete values on exact legal selector/toggle positions.
        expected[MixEngine::kParamBypass]=0.0;
        expected[MixEngine::kParamCalibration]=1.0;
        expected[MixEngine::kParamAutoGain]=1.0;
        expected[MixEngine::kParamConsoleOn]=1.0;
        expected[MixEngine::kParamConsoleMode]=2.0/3.0;
        expected[MixEngine::kParamTubeOn]=1.0;
        expected[MixEngine::kParamTapeOn]=1.0;
        expected[MixEngine::kParamTapeSpeed]=1.0;
        expected[MixEngine::kParamGlueOn]=1.0;
        expected[MixEngine::kParamVinylOn]=1.0;
        expected[MixEngine::kParamQuality]=1.0;
        expected[MixEngine::kParamTubeType]=0.5;
        expected[MixEngine::kParamMeterSource]=0.0;

        auto source=std::make_unique<MixEngine::Processor>();
        applyAll(*source,expected);
        const auto saved=readState(*source);

        MemoryStream transfer;
        if(source->getState(&transfer)!=kResultOk)throw 40;
        if(transfer.seek(0,IBStream::kIBSeekSet,nullptr)!=kResultOk)throw 41;

        auto restored=std::make_unique<MixEngine::Processor>();
        if(restored->setState(&transfer)!=kResultOk)throw 42;
        const auto roundtrip=readState(*restored);

        bool ok=true;
        double maxDiff=0.0;
        for(ParamID id=0;id<MixEngine::kParamCount;++id){
            const double a=saved[id],b=roundtrip[id];
            maxDiff=std::max(maxDiff,std::abs(a-b));
            if(std::abs(a-b)>1.0e-15)ok=false;
        }

        std::cout<<"State roundtrip parameters="<<MixEngine::kParamCount
                 <<" bytes="<<transfer.getSize()
                 <<" maxDiff="<<maxDiff<<"\n";
        std::cout<<(ok?"PASS":"FAIL")<<": complete V3 processor state/preset roundtrip\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"State roundtrip setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"State roundtrip unknown exception\n";return 90;}
}
