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
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr int32 kBlockSize = 256;
constexpr int kWarmup = 12000;
constexpr int kAnalysis = 48000;
constexpr int kTotal = kWarmup + kAnalysis + MixEngine::kFixedLatencySamples + 512;

double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
double gainToDb(double g) { return 20.0 * std::log10(std::max(g, 1.0e-15)); }

void setParam(ParameterChanges& changes, ParamID id, double value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    if (!queue) throw 10;
    int32 pointIndex = 0;
    if (queue->addPoint(0, std::clamp(value, 0.0, 1.0), pointIndex) != kResultTrue) throw 11;
}

using Param = std::pair<ParamID,double>;

std::vector<float> renderSine(double frequency, double amplitude, const std::vector<Param>& overrides) {
    auto processor = std::make_unique<MixEngine::Processor>();
    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = kSampleRate;
    if (processor->setupProcessing(setup) != kResultOk) throw 20;
    if (processor->setProcessing(true) != kResultOk) throw 21;

    std::array<double, MixEngine::kParamCount> values{};
    std::array<bool, MixEngine::kParamCount> used{};
    auto put=[&](ParamID id,double value) {
        values[static_cast<std::size_t>(id)] = value;
        used[static_cast<std::size_t>(id)] = true;
    };

    put(MixEngine::kParamBypass, 0.0);
    put(MixEngine::kParamInput, 0.5);
    put(MixEngine::kParamOutput, 0.5);
    put(MixEngine::kParamCalibration, 0.0); // -18 dBFS = 0 VU for V2 characterization
    put(MixEngine::kParamAutoGain, 0.0);    // raw module signature, no compensation
    put(MixEngine::kParamConsoleOn, 0.0);
    put(MixEngine::kParamTubeOn, 0.0);
    put(MixEngine::kParamTapeOn, 0.0);
    put(MixEngine::kParamGlueOn, 0.0);
    put(MixEngine::kParamVinylOn, 0.0);
    put(MixEngine::kParamConsoleNoise, 0.0);
    put(MixEngine::kParamTapeHiss, 0.0);
    put(MixEngine::kParamVinylNoise, 0.0);
    put(MixEngine::kParamConsoleCrosstalk, 0.0);
    put(MixEngine::kParamWidth, 0.5);
    put(MixEngine::kParamDepth, 0.5);
    put(MixEngine::kParamLowMono, 0.0);
    put(MixEngine::kParamQuality, 1.0);
    for (const auto& p : overrides) put(p.first, p.second);

    ParameterChanges changes{64};
    for (ParamID id=0; id<MixEngine::kParamCount; ++id)
        if (used[static_cast<std::size_t>(id)])
            setParam(changes, id, values[static_cast<std::size_t>(id)]);

    std::vector<float> output(kTotal,0.0f);
    std::array<float,kBlockSize> in{},out{};
    float* inPtr[1]{in.data()};
    float* outPtr[1]{out.data()};
    AudioBusBuffers inBus{},outBus{};
    inBus.numChannels=1; inBus.channelBuffers32=inPtr;
    outBus.numChannels=1; outBus.channelBuffers32=outPtr;

    bool first=true;
    for(int base=0;base<kTotal;base+=kBlockSize) {
        const int count=std::min<int>(kBlockSize,kTotal-base);
        std::fill(in.begin(),in.end(),0.0f);
        std::fill(out.begin(),out.end(),0.0f);
        for(int i=0;i<count;++i) {
            const int n=base+i;
            in[static_cast<std::size_t>(i)] = static_cast<float>(
                amplitude*std::sin(2.0*kPi*frequency*static_cast<double>(n)/kSampleRate));
        }
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=count;
        data.numInputs=1; data.numOutputs=1;
        data.inputs=&inBus; data.outputs=&outBus;
        data.inputParameterChanges=first?&changes:nullptr;
        if(processor->process(data)!=kResultOk) throw 22;
        first=false;
        for(int i=0;i<count;++i) output[static_cast<std::size_t>(base+i)]=out[static_cast<std::size_t>(i)];
    }
    return output;
}

struct Harmonics {
    double fundamental=0.0;
    std::array<double,5> h{};
    double thd=0.0;
};

