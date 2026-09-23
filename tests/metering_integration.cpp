#include "../source/processor.h"
#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <exception>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int32 kBlockSize = 480;
constexpr double kSampleRate = 48000.0;

void setParam(ParameterChanges& changes, ParamID id, double value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    if (!queue) throw 10;
    int32 pointIndex = 0;
    if (queue->addPoint(0, value, pointIndex) != kResultTrue) throw 11;
}

bool getLast(ParameterChanges& changes, ParamID id, double& value) {
    for (int32 i = 0; i < changes.getParameterCount(); ++i) {
        auto* queue = changes.getParameterData(i);
        if (!queue || queue->getParameterId() != id || queue->getPointCount() <= 0) continue;
        int32 offset = 0;
        return queue->getPoint(queue->getPointCount() - 1, offset, value) == kResultTrue;
    }
    return false;
}

struct Harness {
    std::unique_ptr<MixEngine::Processor> processor;
    std::array<float, kBlockSize> inL{}, inR{}, outL{}, outR{};
    float* inPtrs[2]{inL.data(), inR.data()};
    float* outPtrs[2]{outL.data(), outR.data()};
    AudioBusBuffers inBus{};
    AudioBusBuffers outBus{};
    ParameterChanges inputChanges{32};
    ParameterChanges outputChanges{16};

    Harness() : processor(std::make_unique<MixEngine::Processor>()) {
        ProcessSetup setup{};
        setup.processMode = kRealtime;
        setup.symbolicSampleSize = kSample32;
        setup.maxSamplesPerBlock = kBlockSize;
        setup.sampleRate = kSampleRate;
        if (processor->setupProcessing(setup) != kResultOk) throw 20;
        if (processor->setProcessing(true) != kResultOk) throw 21;

        inBus.numChannels = 2;
        inBus.channelBuffers32 = inPtrs;
        outBus.numChannels = 2;
        outBus.channelBuffers32 = outPtrs;
    }

    void fill(float left, float right) {
        inL.fill(left);
        inR.fill(right);
        outL.fill(0.f);
        outR.fill(0.f);
    }

    bool process(float left, float right) {
        fill(left, right);
        outputChanges.clearQueue();

        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = kBlockSize;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;
        data.inputParameterChanges = inputChanges.getParameterCount() > 0 ? &inputChanges : nullptr;
        data.outputParameterChanges = &outputChanges;

        const auto result = processor->process(data);
        inputChanges.clearQueue();
        return result == kResultOk;
    }
};

int fail(const char* message, int code) {
    std::cerr << "Metering integration FAIL: " << message << "\n";
    return code;
}
}

int main() {
    try {
        std::cout << "Metering integration START\n";
        Harness h;

        // Isolate the gain staging path: no coloration, +6 dB output gain.
        setParam(h.inputChanges, MixEngine::kParamConsoleOn, 0.0);
        setParam(h.inputChanges, MixEngine::kParamOutput, 0.75);
        setParam(h.inputChanges, MixEngine::kParamMeterSource, 0.0); // INPUT

        double meterL = 0.0, meterR = 0.0;
        for (int i = 0; i < 120; ++i) {
            if (!h.process(0.10f, 0.05f)) return fail("process() failed in INPUT test", 30);
        }
        if (!getLast(h.outputChanges, MixEngine::kParamMeterL, meterL) ||
            !getLast(h.outputChanges, MixEngine::kParamMeterR, meterR))
            return fail("missing INPUT telemetry", 31);

        const double expectedInL = MixEngine::vuNeedleNormalized(0.10, -18.0);
        const double expectedInR = MixEngine::vuNeedleNormalized(0.05, -18.0);
        if (std::abs(meterL - expectedInL) > 0.03) return fail("INPUT left VU is outside tolerance", 32);
        if (std::abs(meterR - expectedInR) > 0.03) return fail("INPUT right VU is outside tolerance", 33);
        if (!(meterL > meterR)) return fail("stereo L/R ordering is wrong", 34);

        const double inputMeterL = meterL;

        // Switch to OUTPUT. +6 dB output gain must produce a clearly higher reading.
        setParam(h.inputChanges, MixEngine::kParamMeterSource, 1.0);
        for (int i = 0; i < 120; ++i) {
            if (!h.process(0.10f, 0.05f)) return fail("process() failed in OUTPUT test", 35);
        }
        if (!getLast(h.outputChanges, MixEngine::kParamMeterL, meterL) ||
            !getLast(h.outputChanges, MixEngine::kParamMeterR, meterR))
            return fail("missing OUTPUT telemetry", 36);

        const double gain6 = std::pow(10.0, 6.0 / 20.0);
        const double expectedOutL = MixEngine::vuNeedleNormalized(0.10 * gain6, -18.0);
        const double expectedOutR = MixEngine::vuNeedleNormalized(0.05 * gain6, -18.0);
        if (std::abs(meterL - expectedOutL) > 0.04) return fail("OUTPUT left VU is outside tolerance", 37);
        if (std::abs(meterR - expectedOutR) > 0.04) return fail("OUTPUT right VU is outside tolerance", 38);
        if (meterL - inputMeterL < 0.20) return fail("INPUT/OUTPUT source switching is not observable", 39);

        // Clip must trigger at >= 0 dBFS on the selected source, independently per channel.
        setParam(h.inputChanges, MixEngine::kParamMeterSource, 0.0);
        if (!h.process(1.0f, 0.5f)) return fail("process() failed in CLIP trigger test", 40);
        double clipL = 0.0, clipR = 0.0;
        if (!getLast(h.outputChanges, MixEngine::kParamClipL, clipL) ||
            !getLast(h.outputChanges, MixEngine::kParamClipR, clipR))
            return fail("missing CLIP telemetry", 41);
        if (clipL < 0.5) return fail("left CLIP did not trigger at 0 dBFS", 42);
        if (clipR >= 0.5) return fail("right CLIP triggered below 0 dBFS", 43);

        // 750 ms hold at 48 kHz with 480-sample blocks = 75 blocks.
        for (int i = 0; i < 74; ++i) {
            if (!h.process(0.01f, 0.01f)) return fail("process() failed during CLIP hold", 44);
        }
        if (!getLast(h.outputChanges, MixEngine::kParamClipL, clipL) || clipL < 0.5)
            return fail("CLIP hold released too early", 45);

        if (!h.process(0.01f, 0.01f)) return fail("process() failed at CLIP release", 46);
        if (!getLast(h.outputChanges, MixEngine::kParamClipL, clipL) || clipL >= 0.5)
            return fail("CLIP hold did not release after about 750 ms", 47);

        // Input silence flags must never be forwarded blindly. The plugin reports
        // fixed latency and can still have delayed/stateful output after input goes silent.
        h.inBus.silenceFlags = 0x3;
        if (!h.process(0.0f, 0.0f)) return fail("process() failed in silence metadata test", 48);
        if (h.outBus.silenceFlags != 0) return fail("output was incorrectly flagged silent", 49);
        h.inBus.silenceFlags = 0;

        std::cout << "Metering integration PASS\n";
        std::cout << "INPUT L=" << inputMeterL << " OUTPUT L=" << meterL << "\n";
        return 0;
    } catch (int code) {
        std::cerr << "Metering integration setup FAIL: " << code << "\n";
        return code;
    } catch (const std::exception& e) {
        std::cerr << "Metering integration exception: " << e.what() << "\n";
        return 90;
    } catch (...) {
        std::cerr << "Metering integration unknown exception\n";
        return 91;
    }
}
