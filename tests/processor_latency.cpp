#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int32 kBlockSize = 128;
constexpr double kSampleRate = 48000.0;

void setParam(ParameterChanges& changes, ParamID id, double value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    if (!queue) throw 10;
    int32 pointIndex = 0;
    if (queue->addPoint(0, value, pointIndex) != kResultTrue) throw 11;
}

int impulsePeak(const std::vector<std::pair<ParamID,double>>& params, double minPeak = 0.99) {
    auto processor = std::make_unique<MixEngine::Processor>();

    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = kSampleRate;
    if (processor->setupProcessing(setup) != kResultOk) throw 20;
    if (processor->setProcessing(true) != kResultOk) throw 21;
    if (processor->getLatencySamples() != MixEngine::v3ReportedLatencySamples(kSampleRate)) throw 22;

    std::array<float,kBlockSize> inL{}, inR{}, outL{}, outR{};
    inL[0] = 1.0f;
    inR[0] = 1.0f;
    float* inPtrs[2]{inL.data(),inR.data()};
    float* outPtrs[2]{outL.data(),outR.data()};

    AudioBusBuffers inBus{};
    AudioBusBuffers outBus{};
    inBus.numChannels = 2;
    inBus.channelBuffers32 = inPtrs;
    outBus.numChannels = 2;
    outBus.channelBuffers32 = outPtrs;

    ParameterChanges inputChanges{32};
    ParameterChanges outputChanges{16};
    // Remove the default Console coloration so zero-effect cases are truly linear.
    setParam(inputChanges, MixEngine::kParamConsoleOn, 0.0);
    for (const auto& [id,value] : params)
        setParam(inputChanges,id,value);

    ProcessData data{};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample32;
    data.numSamples = kBlockSize;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inBus;
    data.outputs = &outBus;
    data.inputParameterChanges = &inputChanges;
    data.outputParameterChanges = &outputChanges;

    if (processor->process(data) != kResultOk) throw 23;

    int peakIndex = 0;
    double peak = 0.0;
    for (int i=0;i<kBlockSize;++i) {
        const double a = std::abs(static_cast<double>(outL[static_cast<std::size_t>(i)]));
        if (a > peak) { peak = a; peakIndex = i; }
    }
    if (!std::isfinite(peak) || peak < minPeak) throw 24;
    return peakIndex;
}

int mixFxImpulsePeak(const std::vector<std::pair<ParamID,double>>& params, double minPeak = 0.99) {
    auto processor = std::make_unique<MixEngine::Processor>();

    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate = kSampleRate;
    if (processor->setupProcessing(setup) != kResultOk) throw 30;
    if (processor->setProcessing(true) != kResultOk) throw 31;

    SpeakerArrangement arrangement = SpeakerArr::kStereo;
    if (processor->setMixChannelArrangements(&arrangement, 1) != kResultOk) throw 32;

    ParameterChanges controlChanges{32};
    ParameterChanges controlOutput{16};
    setParam(controlChanges, MixEngine::kParamConsoleOn, 0.0);
    for (const auto& [id,value] : params)
        setParam(controlChanges,id,value);

    ProcessData control{};
    control.processMode = kRealtime;
    control.symbolicSampleSize = kSample32;
    control.numSamples = kBlockSize;
    control.inputParameterChanges = &controlChanges;
    control.outputParameterChanges = &controlOutput;
    if (processor->processMixControl(&control) != kResultOk) throw 33;

    std::array<float,kBlockSize> inL{}, inR{}, outL{}, outR{};
    inL[0] = 1.0f;
    inR[0] = 1.0f;
    float* inPtrs[2]{inL.data(),inR.data()};
    float* outPtrs[2]{outL.data(),outR.data()};
    AudioBusBuffers inBus{}, outBus{};
    inBus.numChannels = 2; inBus.channelBuffers32 = inPtrs;
    outBus.numChannels = 2; outBus.channelBuffers32 = outPtrs;

    ProcessData channel{};
    channel.processMode = kRealtime;
    channel.symbolicSampleSize = kSample32;
    channel.numSamples = kBlockSize;
    channel.numInputs = 1;
    channel.numOutputs = 1;
    channel.inputs = &inBus;
    channel.outputs = &outBus;
    if (processor->processMixChannel(0,&channel) != kResultOk) throw 34;

    int peakIndex=0;
    double peak=0.0;
    for(int i=0;i<kBlockSize;++i){
        const double a=std::abs(static_cast<double>(outL[static_cast<std::size_t>(i)]));
        if(a>peak){peak=a;peakIndex=i;}
    }
    if(!std::isfinite(peak) || peak<minPeak) throw 35;
    return peakIndex;
}

bool expectReported(const char* name,const std::vector<std::pair<ParamID,double>>& params,double minPeak=0.99) {
    const int peak=impulsePeak(params,minPeak);
    const int mixPeak=mixFxImpulsePeak(params,minPeak);
    std::cout << name << ": VST3 peak=" << peak << " MixFX peak=" << mixPeak << " samples\n";
    const int expected=MixEngine::v3ReportedLatencySamples(kSampleRate);
    return peak == expected && mixPeak == expected;
}
}

int main() {
    try {
        if (!expectReported("linear", {})) return 1;
        if (!expectReported("bypass", {{MixEngine::kParamBypass,1.0}})) return 2;

        // Zero-intensity enabled modules stay neutral, but the plugin keeps one
        // fixed V3 host-latency contract that includes the nominal tape transport
        // budget. Bypass and every zero-effect configuration must land on that
        // same reported sample index.
        if (!expectReported("tube-on-amount-zero-high",
                      {{MixEngine::kParamQuality,1.0},
                       {MixEngine::kParamTubeOn,1.0},
                       {MixEngine::kParamTubeAmount,0.0}},0.20)) return 3;

        if (!expectReported("tape-on-amount-zero-high",
                      {{MixEngine::kParamQuality,1.0},
                       {MixEngine::kParamTapeOn,1.0},
                       {MixEngine::kParamTapeAmount,0.0}},0.20)) return 4;

        if (!expectReported("vinyl-on-color-wear-zero-high",
                      {{MixEngine::kParamQuality,1.0},
                       {MixEngine::kParamVinylOn,1.0},
                       {MixEngine::kParamVinylCharacter,0.0},
                       {MixEngine::kParamVinylWear,0.0},
                       {MixEngine::kParamVinylNoise,0.0}})) return 5;

        std::cout << "Processor V3 fixed-latency integration PASS: " << MixEngine::v3ReportedLatencySamples(kSampleRate) << " samples\n";
        return 0;
    } catch (int code) {
        std::cerr << "Processor latency setup FAIL: " << code << "\n";
        return code;
    } catch (...) {
        std::cerr << "Processor latency unknown exception\n";
        return 90;
    }
}