Harmonics analyze(const std::vector<float>& x,double frequency) {
    const int start=kWarmup+MixEngine::kFixedLatencySamples;
    Harmonics result{};
    for(int harmonic=1;harmonic<=5;++harmonic) {
        long double re=0.0,im=0.0;
        const double omega=2.0*kPi*frequency*static_cast<double>(harmonic)/kSampleRate;
        for(int i=0;i<kAnalysis;++i) {
            const double y=x[static_cast<std::size_t>(start+i)];
            const double phase=omega*static_cast<double>(i);
            re += y*std::cos(phase);
            im -= y*std::sin(phase);
        }
        const double amp=2.0*std::sqrt(static_cast<double>(re*re+im*im))/static_cast<double>(kAnalysis);
        result.h[static_cast<std::size_t>(harmonic-1)]=amp;
    }
    result.fundamental=result.h[0];
    long double distortion=0.0;
    for(int i=1;i<5;++i) distortion += result.h[static_cast<std::size_t>(i)]*result.h[static_cast<std::size_t>(i)];
    result.thd=result.fundamental>1.0e-15?std::sqrt(static_cast<double>(distortion))/result.fundamental:0.0;
    return result;
}

void printRow(const std::string& module,double levelDb,double frequency,const Harmonics& h,double inputAmp) {
    std::cout<<std::left<<std::setw(10)<<module
             <<" in="<<std::setw(6)<<levelDb
             <<" f="<<std::setw(7)<<frequency
             <<" gain="<<std::setw(9)<<gainToDb(h.fundamental/inputAmp)
             <<" H2="<<std::setw(9)<<gainToDb(h.h[1]/std::max(h.fundamental,1.0e-15))
             <<" H3="<<std::setw(9)<<gainToDb(h.h[2]/std::max(h.fundamental,1.0e-15))
             <<" H4="<<std::setw(9)<<gainToDb(h.h[3]/std::max(h.fundamental,1.0e-15))
             <<" H5="<<std::setw(9)<<gainToDb(h.h[4]/std::max(h.fundamental,1.0e-15))
             <<" THD="<<100.0*h.thd<<"%\n";
}

struct ModuleConfig {
    const char* name;
    std::vector<Param> params;
};

}

int main() {
    try {
        const std::vector<ModuleConfig> modules = {
            {"Dry", {}},
            {"Console", {
                {MixEngine::kParamConsoleOn,1.0},
                {MixEngine::kParamConsoleMode,1.0/3.0},
                {MixEngine::kParamConsoleDrive,0.5}
            }},
            {"Tube", {
                {MixEngine::kParamTubeOn,1.0},
                {MixEngine::kParamTubeType,0.5},
                {MixEngine::kParamTubeAmount,0.5}
            }},
            {"Tape", {
                {MixEngine::kParamTapeOn,1.0},
                {MixEngine::kParamTapeSpeed,0.5},
                {MixEngine::kParamTapeAmount,0.5},
                {MixEngine::kParamTapeStability,1.0}
            }},
            {"Glue", {
                {MixEngine::kParamGlueOn,1.0},
                {MixEngine::kParamGlueAmount,0.5},
                {MixEngine::kParamGlueCharacter,0.5}
            }},
            {"Vinyl", {
                {MixEngine::kParamVinylOn,1.0},
                {MixEngine::kParamVinylCharacter,0.5},
                {MixEngine::kParamVinylWear,0.25}
            }}
        };

        std::cout<<std::fixed<<std::setprecision(4);
        std::cout<<"=== MixEngine sonic characterization: 1 kHz level sweep ===\n";
        for(const auto& module:modules) {
            for(double levelDb:{-30.0,-18.0,-12.0,-6.0}) {
                const double a=dbToGain(levelDb);
                const auto out=renderSine(1000.0,a,module.params);
                const auto h=analyze(out,1000.0);
                if(!std::isfinite(h.thd)||!std::isfinite(h.fundamental)) return 30;
                printRow(module.name,levelDb,1000.0,h,a);
            }
        }

        std::cout<<"=== Frequency-dependent signature at -18 dBFS ===\n";
        for(const auto& module:modules) {
            for(double frequency:{80.0,1000.0,8000.0}) {
                const double a=dbToGain(-18.0);
                const auto out=renderSine(frequency,a,module.params);
                const auto h=analyze(out,frequency);
                if(!std::isfinite(h.thd)||!std::isfinite(h.fundamental)) return 31;
                printRow(module.name,-18.0,frequency,h,a);
            }
        }

        std::cout<<"Sonic characterization completed with finite results\n";
        return 0;
    } catch(int code) {
        std::cerr<<"Sonic characterization setup failed: "<<code<<"\n";
        return code;
    } catch(...) {
        std::cerr<<"Sonic characterization unknown failure\n";
        return 90;
    }
}
