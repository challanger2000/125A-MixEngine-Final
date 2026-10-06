#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double sr=48000.0;
constexpr double pi=3.14159265358979323846;
constexpr int block=256;
constexpr int count=131072;
constexpr int warm=16384;

void setParam(ParameterChanges& c,ParamID id,double v){
    int32 qi=0; auto* q=c.addParameterData(id,qi); if(!q)throw 10;
    int32 pi=0; if(q->addPoint(0,std::clamp(v,0.0,1.0),pi)!=kResultTrue)throw 11;
}

std::vector<double> render(double frequency,double quality,double color,double wear){
    auto p=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=block; setup.sampleRate=sr;
    if(p->setupProcessing(setup)!=kResultOk)throw 20;
    if(p->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges init{64},outChanges{8};
    const std::vector<std::pair<ParamID,double>> cfg={
      {MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},{MixEngine::kParamCalibration,0.5},
      {MixEngine::kParamAutoGain,0.0},{MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamTubeOn,0.0},
      {MixEngine::kParamTapeOn,0.0},{MixEngine::kParamGlueOn,0.0},
      {MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,color},{MixEngine::kParamVinylWear,wear},
      {MixEngine::kParamVinylNoise,0.0},{MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},
      {MixEngine::kParamLowMono,0.0},{MixEngine::kParamQuality,quality}
    };
    for(const auto& [id,v]:cfg)setParam(init,id,v);

    std::vector<double> out(count,0.0); bool first=true;
    for(int base=0;base<count;base+=block){
        const int nCount=std::min(block,count-base);
        std::array<double,block> in{},y{};
        for(int i=0;i<nCount;++i){
            const int n=base+i;
            in[static_cast<std::size_t>(i)]=0.075*std::sin(2.0*pi*frequency*double(n)/sr);
        }
        double* inP[1]{in.data()}; double* outP[1]{y.data()};
        AudioBusBuffers ib{},ob{}; ib.numChannels=1; ib.channelBuffers64=inP; ob.numChannels=1; ob.channelBuffers64=outP;
        ProcessData d{}; d.processMode=kRealtime; d.symbolicSampleSize=kSample64; d.numSamples=nCount;
        d.numInputs=1; d.numOutputs=1; d.inputs=&ib; d.outputs=&ob; d.outputParameterChanges=&outChanges;
        if(first)d.inputParameterChanges=&init;
        if(p->process(d)!=kResultOk)throw 22;
        first=false; init.clearQueue();
        for(int i=0;i<nCount;++i){
            if(!std::isfinite(y[static_cast<std::size_t>(i)]))throw 23;
            out[static_cast<std::size_t>(base+i)]=y[static_cast<std::size_t>(i)];
        }
    }
    return out;
}

double toneMag(const std::vector<double>& y,double f){
    long double re=0.0,im=0.0; long long nCount=0;
    for(int n=warm;n<count;++n){
        const double ph=2.0*pi*f*double(n)/sr;
        re+=y[static_cast<std::size_t>(n)]*std::cos(ph);
        im-=y[static_cast<std::size_t>(n)]*std::sin(ph);
        ++nCount;
    }
    return nCount?2.0*std::sqrt(double(re*re+im*im))/double(nCount):0.0;
}
double dbRatio(double a,double b){return 20.0*std::log10(std::max(a,1e-30)/std::max(b,1e-30));}
double diffRms(const std::vector<double>& a,const std::vector<double>& b){
    long double d=0.0,r=0.0; long long nCount=0;
    for(int n=warm;n<count;++n){const double e=a[n]-b[n];d+=e*e;r+=a[n]*a[n];++nCount;}
    return 20.0*std::log10(std::max(std::sqrt(double(d/nCount)),1e-30)/std::max(std::sqrt(double(r/nCount)),1e-30));
}
}

int main(){
    try{
        bool ok=true;
        constexpr double f=15000.0,alias=18000.0;
        const auto eco=render(f,0.0,1.0,0.0);
        const auto normal=render(f,0.5,1.0,0.0);
        const auto high=render(f,1.0,1.0,0.0);
        for(const auto& q: {std::pair<const char*,const std::vector<double>*>{"Normal",&normal},
                            std::pair<const char*,const std::vector<double>*>{"High",&high}}){
            const double fund=toneMag(*q.second,f);
            const double ali=toneMag(*q.second,alias);
            const double dbc=dbRatio(ali,fund);
            std::cout<<q.first<<" live Vinyl alias="<<dbc<<" dBc fundamental="<<fund<<" alias="<<ali<<"\n";
            if(!std::isfinite(dbc)||fund<1e-5||dbc>-75.0)ok=false;
        }
        const double ecoNormal=diffRms(eco,normal);
        const double normalHigh=diffRms(normal,high);
        std::cout<<"Eco->Normal delta="<<ecoNormal<<" dBFSrel Normal->High delta="<<normalHigh<<" dBFSrel\n";
        // Normal must materially differ because the physical tracing term becomes
        // active there. High may differ only subtly because 2x already met the
        // measured alias target.
        if(!std::isfinite(ecoNormal)||ecoNormal<-60.0)ok=false;
        if(!std::isfinite(normalHigh))ok=false;

        std::cout<<(ok?"PASS":"FAIL")<<": Vinyl V3 live integration and alias gate\n";
        return ok?0:1;
    }catch(int c){std::cerr<<"Vinyl V3 live setup FAIL "<<c<<"\n";return c;}
    catch(...){std::cerr<<"Vinyl V3 live unknown exception\n";return 90;}
}
