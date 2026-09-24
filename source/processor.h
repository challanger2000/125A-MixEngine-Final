#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginids.h"
#include "oversampling.h"
#include "latency_alignment.h"
#include "tape_v3_model.h"
#include "tube_v3_model.h"
#include "vinyl_v3_model.h"
#include "metering.h"
#include "meter_exchange.h"
#include "stereo_field.h"
#include "public.sdk/source/vst/utility/dataexchange.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace MixEngine {

#ifndef MIXENGINE_CHANNEL_BUILD
namespace PresonusMixFx {
struct IAudioMixProcessor : Steinberg::FUnknown {
    static const Steinberg::TUID iid;
    virtual Steinberg::tresult PLUGIN_API setMixChannelArrangements(Steinberg::Vst::SpeakerArrangement* arrangements, Steinberg::int32 count) = 0;
    virtual Steinberg::tresult PLUGIN_API processMixControl(Steinberg::Vst::ProcessData* data) = 0;
};
struct IAudioMixChannelProcessor : Steinberg::FUnknown {
    static const Steinberg::TUID iid;
    virtual Steinberg::tresult PLUGIN_API processMixChannel(Steinberg::int32 index, Steinberg::Vst::ProcessData* data) = 0;
};
}
#endif

class Processor final : public Steinberg::Vst::AudioEffect
#ifndef MIXENGINE_CHANNEL_BUILD
                        , public PresonusMixFx::IAudioMixProcessor
                        , public PresonusMixFx::IAudioMixChannelProcessor
#endif
{
public:
    Processor();
    ~Processor() SMTG_OVERRIDE = default;
    static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor()); }
    Steinberg::uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return Steinberg::Vst::AudioEffect::addRef(); }
    Steinberg::uint32 PLUGIN_API release() SMTG_OVERRIDE { return Steinberg::Vst::AudioEffect::release(); }
