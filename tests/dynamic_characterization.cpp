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
constexpr double kSr=48000.0;
constexpr int kBlock=256;
constexpr double kPi=3.14159265358979323846;
using Param=std::pair<ParamID,double>;

void setParam(ParameterChanges& c,ParamID id,double value){
    int32 q=0; auto* queue=c.addParameterData(id,q); if(!queue)throw 10;
    int32 p=0; if(queue->addPoint(0,std::clamp(value,0.0,1.0),p)!=kResultTrue)throw 11;
}

std::vector<double> render(const std::vector<double>& input,const std::vector<Param>& overrides){
    auto processor=std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{}; setup.processMode=kRealtime; setup.symbolicSampleSize=kSample64;
    setup.maxSamplesPerBlock=kBlock; setup.sampleRate=kSr;
    if(processor->setupProcessing(setup)!=kResultOk)throw 20;
    if(processor->setProcessing(true)!=kResultOk)throw 21;

    ParameterChanges changes{64};
    for(const auto& p:std::vector<Param>{
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
    for(const auto& p:overrides)setParam(changes,p.first,p.second);

    std::vector<double> output(input.size(),0.0);
    std::array<double,kBlock> in{},out{};
    double* inPtr[1]{in.data()}; double* outPtr[1]{out.data()};
    AudioBusBuffers inBus{},outBus{};
    inBus.numChannels=1; inBus.channelBuffers64=inPtr;
    outBus.numChannels=1; outBus.channelBuffers64=outPtr;

    bool first=true;
    for(std::size_t base=0;base<input.size();base+=kBlock){
        const int count=static_cast<int>(std::min<std::size_t>(kBlock,input.size()-base));
        std::fill(in.begin(),in.end(),0.0); std::fill(out.begin(),out.end(),0.0);
        for(int i=0;i<count;++i)in[static_cast<std::size_t>(i)]=input[base+static_cast<std::size_t>(i)];
        ProcessData data{}; data.processMode=kRealtime; data.symbolicSampleSize=kSample64;
        data.numSamples=count; data.numInputs=1; data.numOutputs=1;
        data.inputs=&inBus; data.outputs=&outBus; data.inputParameterChanges=first?&changes:nullptr;
        if(processor->process(data)!=kResultOk)throw 22;
        first=false;
        for(int i=0;i<count;++i){
            const double y=out[static_cast<std::size_t>(i)];
            if(!std::isfinite(y))throw 23;
            output[base+static_cast<std::size_t>(i)]=y;
        }
    }
    return output;
}

double rmsRange(const std::vector<double>& x,double startSec,double endSec){
    const int delay=MixEngine::kFixedLatencySamples;
    const int start=std::max(delay,static_cast<int>(std::lround(startSec*kSr))+delay);
    const int end=std::min(static_cast<int>(x.size()),static_cast<int>(std::lround(endSec*kSr))+delay);
    long double sum=0.0; int n=0;
    for(int i=start;i<end;++i){const double v=x[static_cast<std::size_t>(i)];sum+=v*v;++n;}
    return n?std::sqrt(static_cast<double>(sum/static_cast<long double>(n))):0.0;
}
double dbRatio(double a,double b){return 20.0*std::log10(std::max(a,1e-15)/std::max(b,1e-15));}

double sineResidual(const std::vector<double>& x,double f,double startSec,double endSec){
    const int delay=MixEngine::kFixedLatencySamples;
    const int start=std::max(delay,static_cast<int>(std::lround(startSec*kSr))+delay);
    const int end=std::min(static_cast<int>(x.size()),static_cast<int>(std::lround(endSec*kSr))+delay);
    long double ss=0,cc=0,sc=0,sy=0,cy=0;
    for(int n=start;n<end;++n){
        const double t=static_cast<double>(n-delay)/kSr,p=2.0*kPi*f*t,s=std::sin(p),c=std::cos(p),y=x[static_cast<std::size_t>(n)];
        ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y;cy+=c*y;
    }
    const long double det=ss*cc-sc*sc;
    const double a=static_cast<double>((sy*cc-cy*sc)/det),b=static_cast<double>((cy*ss-sy*sc)/det);
    long double err=0,energy=0;
    for(int n=start;n<end;++n){
        const double t=static_cast<double>(n-delay)/kSr,p=2.0*kPi*f*t,y=x[static_cast<std::size_t>(n)];
        const double fit=a*std::sin(p)+b*std::cos(p),e=y-fit;err+=e*e;energy+=y*y;
    }
    return energy>0?std::sqrt(static_cast<double>(err/energy)):0.0;
}
}

int main(){
    try{
        bool ok=true;

        // Tube memory / sag: a hot burst must leave a short-lived change in the
        // following low-level probe, then recover toward the pre-burst state.
        {
            const int total=static_cast<int>(1.6*kSr);
            std::vector<double> in(static_cast<std::size_t>(total));
            for(int n=0;n<total;++n){
                const double t=static_cast<double>(n)/kSr;
                const double level=(t>=0.50&&t<0.90)?std::pow(10.0,-5.0/20.0):std::pow(10.0,-24.0/20.0);
                in[static_cast<std::size_t>(n)]=level*std::sin(2.0*kPi*997.0*t);
            }
            for(int type=0;type<3;++type){
                const auto out=render(in,{{MixEngine::kParamTubeOn,1.0},
                    {MixEngine::kParamTubeType,static_cast<double>(type)/2.0},
                    {MixEngine::kParamTubeAmount,0.75}});
                const double before=rmsRange(out,0.30,0.45);
                const double early=rmsRange(out,0.92,1.02);
                const double late=rmsRange(out,1.40,1.55);
                const double memoryDb=dbRatio(early,late);
                const double recoveryDb=dbRatio(late,before);
                std::cout<<"TubeMemory type="<<type<<" early-vs-late="<<memoryDb
                         <<" dB late-vs-before="<<recoveryDb<<" dB\n";
                ok=ok&&std::isfinite(memoryDb)&&std::isfinite(recoveryDb)
                     &&std::abs(memoryDb)>0.002&&std::abs(recoveryDb)<0.35;
            }
        }

        // Glue response: RESPONSE must produce measurably different attack and
        // release trajectories for the same programme step.
        {
            const int total=static_cast<int>(1.4*kSr);
            std::vector<double> in(static_cast<std::size_t>(total));
            for(int n=0;n<total;++n){
                const double t=static_cast<double>(n)/kSr;
                double db=-24.0; if(t>=0.35&&t<0.85)db=-8.0;
                const double level=std::pow(10.0,db/20.0);
                in[static_cast<std::size_t>(n)]=level*std::sin(2.0*kPi*997.0*t);
            }
            std::array<double,3> attack{},release{};
            for(int ri=0;ri<3;++ri){
                const double response=0.5*ri;
                const auto out=render(in,{{MixEngine::kParamGlueOn,1.0},
                    {MixEngine::kParamGlueAmount,0.70},{MixEngine::kParamGlueCharacter,response}});
                const double hotEarly=rmsRange(out,0.36,0.39);
                const double hotLate=rmsRange(out,0.72,0.82);
                const double lowEarly=rmsRange(out,0.86,0.91);
                const double lowLate=rmsRange(out,1.25,1.35);
                attack[static_cast<std::size_t>(ri)]=dbRatio(hotEarly,hotLate);
                release[static_cast<std::size_t>(ri)]=dbRatio(lowEarly,lowLate);
                std::cout<<"GlueDynamics response="<<response
                         <<" attackShape="<<attack[static_cast<std::size_t>(ri)]
                         <<" dB releaseShape="<<release[static_cast<std::size_t>(ri)]<<" dB\n";
                ok=ok&&std::isfinite(attack[static_cast<std::size_t>(ri)])
                     &&std::isfinite(release[static_cast<std::size_t>(ri)]);
            }
            const double attackSpread=*std::max_element(attack.begin(),attack.end())-*std::min_element(attack.begin(),attack.end());
            const double releaseSpread=*std::max_element(release.begin(),release.end())-*std::min_element(release.begin(),release.end());
            std::cout<<"GlueDynamics attackSpread="<<attackSpread
                     <<" dB releaseSpread="<<releaseSpread<<" dB\n";
            ok=ok&&(attackSpread>0.02||releaseSpread>0.02);
        }

        // Tape Stability: compare the same processed programme with transport
        // perfectly stable versus fully unstable. A direct waveform residual is
        // much more sensitive to time-varying phase/pitch motion than comparing
        // each render's total nonlinear sine residual, which is dominated by the
        // magnetic harmonics common to both cases.
        {
            const int total=static_cast<int>(2.0*kSr);
            std::vector<double> in(static_cast<std::size_t>(total));
            for(int n=0;n<total;++n){
                const double t=static_cast<double>(n)/kSr;
                in[static_cast<std::size_t>(n)]=std::pow(10.0,-18.0/20.0)*std::sin(2.0*kPi*3000.0*t);
            }
            const auto stable=render(in,{{MixEngine::kParamTapeOn,1.0},
                {MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeAmount,0.35},
                {MixEngine::kParamTapeStability,1.0}});
            const auto unstable=render(in,{{MixEngine::kParamTapeOn,1.0},
                {MixEngine::kParamTapeSpeed,0.5},{MixEngine::kParamTapeAmount,0.35},
                {MixEngine::kParamTapeStability,0.0}});

            const int delay=MixEngine::kFixedLatencySamples;
            const int start=static_cast<int>(0.6*kSr)+delay;
            const int end=std::min(static_cast<int>(stable.size()),
                                   static_cast<int>(1.9*kSr)+delay);
            long double diffE=0.0,refE=0.0;
            for(int n=start;n<end;++n){
                const double s=stable[static_cast<std::size_t>(n)];
                const double d=unstable[static_cast<std::size_t>(n)]-s;
                diffE+=d*d;
                refE+=s*s;
            }
            const double residual=refE>0.0
                ? std::sqrt(static_cast<double>(diffE/refE)) : 0.0;
            const double residualDb=20.0*std::log10(std::max(residual,1e-15));
            std::cout<<"TapeStability unstable-vs-stable residual="
                     <<residual<<" ("<<residualDb<<" dB relative)\n";

            // Must be clearly measurable but remain a modulation effect rather
            // than a wholesale level/tonal replacement.
            ok=ok&&std::isfinite(residualDb)
                 &&residualDb>-80.0&&residualDb<-6.0;
        }

        if(!ok){
            std::cerr<<"FAILED: V2 dynamic/model-memory characterization\n";
            return 1;
        }
        std::cout<<"PASSED: V2 dynamic/model-memory characterization\n";
        return 0;
    }catch(int e){
        std::cerr<<"Dynamic characterization setup failure: "<<e<<"\n"; return e;
    }catch(...){
        std::cerr<<"Dynamic characterization unknown failure\n"; return 90;
    }
}