#ifndef MIXENGINE_CHANNEL_BUILD
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setMixChannelArrangements(Steinberg::Vst::SpeakerArrangement* arrangements, Steinberg::int32 count) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API processMixControl(Steinberg::Vst::ProcessData* data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API processMixChannel(Steinberg::int32 index, Steinberg::Vst::ProcessData* data) SMTG_OVERRIDE;
#endif
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API connect(Steinberg::Vst::IConnectionPoint* other) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API disconnect(Steinberg::Vst::IConnectionPoint* other) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns, Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE { return static_cast<Steinberg::uint32>(reportedLatencySamples_); }
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    const Metering& inputMeter() const noexcept { return inputMeter_; }
    const Metering& outputMeter() const noexcept { return outputMeter_; }
private:
    static constexpr int kMaxAudioChannels = 2;
#ifdef MIXENGINE_CHANNEL_BUILD
    // Standard insert instances never need the 128-source Mix FX state banks.
    // Zero-sized std::arrays keep the shared implementation compilable without
    // carrying hundreds of unused oversamplers/states in every Channel instance.
    static constexpr int kMaxMixFxChannels = 0;
#else
    static constexpr int kMaxMixFxChannels = 128;
#endif
    struct ConsoleChannelState { double dcX1=0.0,dcY1=0.0,lowMemory=0.0,transformerMemory=0.0,noiseMemory=0.0; std::uint32_t noiseRng=0; };
    struct TubeChannelState { V3Research::TubeV3State v3{}; };
    struct TapeChannelState { double highMemory=0.0,lowMemory=0.0,wowPhase=0.0,flutterPhase=0.0,previousInput=0.0,hissMemory=0.0,compressionEnvelope=0.0,magneticMemory=0.0; std::uint32_t noiseRng=0; V3Research::TapeV3State v3{}; };
    struct GlueChannelState { double envelope=0.0; };
    struct VinylChannelState { double highMemory=0.0,lowMemory=0.0,stylusMemory=0.0,surfaceMemory=0.0,clickEnvelope=0.0,clickPolarity=1.0; std::uint32_t noiseRng=0; V3Research::VinylV3State v3{}; };
    using StereoChannelState = StereoFieldState;
    void readParameterChanges(Steinberg::Vst::IParameterChanges* changes);
    void syncMixFxTargets();
    void resetConsoleState();
    void publishMeterParameters(Steinberg::Vst::IParameterChanges* changes,
                                double vuL, double vuR, double peakL, double peakR,
                                bool outputSource, Steinberg::int32 numSamples);
    void sendMeterExchange(double vuL, double vuR, double clipL, double clipR,
                           Steinberg::int32 numSamples);
    double processConsoleSample(double x, ConsoleChannelState& state, int sourceIndex, int lane, int mode, double drive, int osFactor, double noiseAmount, double calibrationNorm);
    double processTubeSample(double x, TubeChannelState& state, double typeMorph, double amount, int osFactor);
    double processTapeSample(double x, TapeChannelState& state, int sourceIndex, int lane, double speedMorph, double amount, double stability, int osFactor, double noiseAmount, double calibrationNorm);
    double processGlueGain(double detector, GlueChannelState& state, double amount, double character) const;
    double processVinylSample(double x, VinylChannelState& state, int sourceIndex, int lane, double character, double wear, int osFactor, double noiseAmount, double calibrationNorm);
    double dcBlock(double x, ConsoleChannelState& state);
#ifndef MIXENGINE_CHANNEL_BUILD
    Steinberg::tresult processMixFxChannelInternal(Steinberg::int32 index, Steinberg::Vst::ProcessData& data);
    void prepareMixFxSnapshotBuffers();
    void captureMixFxInputSnapshot(const Steinberg::Vst::ProcessData& data);
    void prepareMixFxConsoleCoupling();
    double mixFxConsoleCoupledCorrection(Steinberg::int32 targetIndex, Steinberg::int32 lane,
                                         Steinberg::int32 sampleIndex, double inputGain,
                                         double calibrationGain) const noexcept;
    double mixFxCrosstalkSource(Steinberg::int32 targetIndex, Steinberg::int32 lane, Steinberg::int32 sampleIndex) const noexcept;
#endif
    std::array<double,kParamCount> params_{};
    // Short character ramps keep stepped selector changes and host automation
    // click-free without changing the existing three-position GUI.
    double tubeTypeMorphState_=0.5;
    double tapeSpeedMorphState_=0.5;
    double tubeCompGainState_=1.0;
    std::array<double,kMaxMixFxChannels> mixFxTubeTypeMorphState_{};
    std::array<double,kMaxMixFxChannels> mixFxTapeSpeedMorphState_{};
    std::array<double,kMaxMixFxChannels> mixFxTubeCompGainState_{};
    std::array<ConsoleChannelState,kMaxAudioChannels> consoleState_{};
    std::array<TubeChannelState,kMaxAudioChannels> tubeState_{};
    std::array<std::array<ConsoleChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxConsoleState_{};
    std::array<std::array<TubeChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxTubeState_{};
    std::array<TapeChannelState,kMaxAudioChannels> tapeState_{};
    std::array<std::array<TapeChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxTapeState_{};
    std::array<GlueChannelState,kMaxAudioChannels> glueState_{};
    std::array<std::array<GlueChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxGlueState_{};
    std::array<VinylChannelState,kMaxAudioChannels> vinylState_{};
    std::array<std::array<VinylChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxVinylState_{};
    std::array<StereoChannelState,kMaxAudioChannels> stereoState_{};
    std::array<std::array<StereoChannelState,kMaxAudioChannels>,kMaxMixFxChannels> mixFxStereoState_{};
    std::array<OversamplingEngine,kMaxAudioChannels> consoleOversampling_{};
    std::array<std::array<OversamplingEngine,kMaxAudioChannels>,kMaxMixFxChannels> mixFxConsoleOversampling_{};
    std::array<int,kMaxAudioChannels> consoleOversamplingFactor_{{1,1}};
    std::array<std::array<int,kMaxAudioChannels>,kMaxMixFxChannels> mixFxConsoleOversamplingFactor_{};
    std::array<OversamplingEngine,kMaxAudioChannels> nonlinearOversampling_{};
    std::array<std::array<OversamplingEngine,kMaxAudioChannels>,kMaxMixFxChannels> mixFxNonlinearOversampling_{};
    std::array<int,kMaxAudioChannels> nonlinearOversamplingFactor_{{1,1}};
    std::array<std::array<int,kMaxAudioChannels>,kMaxMixFxChannels> mixFxNonlinearOversamplingFactor_{};
    std::array<OversamplingEngine,kMaxAudioChannels> tapeOversampling_{};
    std::array<std::array<OversamplingEngine,kMaxAudioChannels>,kMaxMixFxChannels> mixFxTapeOversampling_{};
    std::array<int,kMaxAudioChannels> tapeOversamplingFactor_{{1,1}};
    std::array<std::array<int,kMaxAudioChannels>,kMaxMixFxChannels> mixFxTapeOversamplingFactor_{};
    std::array<LatencyAligner,kMaxAudioChannels> tapeDryAligner_{};
    std::array<std::array<LatencyAligner,kMaxAudioChannels>,kMaxMixFxChannels> mixFxTapeDryAligner_{};
    std::array<OversamplingEngine,kMaxAudioChannels> vinylOversampling_{};
    std::array<std::array<OversamplingEngine,kMaxAudioChannels>,kMaxMixFxChannels> mixFxVinylOversampling_{};
    std::array<int,kMaxAudioChannels> vinylOversamplingFactor_{{1,1}};
    std::array<std::array<int,kMaxAudioChannels>,kMaxMixFxChannels> mixFxVinylOversamplingFactor_{};
    std::array<LatencyAligner,kMaxAudioChannels> vinylDryAligner_{};
    std::array<std::array<LatencyAligner,kMaxAudioChannels>,kMaxMixFxChannels> mixFxVinylDryAligner_{};
    std::array<LatencyAligner,kMaxAudioChannels> latencyAligner_{};
    std::array<std::array<LatencyAligner,kMaxAudioChannels>,kMaxMixFxChannels> mixFxLatencyAligner_{};
    Metering inputMeter_{};
    Metering outputMeter_{};
    std::array<Metering,kMaxMixFxChannels> mixFxInputMeters_{};
    std::array<Metering,kMaxMixFxChannels> mixFxOutputMeters_{};
#ifndef MIXENGINE_CHANNEL_BUILD
    std::array<std::array<std::vector<double>,kMaxAudioChannels>,kMaxMixFxChannels> mixFxInputSnapshot_{};
    std::array<Steinberg::int32,kMaxMixFxChannels> mixFxSnapshotChannels_{};
    Steinberg::int32 mixFxSnapshotCapacity_=0;
    std::atomic<Steinberg::int32> mixFxSnapshotSamples_{0};
    std::array<std::vector<double>,kMaxAudioChannels> mixFxConsoleCorrection_{};
    std::array<std::vector<double>,kMaxAudioChannels> mixFxConsoleAbsSum_{};
    std::atomic<Steinberg::int32> mixFxConsoleCouplingSamples_{0};

    struct MixFxAutomationPoint {
        Steinberg::int32 offset=0;
        double value=0.0;
    };
    std::array<std::vector<MixFxAutomationPoint>,kParamCount> mixFxAutomation_{};
    std::array<double,kParamCount> mixFxBlockStartParams_{};
    Steinberg::int32 mixFxAutomationSamples_=0;
#endif
    Steinberg::Vst::DataExchangeHandler meterExchange_;
    int meterExchangeCountdown_=0;
    std::atomic<double> mixFxBypass_{0.0},mixFxInput_{0.5},mixFxCalibration_{0.5},mixFxAutoGain_{1.0},mixFxConsoleOn_{1.0},mixFxConsoleMode_{1.0/3.0},mixFxConsoleDrive_{0.25},mixFxCrosstalk_{0.10},mixFxConsoleNoise_{0.0},mixFxTubeOn_{0.0},mixFxTubeAmount_{0.20},mixFxTubeType_{0.5},mixFxTapeOn_{0.0},mixFxTapeAmount_{0.20},mixFxTapeSpeed_{0.5},mixFxTapeStability_{0.90},mixFxTapeHiss_{0.0},mixFxGlueOn_{0.0},mixFxGlueAmount_{0.15},mixFxGlueCharacter_{0.50},mixFxVinylOn_{0.0},mixFxVinylCharacter_{0.25},mixFxVinylWear_{0.0},mixFxVinylNoise_{0.0},mixFxDepth_{0.5},mixFxWidth_{0.5},mixFxLowMono_{0.0},mixFxQuality_{0.5},mixFxOutput_{0.5},mixFxMeterSource_{1.0};
    double sampleRate_=44100.0,dcCoeff_=0.995,lowCoeff_=0.0;
    int reportedLatencySamples_=kFixedLatencySamples;
    int clipHoldSamplesL_=0,clipHoldSamplesR_=0;
    bool lastMeterOutput_=true;
    bool processing_=false,mixFxEngaged_=false;
    Steinberg::int32 mixFxChannelCount_=0;
};

} // namespace MixEngine
