#include "processor.h"
#include "console_oversampling_live.h"
#include "media_oversampling_live.h"
#include "nonlinear_cores.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace MixEngine {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaults[kParamCount] = {
    0.0, 0.5, 0.0, 1.0,
    1.0, 1.0 / 3.0, 0.25, 0.10,
    0.0, 0.20,
    0.0, 0.20, 0.5, 0.9,
    0.0, 0.15, 0.5,
    0.0, 0.25, 0.0,
    0.5, 0.5, 0.0,
    0.0, 0.5, 0.5,
    0.5,
    1.0,
    0.0, 0.0
};

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double gain) { return 20.0 * std::log10(std::max(gain, 1.0e-12)); }
inline double calibrationReferenceDb(double normalized) {
    if (normalized < 0.25) return -18.0;
    if (normalized < 0.75) return -14.0;
    return -10.0;
}
inline int qualityFactor(double normalized) {
    if (normalized < 0.25) return 1;
    if (normalized < 0.75) return 2;
    return 4;
}
inline double consoleAutoGain(int mode, double drive) {
    const double d = std::clamp(drive, 0.0, 1.0);
    if (d <= 0.0) return 1.0;

    // V2 measured compensation at the -18 dBFS = 0 VU operating reference.
    // Each console mode has its own fitted loss curve because the transfer
    // families are intentionally different.
    double linear = 0.76044803;
    double quadratic = 0.83174813;
    switch (std::clamp(mode, 0, 3)) {
        case 0: linear = 0.43726244; quadratic = 0.17564619; break;
        case 1: linear = 0.83954979; quadratic = 1.17466297; break;
        case 2: linear = 2.68284178; quadratic = 1.55514890; break;
        default: break;
    }
    const double compensationDb =
        linear * d + quadratic * d * d;
    return dbToGain(compensationDb);
}

inline double tubeAutoGain(int type, double amount) {
    const double a = std::clamp(amount, 0.0, 1.0);

    // Measured V2 fits include the intentional Amount=0 base colour.
    double base = 0.08117400;
    double linear = 2.17855318;
    double quadratic = 1.71084594;
    switch (std::clamp(type, 0, 2)) {
        case kTube12AU7:
            base = 0.07801063; linear = 1.98058655; quadratic = 0.95506537;
            break;
        case kTube12AT7:
            break;
        default:
            base = 0.08573736; linear = 2.43663914; quadratic = 2.53950286;
            break;
    }
    const double compensationDb =
        base + linear * a + quadratic * a * a;
    return dbToGain(compensationDb);
}

inline double tapeAutoGain(int speed, double amount) {
    const double a = std::clamp(amount, 0.0, 1.0);

    // Tape loss is strongly speed-dependent in V2, so a single Amount-only
    // compensation is no longer adequate. Cubic fits stay within a few
    // hundredths of a dB over the measured 0/25/50/75/100% grid.
    double base = 0.10391574;
    double p1 = 4.23620619;
    double p2 = 2.15866514;
    double p3 = 2.18081067;
    switch (std::clamp(speed, 0, 2)) {
        case 0:
            base = 0.11247080; p1 = 4.72967333; p2 = 1.82382400; p3 = 2.75741867;
            break;
        case 1:
            break;
        default:
            base = 0.09658058; p1 = 3.71617512; p2 = 2.37957131; p3 = 1.58044747;
            break;
    }
    const double compensationDb =
        base + p1 * a + p2 * a * a + p3 * a * a * a;
    return dbToGain(compensationDb);
}

inline double glueAutoGain(double amount, double character) {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0) return 1.0;
    const double c = std::clamp(character, 0.0, 1.0);

    // RESPONSE changes both the approximately linear and quadratic components
    // of gain reduction. These coefficients are fitted jointly across
    // RESPONSE 0/50/100 and AMOUNT 25/50/75/100.
    const double linear = 0.79559015 + 1.18225346 * c;
    const double quadratic = 5.29655910 + 0.37289729 * c;
    const double compensationDb =
        linear * a + quadratic * a * a;
    return dbToGain(compensationDb);
}

inline double vinylAutoGain(double character, double wear) {
    const double c = std::clamp(character, 0.0, 1.0);
    const double w = std::clamp(wear, 0.0, 1.0);
    if (c <= 0.0 && w <= 0.0) return 1.0;

    // Joint V2 fit over Color/Wear grid. The cross-term matters because worn
    // groove loss rises more strongly when Color is already driving the
    // tracing path.
    const double compensationDb =
        1.41064667 * c +
        0.05342071 * c * c +
        0.82946898 * w +
        0.27091501 * w * w +
        0.65669402 * c * w;
    return dbToGain(compensationDb);
}

inline double stableVariation(int sourceIndex,int lane){std::uint32_t x=0x9E3779B9u*static_cast<std::uint32_t>(sourceIndex+1);x^=0x7F4A7C15u*static_cast<std::uint32_t>(lane+1);x^=x>>16;x*=0x7FEB352Du;x^=x>>15;x*=0x846CA68Bu;x^=x>>16;return(static_cast<double>(x&0xFFFFu)/32767.5)-1.0;}
inline std::uint32_t makeNoiseSeed(int sourceIndex,int lane,std::uint32_t salt){std::uint32_t x=salt;x^=0x9E3779B9u*static_cast<std::uint32_t>(sourceIndex+1);x^=0x85EBCA6Bu*static_cast<std::uint32_t>(lane+1);x^=x>>16;x*=0x7FEB352Du;x^=x>>15;x*=0x846CA68Bu;x^=x>>16;return x?x:0xA341316Cu;}
inline double randomBipolar(std::uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return(static_cast<double>(state)/2147483647.5)-1.0;}
inline void addOutputPoint(IParameterChanges* changes,ParamID id,double value,int32 sampleOffset){
    if(!changes)return;
    int32 queueIndex=0;
    if(auto* queue=changes->addParameterData(id,queueIndex)){
        int32 pointIndex=0;
        queue->addPoint(std::max<int32>(0,sampleOffset),std::clamp(value,0.0,1.0),pointIndex);
    }
}
}

#ifndef MIXENGINE_CHANNEL_BUILD
const TUID PresonusMixFx::IAudioMixProcessor::iid={char(0x4C),char(0x05),char(0xC9),char(0x5A),char(0xE1),char(0xFC),char(0xF0),char(0x4F),char(0xAF),char(0x37),char(0x1A),char(0xD9),char(0xA6),char(0x88),char(0x73),char(0x21)};
const TUID PresonusMixFx::IAudioMixChannelProcessor::iid={char(0xB1),char(0x17),char(0x05),char(0x30),char(0x61),char(0x81),char(0xC2),char(0x47),char(0x8D),char(0x58),char(0x73),char(0x2B),char(0x8E),char(0xEE),char(0x72),char(0xC9)};
#endif

Processor::Processor()
: meterExchange_(this,[](auto& config,const auto&){
    config.blockSize=sizeof(MeterExchangeData);
    config.numBlocks=16;
    config.alignment=alignof(MeterExchangeData);
    config.userContextID=kMeterExchangeContext;
    return true;
})
{
#ifdef MIXENGINE_CHANNEL_BUILD
    setControllerClass(kChannelControllerUID);
#else
    setControllerClass(kControllerUID);
#endif
    std::copy(std::begin(kDefaults),std::end(kDefaults),params_.begin());
    syncMixFxTargets();
}
#ifndef MIXENGINE_CHANNEL_BUILD
tresult PLUGIN_API Processor::queryInterface(const TUID iid,void** obj){if(!obj)return kInvalidArgument;if(std::memcmp(iid,PresonusMixFx::IAudioMixProcessor::iid,16)==0){*obj=static_cast<PresonusMixFx::IAudioMixProcessor*>(this);AudioEffect::addRef();return kResultOk;}if(std::memcmp(iid,PresonusMixFx::IAudioMixChannelProcessor::iid,16)==0){*obj=static_cast<PresonusMixFx::IAudioMixChannelProcessor*>(this);AudioEffect::addRef();return kResultOk;}return AudioEffect::queryInterface(iid,obj);}
#endif
tresult PLUGIN_API Processor::initialize(FUnknown* context){auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;addAudioInput(STR16("Mix Input"),SpeakerArr::kStereo);addAudioOutput(STR16("Mix Output"),SpeakerArr::kStereo);return kResultOk;}
tresult PLUGIN_API Processor::connect(IConnectionPoint* other){auto result=AudioEffect::connect(other);meterExchange_.onConnect(other,getHostContext());return result;}
tresult PLUGIN_API Processor::disconnect(IConnectionPoint* other){meterExchange_.onDisconnect(other);return AudioEffect::disconnect(other);}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs,int32 numIns,SpeakerArrangement* outputs,int32 numOuts){if(numIns!=1||numOuts!=1||!inputs||!outputs)return kResultFalse;if(inputs[0]!=SpeakerArr::kMono&&inputs[0]!=SpeakerArr::kStereo)return kResultFalse;if(outputs[0]!=inputs[0])return kResultFalse;return AudioEffect::setBusArrangements(inputs,numIns,outputs,numOuts);}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 s){return s==kSample32||s==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup){sampleRate_=setup.sampleRate>0.0?setup.sampleRate:44100.0;dcCoeff_=std::exp(-2.0*kPi*8.0/sampleRate_);lowCoeff_=1.0-std::exp(-2.0*kPi*180.0/sampleRate_);inputMeter_.prepare(sampleRate_);outputMeter_.prepare(sampleRate_);for(auto&m:mixFxInputMeters_)m.prepare(sampleRate_);for(auto&m:mixFxOutputMeters_)m.prepare(sampleRate_);
#ifndef MIXENGINE_CHANNEL_BUILD
 mixFxSnapshotCapacity_=std::max<int32>(1,setup.maxSamplesPerBlock);prepareMixFxSnapshotBuffers();
#endif
 resetConsoleState();return AudioEffect::setupProcessing(setup);}
tresult PLUGIN_API Processor::setActive(TBool state){
 if(state){resetConsoleState();meterExchange_.onActivate(processSetup);}
 else{processing_=false;meterExchange_.onDeactivate();}
 return AudioEffect::setActive(state);
}
tresult PLUGIN_API Processor::setProcessing(TBool state){const bool p=state!=0;if(p&&!processing_)resetConsoleState();processing_=p;return kResultOk;}

void Processor::resetConsoleState(){
 for(auto&s:consoleState_)s={};for(auto&source:mixFxConsoleState_)for(auto&s:source)s={};for(auto&s:tubeState_)s.reset();for(auto&source:mixFxTubeState_)for(auto&s:source)s.reset();for(auto&s:tapeState_)s={};for(auto&source:mixFxTapeState_)for(auto&s:source)s={};for(auto&s:glueState_)s={};for(auto&source:mixFxGlueState_)for(auto&s:source)s={};for(auto&s:vinylState_)s={};for(auto&source:mixFxVinylState_)for(auto&s:source)s={};for(auto&s:stereoState_)s={};for(auto&source:mixFxStereoState_)for(auto&s:source)s={};
 for(auto&s:consoleOversampling_)s.reset();for(auto&source:mixFxConsoleOversampling_)for(auto&s:source)s.reset();
 consoleOversamplingFactor_.fill(1);for(auto&source:mixFxConsoleOversamplingFactor_)source.fill(1);
 for(auto&s:nonlinearOversampling_)s.reset();for(auto&source:mixFxNonlinearOversampling_)for(auto&s:source)s.reset();
 nonlinearOversamplingFactor_.fill(1);for(auto&source:mixFxNonlinearOversamplingFactor_)source.fill(1);
 for(auto&s:tapeOversampling_)s.reset();for(auto&source:mixFxTapeOversampling_)for(auto&s:source)s.reset();
 tapeOversamplingFactor_.fill(1);for(auto&source:mixFxTapeOversamplingFactor_)source.fill(1);
 for(auto&s:vinylOversampling_)s.reset();for(auto&source:mixFxVinylOversampling_)for(auto&s:source)s.reset();
 vinylOversamplingFactor_.fill(1);for(auto&source:mixFxVinylOversamplingFactor_)source.fill(1);
 for(auto&s:tapeDryAligner_)s.reset();for(auto&source:mixFxTapeDryAligner_)for(auto&s:source)s.reset();
 for(auto&s:vinylDryAligner_)s.reset();for(auto&source:mixFxVinylDryAligner_)for(auto&s:source)s.reset();
 for(auto&s:latencyAligner_)s.reset();for(auto&source:mixFxLatencyAligner_)for(auto&s:source)s.reset();
 inputMeter_.reset();outputMeter_.reset();for(auto&m:mixFxInputMeters_)m.reset();for(auto&m:mixFxOutputMeters_)m.reset();
 clipHoldSamplesL_=clipHoldSamplesR_=0;lastMeterOutput_=true;meterExchangeCountdown_=0;
#ifndef MIXENGINE_CHANNEL_BUILD
 mixFxSnapshotSamples_.store(0,std::memory_order_relaxed);mixFxSnapshotChannels_.fill(0);
#endif
}
void Processor::syncMixFxTargets(){
 mixFxBypass_.store(params_[kParamBypass],std::memory_order_relaxed);mixFxInput_.store(params_[kParamInput],std::memory_order_relaxed);mixFxCalibration_.store(params_[kParamCalibration],std::memory_order_relaxed);mixFxAutoGain_.store(params_[kParamAutoGain],std::memory_order_relaxed);mixFxConsoleOn_.store(params_[kParamConsoleOn],std::memory_order_relaxed);mixFxConsoleMode_.store(params_[kParamConsoleMode],std::memory_order_relaxed);mixFxConsoleDrive_.store(params_[kParamConsoleDrive],std::memory_order_relaxed);mixFxCrosstalk_.store(params_[kParamConsoleCrosstalk],std::memory_order_relaxed);mixFxTubeOn_.store(params_[kParamTubeOn],std::memory_order_relaxed);mixFxTubeAmount_.store(params_[kParamTubeAmount],std::memory_order_relaxed);mixFxTubeType_.store(params_[kParamTubeType],std::memory_order_relaxed);mixFxTapeOn_.store(params_[kParamTapeOn],std::memory_order_relaxed);mixFxTapeAmount_.store(params_[kParamTapeAmount],std::memory_order_relaxed);mixFxTapeSpeed_.store(params_[kParamTapeSpeed],std::memory_order_relaxed);mixFxTapeStability_.store(params_[kParamTapeStability],std::memory_order_relaxed);mixFxGlueOn_.store(params_[kParamGlueOn],std::memory_order_relaxed);mixFxGlueAmount_.store(params_[kParamGlueAmount],std::memory_order_relaxed);mixFxGlueCharacter_.store(params_[kParamGlueCharacter],std::memory_order_relaxed);mixFxVinylOn_.store(params_[kParamVinylOn],std::memory_order_relaxed);mixFxVinylCharacter_.store(params_[kParamVinylCharacter],std::memory_order_relaxed);mixFxVinylWear_.store(params_[kParamVinylWear],std::memory_order_relaxed);mixFxDepth_.store(params_[kParamDepth],std::memory_order_relaxed);mixFxWidth_.store(params_[kParamWidth],std::memory_order_relaxed);mixFxLowMono_.store(params_[kParamLowMono],std::memory_order_relaxed);mixFxConsoleNoise_.store(params_[kParamConsoleNoise],std::memory_order_relaxed);mixFxTapeHiss_.store(params_[kParamTapeHiss],std::memory_order_relaxed);mixFxVinylNoise_.store(params_[kParamVinylNoise],std::memory_order_relaxed);mixFxQuality_.store(params_[kParamQuality],std::memory_order_relaxed);mixFxOutput_.store(params_[kParamOutput],std::memory_order_relaxed);mixFxMeterSource_.store(params_[kParamMeterSource],std::memory_order_relaxed);
}
void Processor::sendMeterExchange(double vuL,double vuR,double clipL,double clipR,int32 numSamples){
 meterExchangeCountdown_-=std::max<int32>(1,numSamples);
 if(meterExchangeCountdown_>0)return;
 meterExchangeCountdown_=std::max(1,static_cast<int>(std::lround(sampleRate_/30.0)));
 auto block=meterExchange_.getCurrentOrNewBlock();
 if(block.blockID==InvalidDataExchangeBlockID)return;
 if(!block.data||block.size<sizeof(MeterExchangeData)){meterExchange_.discardCurrentBlock();return;}
 auto* payload=static_cast<MeterExchangeData*>(block.data);
 payload->vuL=std::clamp(vuL,0.0,1.0);
 payload->vuR=std::clamp(vuR,0.0,1.0);
 payload->clipL=std::clamp(clipL,0.0,1.0);
 payload->clipR=std::clamp(clipR,0.0,1.0);
 meterExchange_.sendCurrentBlock();
}

void Processor::publishMeterParameters(IParameterChanges* changes,double vuL,double vuR,double peakL,double peakR,bool outputSource,int32 numSamples){
 if(outputSource!=lastMeterOutput_){clipHoldSamplesL_=clipHoldSamplesR_=0;lastMeterOutput_=outputSource;}
 const double refDb=calibrationReferenceDb(mixFxEngaged_?mixFxCalibration_.load(std::memory_order_relaxed):params_[kParamCalibration]);
 const int hold=std::max(1,static_cast<int>(std::lround(sampleRate_*0.75)));const int step=std::max<int32>(1,numSamples);
 if(peakL>=1.0)clipHoldSamplesL_=hold;else clipHoldSamplesL_=std::max(0,clipHoldSamplesL_-step);
 if(peakR>=1.0)clipHoldSamplesR_=hold;else clipHoldSamplesR_=std::max(0,clipHoldSamplesR_-step);
 const double normL=vuNeedleNormalized(vuL,refDb),normR=vuNeedleNormalized(vuR,refDb);
 const double clipL=clipHoldSamplesL_>0?1.0:0.0,clipR=clipHoldSamplesR_>0?1.0:0.0;
 const int32 offset=std::max<int32>(0,numSamples-1);
 addOutputPoint(changes,kParamMeterL,normL,offset);addOutputPoint(changes,kParamMeterR,normR,offset);addOutputPoint(changes,kParamClipL,clipL,offset);addOutputPoint(changes,kParamClipR,clipR,offset);
 // Keep standard outputParameterChanges as a host-native path, but also send the same
 // normalized values through VST3 Data Exchange for Mix FX hosts that do not reflect
 // custom processMixControl output queues back into the edit controller.
 sendMeterExchange(normL,normR,clipL,clipR,numSamples);
}
double Processor::dcBlock(double x,ConsoleChannelState& state){const double y=x-state.dcX1+dcCoeff_*state.dcY1;state.dcX1=x;state.dcY1=y;return y;}
double Processor::processConsoleSample(double x,
                                      ConsoleChannelState& state,
                                      int sourceIndex,
                                      int lane,
                                      int mode,
                                      double drive) {
    const double d = std::clamp(drive, 0.0, 1.0);
    const double variation = stableVariation(sourceIndex, lane);
    const double tolerance = 1.0 + 0.006 * variation;
    x *= tolerance;

    state.lowMemory += lowCoeff_ * (x - state.lowMemory);
    const double low = state.lowMemory;
    const double high = x - low;

    // Slow level and signed-energy memories model a moving analogue operating
    // point. Each mode gets a different amount of this behaviour, while DRIVE
    // 0 remains neutral.
    const double level = std::abs(x);
    const double envAttack =
        std::exp(-1.0 / (0.001 * 3.0 * sampleRate_));
    const double envRelease =
        std::exp(-1.0 / (0.001 * 130.0 * sampleRate_));
    const double envCoeff =
        level > state.envelope ? envAttack : envRelease;
    state.envelope =
        envCoeff * state.envelope +
        (1.0 - envCoeff) * level;

    const double signedEnergy =
        x * std::abs(x) / (1.0 + 0.85 * x * x);
    const double biasCoeff =
        std::exp(-1.0 / (0.001 * 48.0 * sampleRate_));
    state.biasMemory =
        biasCoeff * state.biasMemory +
        (1.0 - biasCoeff) * signedEnergy;

    double modeMemory = 0.0;
    switch (std::clamp(mode, 0, 3)) {
        case 0: modeMemory = 0.006; break;
        case 1: modeMemory = 0.015; break;
        case 2: modeMemory = 0.026; break;
        default: modeMemory = 0.010; break;
    }

    const double staticToleranceBias =
        0.0008 * variation * d;
    const double dynamicBias =
        modeMemory * d * state.biasMemory *
        (1.0 + 0.30 * std::clamp(state.envelope, 0.0, 1.5));
    x += staticToleranceBias + dynamicBias;

    OversamplingEngine* engine = nullptr;
    int* currentFactor = nullptr;
    const int factor = qualityFactor(
        mixFxEngaged_
            ? mixFxQuality_.load(std::memory_order_relaxed)
            : params_[kParamQuality]);

    if (mixFxEngaged_) {
        engine =
            &mixFxConsoleOversampling_[static_cast<std::size_t>(sourceIndex)]
                                      [static_cast<std::size_t>(lane)];
        currentFactor =
            &mixFxConsoleOversamplingFactor_[static_cast<std::size_t>(sourceIndex)]
                                           [static_cast<std::size_t>(lane)];
    } else {
        engine = &consoleOversampling_[static_cast<std::size_t>(lane)];
        currentFactor =
            &consoleOversamplingFactor_[static_cast<std::size_t>(lane)];
    }

    double y = processConsoleOversampledCore(
        *engine, *currentFactor, factor, x, low, high, mode, d);
    y = dcBlock(y, state);

    const double noiseAmount = std::clamp(
        mixFxEngaged_
            ? mixFxConsoleNoise_.load(std::memory_order_relaxed)
            : params_[kParamConsoleNoise],
        0.0, 1.0);
    if (noiseAmount > 0.0) {
        if (state.noiseRng == 0u)
            state.noiseRng =
                makeNoiseSeed(sourceIndex, lane, 0xC01150E1u);
        const double white = randomBipolar(state.noiseRng);
        const double coeff =
            1.0 - std::exp(-2.0 * kPi * 6500.0 / sampleRate_);
        state.noiseMemory +=
            coeff * (white - state.noiseMemory);
        const double colored =
            0.70 * white + 0.30 * state.noiseMemory;
        const double n = noiseAmount * noiseAmount;
        const double sourceScale =
            (mixFxEngaged_ && mixFxChannelCount_ > 1)
                ? 1.0 / std::sqrt(static_cast<double>(mixFxChannelCount_))
                : 1.0;
        const double calibrationNorm = dbToGain(-calibrationReferenceDb(
            mixFxEngaged_
                ? mixFxCalibration_.load(std::memory_order_relaxed)
                : params_[kParamCalibration]));
        y += colored * 0.00022 * n *
             sourceScale * calibrationNorm;
    }

    // The mode-specific nonlinear core is already bounded when driven.
    // Preserve exact/high-headroom dry behaviour instead of imposing a hidden
    // post-console hard limiter.
    return y;
}
double Processor::processTubeSample(double x,TubeChannelState& state,int type,double amount,double effectiveSampleRate)const{return processTubeModelV2(x,state,type,amount,effectiveSampleRate);}
double Processor::processTapeSample(double x,
                                    TapeChannelState& state,
                                    int sourceIndex,
                                    int lane,
                                    int speed,
                                    double amount,
                                    double stability) {
    const double a = std::clamp(amount, 0.0, 1.0);
    const double character = analogCharacterAmount(a);
    speed = std::clamp(speed, 0, 2);
    const double instability = 1.0 - std::clamp(stability, 0.0, 1.0);

    // Speed is one coupled operating mode: slower tape saturates sooner,
    // carries a stronger/lower head bump and loses more HF. High-level
    // material increases the HF loss slightly, as expected from tape driven
    // further into its magnetic operating region.
    double cutoff = 17800.0;
    double bumpFreq = 82.0;
    double bumpAmount = 0.060;
    double compressionStrength = 0.92;
    switch (speed) {
        case 0:
            // 7.5 ips: more obvious head bump, earlier HF loss and denser
            // programme compression.
            cutoff = 12500.0;
            bumpFreq = 55.0;
            bumpAmount = 0.115;
            compressionStrength = 1.32;
            break;
        case 1:
            break;
        default:
            // 30 ips: highest headroom/open top, smallest head bump and least
            // programme compression.
            cutoff = 22500.0;
            bumpFreq = 125.0;
            bumpAmount = 0.020;
            compressionStrength = 0.58;
            break;
    }

    // Deterministic transport movement. This remains intentionally subtle; the
    // magnetic and loss model is the main colour mechanism.
    const double wowHz = speed == 0 ? 0.38 : (speed == 1 ? 0.48 : 0.58);
    const double flutterHz = speed == 0 ? 4.7 : (speed == 1 ? 5.8 : 7.0);
    state.wowPhase += 2.0 * kPi * wowHz / sampleRate_;
    state.flutterPhase += 2.0 * kPi * flutterHz / sampleRate_;
    if (state.wowPhase >= 2.0 * kPi) state.wowPhase -= 2.0 * kPi;
    if (state.flutterPhase >= 2.0 * kPi) state.flutterPhase -= 2.0 * kPi;

    const double motion =
        0.78 * std::sin(state.wowPhase) +
        0.22 * std::sin(state.flutterPhase);
    double transport = x;
    if (instability > 0.0) {
        // First-order time-varying allpass as a fractional-delay element.
        // Varying the delay produces real phase/pitch motion without adding an
        // extra whole-sample transport latency to the plugin contract.
        const double sampleScale =
            std::clamp(sampleRate_ / 48000.0, 0.75, 2.0);
        const double fractionalDelay = std::clamp(
            (0.055 + 0.31 * (0.5 + 0.5 * motion)) *
                instability * instability * sampleScale,
            0.0, 0.92);
        const double ap =
            (1.0 - fractionalDelay) /
            (1.0 + fractionalDelay);
        transport = ap * x + state.transportZ;
        state.transportZ = x - ap * transport;
    } else {
        state.transportZ = 0.0;
    }

    // Program-dependent tape compression. Attack and release themselves move
    // slightly with Amount, rather than applying a static post-waveshaper gain.
    const double attackMs = 1.8 + 2.0 * (1.0 - character);
    const double releaseMs = 72.0 + 58.0 * (1.0 - character);
    const double attack = std::exp(-1.0 / (0.001 * attackMs * sampleRate_));
    const double release = std::exp(-1.0 / (0.001 * releaseMs * sampleRate_));
    const double level = std::abs(transport);
    const double envCoeff = level > state.compressionEnvelope ? attack : release;
    state.compressionEnvelope =
        envCoeff * state.compressionEnvelope +
        (1.0 - envCoeff) * level;

    const double threshold = 0.17 + 0.05 * (1.0 - character);
    const double over = std::max(0.0, state.compressionEnvelope - threshold);
    const double dynamicGain =
        1.0 / (1.0 + compressionStrength * character * over);
    const double compressed = transport * dynamicGain;

    OversamplingEngine* osEngine = nullptr;
    int* osCurrentFactor = nullptr;
    const int osFactor = qualityFactor(
        mixFxEngaged_
            ? mixFxQuality_.load(std::memory_order_relaxed)
            : params_[kParamQuality]);

    if (mixFxEngaged_) {
        osEngine = &mixFxTapeOversampling_[static_cast<std::size_t>(sourceIndex)]
                                          [static_cast<std::size_t>(lane)];
        osCurrentFactor =
            &mixFxTapeOversamplingFactor_[static_cast<std::size_t>(sourceIndex)]
                                         [static_cast<std::size_t>(lane)];
    } else {
        osEngine = &tapeOversampling_[static_cast<std::size_t>(lane)];
        osCurrentFactor = &tapeOversamplingFactor_[static_cast<std::size_t>(lane)];
    }

    if (*osCurrentFactor != osFactor) {
        osEngine->reset();
        *osCurrentFactor = osFactor;
        state.magnetic.reset();
    }

    const double magnetic = osEngine->process(
        compressed, osFactor,
        [&](double v) {
            return processTapeMagneticV2(v, state.magnetic, speed, a, sampleRate_ * static_cast<double>(osFactor));
        });

    // Level-dependent HF loss and a broad two-pole head-bump approximation.
    const double hfStress =
        std::clamp(over * (1.3 + 1.2 * character), 0.0, 1.0);
    cutoff *= (1.0 - 0.20 * character * hfStress);
    cutoff = std::clamp(cutoff, 7000.0, sampleRate_ * 0.45);
    const double highCoeff =
        1.0 - std::exp(-2.0 * kPi * cutoff / sampleRate_);
    state.highMemory += highCoeff * (magnetic - state.highMemory);

    const double fastCoeff =
        1.0 - std::exp(-2.0 * kPi * (bumpFreq * 1.85) / sampleRate_);
    const double slowCoeff =
        1.0 - std::exp(-2.0 * kPi * (bumpFreq * 0.52) / sampleRate_);
    state.bumpFast += fastCoeff * (state.highMemory - state.bumpFast);
    state.bumpSlow += slowCoeff * (state.highMemory - state.bumpSlow);
    const double bumpBand = state.bumpFast - state.bumpSlow;

    const double tape =
        state.highMemory + bumpAmount * character * bumpBand;

    const double wet = 0.05 + 0.90 * std::pow(a, 0.86);
    LatencyAligner* dryAligner =
        mixFxEngaged_
            ? &mixFxTapeDryAligner_[static_cast<std::size_t>(sourceIndex)]
                                     [static_cast<std::size_t>(lane)]
            : &tapeDryAligner_[static_cast<std::size_t>(lane)];
    const double dry =
        dryAligner->process(x, oversamplingBulkDelay(osFactor, 1));
    double y = dry + (tape - dry) * wet;

    const double noiseAmount = std::clamp(
        mixFxEngaged_
            ? mixFxTapeHiss_.load(std::memory_order_relaxed)
            : params_[kParamTapeHiss],
        0.0, 1.0);
    if (noiseAmount > 0.0) {
        if (state.noiseRng == 0u)
            state.noiseRng = makeNoiseSeed(sourceIndex, lane, 0x7A9E51A5u);
        const double white = randomBipolar(state.noiseRng);
        const double hissCorner =
            speed == 0 ? 850.0 : (speed == 1 ? 1200.0 : 1750.0);
        const double hissCoeff =
            1.0 - std::exp(-2.0 * kPi * hissCorner / sampleRate_);
        state.hissMemory += hissCoeff * (white - state.hissMemory);
        const double hissTilt =
            speed == 0 ? 0.82 : (speed == 1 ? 0.78 : 0.72);
        const double hiss = white - hissTilt * state.hissMemory;
        const double n = noiseAmount * noiseAmount;
        const double sourceScale =
            (mixFxEngaged_ && mixFxChannelCount_ > 1)
                ? 1.0 / std::sqrt(static_cast<double>(mixFxChannelCount_))
                : 1.0;
        const double calibrationNorm = dbToGain(-calibrationReferenceDb(
            mixFxEngaged_
                ? mixFxCalibration_.load(std::memory_order_relaxed)
                : params_[kParamCalibration]));
        const double speedTone =
            speed == 0 ? 0.82 : (speed == 1 ? 1.0 : 1.10);
        y += hiss * 0.0065 * speedTone * n * sourceScale * calibrationNorm;
    }

    // Keep the processed magnetic branch bounded, but never hard-clip the
    // latency-aligned dry contribution after parallel mixing.
    return y;
}
double Processor::processGlueGain(double detector,
                                  GlueChannelState& state,
                                  double amount,
                                  double character) const {
    const double a = std::clamp(amount, 0.0, 1.0);
    if (a <= 0.0) return 1.0;
    const double c = std::clamp(character, 0.0, 1.0);
    const double level = std::abs(detector);

    // Dual detector: the fast path sees transient crest while the slow path
    // represents programme body. RESPONSE morphs how strongly the transient
    // path influences gain control.
    const double fastAttackMs = 0.55 + 1.65 * (1.0 - c);
    const double fastReleaseMs = 32.0 + 38.0 * (1.0 - c);
    const double slowAttackMs = 7.0 + 11.0 * (1.0 - c);
    const double slowReleaseMs = 230.0 + 190.0 * (1.0 - c);

    const auto follow = [&](double input, double& env,
                            double attackMs, double releaseMs) {
        const double attack =
            std::exp(-1.0 / (0.001 * attackMs * sampleRate_));
        const double release =
            std::exp(-1.0 / (0.001 * releaseMs * sampleRate_));
        const double coeff = input > env ? attack : release;
        env = coeff * env + (1.0 - coeff) * input;
    };

    follow(level, state.fastEnvelope, fastAttackMs, fastReleaseMs);
    follow(level, state.slowEnvelope, slowAttackMs, slowReleaseMs);

    const double crest =
        state.fastEnvelope / std::max(1.0e-8, state.slowEnvelope);
    const double crestCoeff =
        std::exp(-1.0 / (0.001 * 45.0 * sampleRate_));
    state.crestMemory =
        crestCoeff * state.crestMemory +
        (1.0 - crestCoeff) * std::clamp(crest, 0.5, 5.0);

    const double transientWeight =
        std::clamp(0.14 + 0.38 * c +
                   0.08 * (state.crestMemory - 1.0),
                   0.10, 0.68);
    const double controlLevel =
        state.slowEnvelope +
        transientWeight * (state.fastEnvelope - state.slowEnvelope);

    // Keep low-level programme material largely untouched; compression
    // begins around the calibrated operating region and moves downward as
    // AMOUNT rises. This avoids compressing signals far below 0 VU.
    const double thresholdDb = -4.0 - 8.0 * a;
    const double ratio = 1.0 + (1.8 + 1.4 * c) * a;
    const double kneeDb = 10.0 - 4.0 * c;
    const double envDb = gainToDb(controlLevel);
    const double overDb = envDb - thresholdDb;

    double grDb = 0.0;
    if (overDb > kneeDb * 0.5) {
        grDb = overDb - overDb / ratio;
    } else if (overDb > -kneeDb * 0.5) {
        const double p = overDb + kneeDb * 0.5;
        grDb = (1.0 - 1.0 / ratio) * p * p / (2.0 * kneeDb);
    }

    // Gain-control smoothing is separate from the level detector. Release gets
    // longer after deeper gain reduction and shorter for high-crest material,
    // yielding a musical programme-dependent recovery instead of one fixed RC.
    const double desiredGainDb = -grDb;
    const double gainAttackMs = 5.0 + 13.0 * (1.0 - c);
    const double depth = std::clamp(grDb / 10.0, 0.0, 1.0);
    const double crestRelease =
        std::clamp((state.crestMemory - 1.0) / 2.5, 0.0, 1.0);
    const double releaseFastMs = 85.0 + 55.0 * (1.0 - c);
    const double releaseSlowMs = 360.0 + 180.0 * (1.0 - c);
    const double gainReleaseMs =
        releaseFastMs +
        (releaseSlowMs - releaseFastMs) *
            depth * (1.0 - 0.45 * crestRelease);

    const double gainAttack =
        std::exp(-1.0 / (0.001 * gainAttackMs * sampleRate_));
    const double gainRelease =
        std::exp(-1.0 / (0.001 * gainReleaseMs * sampleRate_));
    const double gainCoeff =
        desiredGainDb < state.gainDb ? gainAttack : gainRelease;
    state.gainDb =
        gainCoeff * state.gainDb +
        (1.0 - gainCoeff) * desiredGainDb;

    const double compressedGain = dbToGain(state.gainDb);
    const double wet = 0.12 + 0.78 * std::pow(a, 0.82);
    return 1.0 + (compressedGain - 1.0) * wet;
}
double Processor::processVinylSample(double x,
                                    VinylChannelState& state,
                                    int sourceIndex,
                                    int lane,
                                    double character,
                                    double wear) {
    const double c = std::clamp(character, 0.0, 1.0);
    const double w = std::clamp(wear, 0.0, 1.0);
    double y = x;

    if (c > 0.0 || w > 0.0) {
        // Wear reacts to fast/high-frequency movement rather than acting as a
        // fixed low-pass amount. Strong HF/transient content therefore loses a
        // little more edge on a worn record while quiet/body information is
        // preserved.
        const double derivative = x - state.previousInput;
        state.previousInput = x;
        const double hfActivity = std::abs(derivative);
        const double wearAttack =
            std::exp(-1.0 / (0.001 * 2.2 * sampleRate_));
        const double wearRelease =
            std::exp(-1.0 / (0.001 * 95.0 * sampleRate_));
        const double wearCoeff =
            hfActivity > state.wearEnvelope ? wearAttack : wearRelease;
        state.wearEnvelope =
            wearCoeff * state.wearEnvelope +
            (1.0 - wearCoeff) * hfActivity;

        const double activity =
            std::clamp(state.wearEnvelope * 8.0, 0.0, 1.0);

        double cutoff =
            20500.0 - 2600.0 * c - 7200.0 * w;
        cutoff *= (1.0 - 0.30 * w * activity);
        cutoff = std::clamp(cutoff, 5200.0, sampleRate_ * 0.45);

        const double highCoeff =
            1.0 - std::exp(-2.0 * kPi * cutoff / sampleRate_);
        state.highMemory +=
            highCoeff * (x - state.highMemory);

        const double bodyCoeff =
            1.0 - std::exp(-2.0 * kPi * 240.0 / sampleRate_);
        state.bodyMemory +=
            bodyCoeff * (state.highMemory - state.bodyMemory);

        const double bodyBoost =
            0.018 * c + 0.014 * w;
        double colored =
            state.highMemory + bodyBoost * state.bodyMemory;

        // Worn groove walls soften the fastest excursions before the nonlinear
        // tracing stage. This is level dependent through the wear envelope.
        const double transientSoft =
            std::clamp(0.055 * w * activity, 0.0, 0.055);
        colored -= transientSoft * derivative;

        OversamplingEngine* osEngine = nullptr;
        int* osCurrentFactor = nullptr;
        const int osFactor = qualityFactor(
            mixFxEngaged_
                ? mixFxQuality_.load(std::memory_order_relaxed)
                : params_[kParamQuality]);

        if (mixFxEngaged_) {
            osEngine =
                &mixFxVinylOversampling_[static_cast<std::size_t>(sourceIndex)]
                                        [static_cast<std::size_t>(lane)];
            osCurrentFactor =
                &mixFxVinylOversamplingFactor_[static_cast<std::size_t>(sourceIndex)]
                                             [static_cast<std::size_t>(lane)];
        } else {
            osEngine = &vinylOversampling_[static_cast<std::size_t>(lane)];
            osCurrentFactor =
                &vinylOversamplingFactor_[static_cast<std::size_t>(lane)];
        }

        if (*osCurrentFactor != osFactor) {
            osEngine->reset();
            *osCurrentFactor = osFactor;
        }

        const double shapedRaw = osEngine->process(
            colored, osFactor,
            [&](double v) {
                return processVinylGrooveV2(v, c, w);
            });

        // AC-couple the asymmetric groove stage so H2/tracing colour is
        // preserved while the generated DC component is rejected.
        const double vinylDcCoeff =
            std::exp(-2.0 * kPi * 8.0 / sampleRate_);
        const double shaped =
            shapedRaw - state.dcX1 + vinylDcCoeff * state.dcY1;
        state.dcX1 = shapedRaw;
        state.dcY1 = shaped;

        const double wet =
            std::clamp(0.10 + 0.48 * c + 0.32 * w, 0.0, 0.90);
        LatencyAligner* dryAligner =
            mixFxEngaged_
                ? &mixFxVinylDryAligner_[static_cast<std::size_t>(sourceIndex)]
                                          [static_cast<std::size_t>(lane)]
                : &vinylDryAligner_[static_cast<std::size_t>(lane)];
        const double dry =
            dryAligner->process(x, oversamplingBulkDelay(osFactor, 1));
        y = dry + (shaped - dry) * wet;
    }

    // SURFACE remains independent from Color/Wear. Wear changes the statistical
    // severity of a noisy surface, but Surface=0 is still mathematically silent.
    const double noiseAmount = std::clamp(
        mixFxEngaged_
            ? mixFxVinylNoise_.load(std::memory_order_relaxed)
            : params_[kParamVinylNoise],
        0.0, 1.0);

    if (noiseAmount > 0.0) {
        if (state.noiseRng == 0u)
            state.noiseRng =
                makeNoiseSeed(sourceIndex, lane, 0xB17E4A11u);

        const double white = randomBipolar(state.noiseRng);
        const double surfaceCoeff =
            1.0 - std::exp(-2.0 * kPi * 7600.0 / sampleRate_);
        state.surfaceMemory +=
            surfaceCoeff * (white - state.surfaceMemory);

        const double rumbleCoeff =
            1.0 - std::exp(-2.0 * kPi * 42.0 / sampleRate_);
        state.rumbleMemory +=
            rumbleCoeff * (white - state.rumbleMemory);

        const double surface =
            0.48 * white +
            0.47 * state.surfaceMemory +
            0.05 * state.rumbleMemory;
        const double n = noiseAmount * noiseAmount;
        const double sourceScale =
            (mixFxEngaged_ && mixFxChannelCount_ > 1)
                ? 1.0 / std::sqrt(static_cast<double>(mixFxChannelCount_))
                : 1.0;
        const double calibrationNorm = dbToGain(-calibrationReferenceDb(
            mixFxEngaged_
                ? mixFxCalibration_.load(std::memory_order_relaxed)
                : params_[kParamCalibration]));

        const double surfaceLevel =
            0.0062 * (0.78 + 0.72 * w);
        y += surface * surfaceLevel * n *
             sourceScale * calibrationNorm;

        const double clickRateHz =
            (0.08 + 2.8 * w * w) *
            (0.35 + 0.65 * noiseAmount);
        const double eventProbe =
            0.5 * (randomBipolar(state.noiseRng) + 1.0);
        if (eventProbe < clickRateHz / sampleRate_) {
            const double randomLevel =
                0.5 * (randomBipolar(state.noiseRng) + 1.0);
            const double heavy =
                randomLevel * randomLevel;
            state.clickEnvelope =
                0.010 +
                (0.028 + 0.060 * w) * heavy;
            state.clickPolarity =
                randomBipolar(state.noiseRng) >= 0.0 ? 1.0 : -1.0;
        }

        const double clickDecayMs =
            0.45 + 2.3 * w;
        const double clickDecay =
            std::exp(-1.0 /
                     (0.001 * clickDecayMs * sampleRate_));
        y += state.clickPolarity *
             state.clickEnvelope *
             n * sourceScale * calibrationNorm;
        state.clickEnvelope *= clickDecay;
        if (state.clickEnvelope < 1.0e-10)
            state.clickEnvelope = 0.0;
    }

    // Groove nonlinearity is bounded before the wet/dry mix. Avoid a hidden
    // limiter on the final Vinyl output so Color=0/Wear=0/Surface=0 is exactly
    // transparent even for high peaks.
    return y;
}
void Processor::readParameterChanges(IParameterChanges* changes){if(!changes)return;const int32 count=changes->getParameterCount();for(int32 i=0;i<count;++i){auto*q=changes->getParameterData(i);if(!q)continue;const ParamID id=q->getParameterId();if(id>=kParamCount)continue;const int32 points=q->getPointCount();for(int32 point=0;point<points;++point){int32 offset=0;ParamValue value=0.0;if(q->getPoint(point,offset,value)==kResultTrue)params_[id]=std::clamp(static_cast<double>(value),0.0,1.0);}}}
#ifndef MIXENGINE_CHANNEL_BUILD
void Processor::prepareMixFxSnapshotBuffers(){
 const auto count=std::clamp<int32>(mixFxChannelCount_,0,kMaxMixFxChannels);
 if(mixFxSnapshotCapacity_<=0)return;
 for(int32 channel=0;channel<count;++channel)
  for(int lane=0;lane<kMaxAudioChannels;++lane)
   mixFxInputSnapshot_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(lane)].resize(static_cast<std::size_t>(mixFxSnapshotCapacity_));
}
void Processor::captureMixFxInputSnapshot(const ProcessData& data){
 mixFxSnapshotSamples_.store(0,std::memory_order_release);
 const int32 count=std::min<int32>(std::clamp<int32>(mixFxChannelCount_,0,kMaxMixFxChannels),data.numInputs);
 if(count<=0||!data.inputs||data.numSamples<=0||data.numSamples>mixFxSnapshotCapacity_)return;
 for(int32 channel=0;channel<count;++channel){
  const auto& bus=data.inputs[channel];
  const int32 lanes=std::clamp<int32>(bus.numChannels,0,kMaxAudioChannels);
  mixFxSnapshotChannels_[static_cast<std::size_t>(channel)]=lanes;
  auto& left=mixFxInputSnapshot_[static_cast<std::size_t>(channel)][0];
  auto& right=mixFxInputSnapshot_[static_cast<std::size_t>(channel)][1];
  if(static_cast<int32>(left.size())<data.numSamples||static_cast<int32>(right.size())<data.numSamples){mixFxSnapshotSamples_.store(0,std::memory_order_release);return;}
  if(data.symbolicSampleSize==kSample32&&bus.channelBuffers32&&lanes>0&&bus.channelBuffers32[0]){
   const float* l=bus.channelBuffers32[0];const float* r=lanes>1&&bus.channelBuffers32[1]?bus.channelBuffers32[1]:l;
   for(int32 i=0;i<data.numSamples;++i){left[static_cast<std::size_t>(i)]=l[i];right[static_cast<std::size_t>(i)]=r[i];}
  }else if(data.symbolicSampleSize==kSample64&&bus.channelBuffers64&&lanes>0&&bus.channelBuffers64[0]){
   const double* l=bus.channelBuffers64[0];const double* r=lanes>1&&bus.channelBuffers64[1]?bus.channelBuffers64[1]:l;
   for(int32 i=0;i<data.numSamples;++i){left[static_cast<std::size_t>(i)]=l[i];right[static_cast<std::size_t>(i)]=r[i];}
  }else{
   mixFxSnapshotChannels_[static_cast<std::size_t>(channel)]=0;
   std::fill_n(left.begin(),data.numSamples,0.0);std::fill_n(right.begin(),data.numSamples,0.0);
  }
 }
 for(int32 channel=count;channel<mixFxChannelCount_&&channel<kMaxMixFxChannels;++channel)mixFxSnapshotChannels_[static_cast<std::size_t>(channel)]=0;
 mixFxSnapshotSamples_.store(data.numSamples,std::memory_order_release);
}
double Processor::mixFxCrosstalkSource(int32 targetIndex,int32 lane,int32 sampleIndex)const noexcept{
 const int32 snapshotSamples=mixFxSnapshotSamples_.load(std::memory_order_acquire);
 if(targetIndex<0||targetIndex>=mixFxChannelCount_||lane<0||lane>=kMaxAudioChannels||sampleIndex<0||sampleIndex>=snapshotSamples)return 0.0;
 double sum=0.0;int contributors=0;
 for(const int32 source:{targetIndex-1,targetIndex+1}){
  if(source<0||source>=mixFxChannelCount_)continue;
  if(mixFxSnapshotChannels_[static_cast<std::size_t>(source)]<=0)continue;
  const auto& snap=mixFxInputSnapshot_[static_cast<std::size_t>(source)][static_cast<std::size_t>(lane)];
  if(sampleIndex>=static_cast<int32>(snap.size()))continue;
  sum+=snap[static_cast<std::size_t>(sampleIndex)];++contributors;
 }
 return contributors>0?sum/static_cast<double>(contributors):0.0;
}
tresult PLUGIN_API Processor::setMixChannelArrangements(SpeakerArrangement* arrangements,int32 count){if(count<0||count>kMaxMixFxChannels)return kInvalidArgument;if(count>0&&!arrangements)return kInvalidArgument;for(int32 i=0;i<count;++i){if(arrangements[i]!=SpeakerArr::kMono&&arrangements[i]!=SpeakerArr::kStereo)return kResultFalse;}mixFxEngaged_=true;mixFxChannelCount_=count;prepareMixFxSnapshotBuffers();resetConsoleState();syncMixFxTargets();return kResultOk;}
tresult PLUGIN_API Processor::processMixControl(ProcessData* data){
 if(data)readParameterChanges(data->inputParameterChanges);syncMixFxTargets();if(data)captureMixFxInputSnapshot(*data);
 if(data){
  double inSqL=0.0,inSqR=0.0,outSqL=0.0,outSqR=0.0,inPeakL=0.0,inPeakR=0.0,outPeakL=0.0,outPeakR=0.0;
  const int32 count=std::clamp<int32>(mixFxChannelCount_,0,kMaxMixFxChannels);
  for(int32 i=0;i<count;++i){const auto& in=mixFxInputMeters_[static_cast<std::size_t>(i)];const auto& out=mixFxOutputMeters_[static_cast<std::size_t>(i)];const double ivl=in.vuL(),ivr=in.vuR(),ovl=out.vuL(),ovr=out.vuR();inSqL+=ivl*ivl;inSqR+=ivr*ivr;outSqL+=ovl*ovl;outSqR+=ovr*ovr;inPeakL=std::max(inPeakL,in.peakL());inPeakR=std::max(inPeakR,in.peakR());outPeakL=std::max(outPeakL,out.peakL());outPeakR=std::max(outPeakR,out.peakR());}
  const double inVuL=std::sqrt(inSqL),inVuR=std::sqrt(inSqR),outVuL=std::sqrt(outSqL),outVuR=std::sqrt(outSqR);const bool outputSource=mixFxMeterSource_.load(std::memory_order_relaxed)>=0.5;
  if(outputSource)publishMeterParameters(data->outputParameterChanges,outVuL,outVuR,std::max(outPeakL,outVuL),std::max(outPeakR,outVuR),true,data->numSamples);else publishMeterParameters(data->outputParameterChanges,inVuL,inVuR,std::max(inPeakL,inVuL),std::max(inPeakR,inVuR),false,data->numSamples);
 }
 return kResultOk;
}

tresult Processor::processMixFxChannelInternal(int32 index,ProcessData& data){
 if(index<0||index>=kMaxMixFxChannels)return kInvalidArgument;if(mixFxChannelCount_>0&&index>=mixFxChannelCount_)return kInvalidArgument;if(data.numInputs<1||data.numOutputs<1||data.numSamples<=0)return kResultOk;auto&inBus=data.inputs[0];auto&outBus=data.outputs[0];const int32 channels=std::min<int32>(std::min(inBus.numChannels,outBus.numChannels),kMaxAudioChannels);if(channels<=0)return kResultOk;
 auto& inputMeter=mixFxInputMeters_[static_cast<std::size_t>(index)];auto& outputMeter=mixFxOutputMeters_[static_cast<std::size_t>(index)];inputMeter.beginBlock();outputMeter.beginBlock();
 const bool bypass=mixFxBypass_.load(std::memory_order_relaxed)>=0.5,consoleOn=mixFxConsoleOn_.load(std::memory_order_relaxed)>=0.5,tubeOn=mixFxTubeOn_.load(std::memory_order_relaxed)>=0.5,tapeOn=mixFxTapeOn_.load(std::memory_order_relaxed)>=0.5,glueOn=mixFxGlueOn_.load(std::memory_order_relaxed)>=0.5,vinylOn=mixFxVinylOn_.load(std::memory_order_relaxed)>=0.5,autoGainOn=mixFxAutoGain_.load(std::memory_order_relaxed)>=0.5;
 const double inputGain=dbToGain((mixFxInput_.load(std::memory_order_relaxed)-0.5)*24.0),outputGain=dbToGain((mixFxOutput_.load(std::memory_order_relaxed)-0.5)*24.0),inputMatchGain=autoGainOn?1.0/inputGain:1.0,calibrationDb=calibrationReferenceDb(mixFxCalibration_.load(std::memory_order_relaxed)),calibrationGain=dbToGain(-calibrationDb),calibrationReturn=1.0/calibrationGain,drive=mixFxConsoleDrive_.load(std::memory_order_relaxed);const double crosstalk=std::clamp(mixFxCrosstalk_.load(std::memory_order_relaxed),0.0,1.0)*0.018;const int mode=std::clamp(static_cast<int>(std::lround(mixFxConsoleMode_.load(std::memory_order_relaxed)*3.0)),0,3),tubeType=std::clamp(static_cast<int>(std::lround(mixFxTubeType_.load(std::memory_order_relaxed)*2.0)),0,2),tapeSpeed=std::clamp(static_cast<int>(std::lround(mixFxTapeSpeed_.load(std::memory_order_relaxed)*2.0)),0,2);const double tubeAmount=std::clamp(mixFxTubeAmount_.load(std::memory_order_relaxed),0.0,1.0),tapeAmount=std::clamp(mixFxTapeAmount_.load(std::memory_order_relaxed),0.0,1.0),tapeStability=std::clamp(mixFxTapeStability_.load(std::memory_order_relaxed),0.0,1.0),glueAmount=std::clamp(mixFxGlueAmount_.load(std::memory_order_relaxed),0.0,1.0),glueCharacter=std::clamp(mixFxGlueCharacter_.load(std::memory_order_relaxed),0.0,1.0),vinylCharacter=std::clamp(mixFxVinylCharacter_.load(std::memory_order_relaxed),0.0,1.0),vinylWear=std::clamp(mixFxVinylWear_.load(std::memory_order_relaxed),0.0,1.0),widthGain=2.0*std::clamp(mixFxWidth_.load(std::memory_order_relaxed),0.0,1.0),depthBipolar=(std::clamp(mixFxDepth_.load(std::memory_order_relaxed),0.0,1.0)-0.5)*2.0,lowMono=std::clamp(mixFxLowMono_.load(std::memory_order_relaxed),0.0,1.0);const int osFactor=qualityFactor(mixFxQuality_.load(std::memory_order_relaxed));const int osIslands=bypass?0:(static_cast<int>(consoleOn)+static_cast<int>(tubeOn)+static_cast<int>(tapeOn)+static_cast<int>(vinylOn&&(vinylCharacter>0.0||vinylWear>0.0)));const int latencyDelay=latencyCompensation(osFactor,osIslands);const double autoGain=consoleOn&&autoGainOn?consoleAutoGain(mode,drive):1.0,tubeGain=tubeOn&&autoGainOn?tubeAutoGain(tubeType,tubeAmount):1.0,tapeGain=tapeOn&&autoGainOn?tapeAutoGain(tapeSpeed,tapeAmount):1.0,glueGain=glueOn&&autoGainOn?glueAutoGain(glueAmount,glueCharacter):1.0,vinylGain=vinylOn&&autoGainOn?vinylAutoGain(vinylCharacter,vinylWear):1.0,lowMonoCoeff=stereoOnePoleCoefficient(120.0,sampleRate_),depthCoeff=stereoOnePoleCoefficient(2000.0,sampleRate_),depthGain=stereoDepthGain(depthBipolar),correlationCoeff=stereoOnePoleCoefficient(4.0,sampleRate_);
 auto processFrame=[&](int32 sampleIndex,double leftIn,double rightIn,bool stereo,double&leftOut,double&rightOut){const double meterL=bypass?leftIn:leftIn*inputGain,meterR=bypass?rightIn:rightIn*inputGain;inputMeter.push(meterL,stereo?meterR:meterL);if(bypass){auto&align=mixFxLatencyAligner_[static_cast<std::size_t>(index)];leftOut=align[0].process(leftIn,kFixedLatencySamples);rightOut=stereo?align[1].process(rightIn,kFixedLatencySamples):leftOut;outputMeter.push(leftOut,stereo?rightOut:leftOut);return;}double l=meterL,r=meterR;if(consoleOn){auto&states=mixFxConsoleState_[static_cast<std::size_t>(index)];if(crosstalk>0.0&&mixFxSnapshotSamples_.load(std::memory_order_acquire)==data.numSamples){const double xtCoeff=1.0-std::exp(-2.0*kPi*720.0/sampleRate_);auto shapeCrosstalk=[&](double source,ConsoleChannelState&st){st.crosstalkLowMemory+=xtCoeff*(source-st.crosstalkLowMemory);const double high=source-st.crosstalkLowMemory;return 0.52*st.crosstalkLowMemory+1.08*high;};const double xtL=shapeCrosstalk(mixFxCrosstalkSource(index,0,sampleIndex),states[0]);const double xtR=shapeCrosstalk(mixFxCrosstalkSource(index,1,sampleIndex),states[1]);l+=xtL*inputGain*crosstalk;r+=xtR*inputGain*crosstalk;}l*=calibrationGain;r*=calibrationGain;l=processConsoleSample(l,states[0],index,0,mode,drive);r=stereo?processConsoleSample(r,states[1],index,1,mode,drive):l;l*=calibrationReturn*autoGain;r*=calibrationReturn*autoGain;}if(tubeOn){auto&engines=mixFxNonlinearOversampling_[static_cast<std::size_t>(index)];auto&factors=mixFxNonlinearOversamplingFactor_[static_cast<std::size_t>(index)];auto&tubeStates=mixFxTubeState_[static_cast<std::size_t>(index)];const double tubeSampleRate=sampleRate_*static_cast<double>(osFactor);if(factors[0]!=osFactor){engines[0].reset();factors[0]=osFactor;tubeStates[0].reset();}l=engines[0].process(l*calibrationGain,osFactor,[&](double v){return processTubeSample(v,tubeStates[0],tubeType,tubeAmount,tubeSampleRate);})*calibrationReturn*tubeGain;if(stereo){if(factors[1]!=osFactor){engines[1].reset();factors[1]=osFactor;tubeStates[1].reset();}r=engines[1].process(r*calibrationGain,osFactor,[&](double v){return processTubeSample(v,tubeStates[1],tubeType,tubeAmount,tubeSampleRate);})*calibrationReturn*tubeGain;}else r=l;}if(tapeOn){auto&states=mixFxTapeState_[static_cast<std::size_t>(index)];l=processTapeSample(l*calibrationGain,states[0],index,0,tapeSpeed,tapeAmount,tapeStability)*calibrationReturn*tapeGain;r=stereo?processTapeSample(r*calibrationGain,states[1],index,1,tapeSpeed,tapeAmount,tapeStability)*calibrationReturn*tapeGain:l;}if(glueOn){auto&states=mixFxGlueState_[static_cast<std::size_t>(index)];const double glueL=l*calibrationGain,glueR=r*calibrationGain,detector=stereo?std::max(std::abs(glueL),std::abs(glueR)):std::abs(glueL),linkedGain=processGlueGain(detector,states[0],glueAmount,glueCharacter)*glueGain;l=glueL*linkedGain*calibrationReturn;r=stereo?glueR*linkedGain*calibrationReturn:l;}if(vinylOn){auto&states=mixFxVinylState_[static_cast<std::size_t>(index)];l=processVinylSample(l*calibrationGain,states[0],index,0,vinylCharacter,vinylWear)*calibrationReturn*vinylGain;r=stereo?processVinylSample(r*calibrationGain,states[1],index,1,vinylCharacter,vinylWear)*calibrationReturn*vinylGain:l;}if(stereo){auto&st=mixFxStereoState_[static_cast<std::size_t>(index)][0];processStereoFieldSample(l,r,st,widthGain,lowMono,lowMonoCoeff,depthGain,depthCoeff,correlationCoeff);}auto&align=mixFxLatencyAligner_[static_cast<std::size_t>(index)];leftOut=align[0].process(l*outputGain*inputMatchGain,latencyDelay);rightOut=stereo?align[1].process(r*outputGain*inputMatchGain,latencyDelay):leftOut;outputMeter.push(leftOut,stereo?rightOut:leftOut);};
 if(data.symbolicSampleSize==kSample32){auto*inL=inBus.channelBuffers32[0];auto*outL=outBus.channelBuffers32[0];auto*inR=channels>1?inBus.channelBuffers32[1]:inL;auto*outR=channels>1?outBus.channelBuffers32[1]:outL;if(!inL||!outL)return kResultOk;for(int32 i=0;i<data.numSamples;++i){double l=0,r=0;processFrame(i,inL[i],inR?inR[i]:inL[i],channels>1,l,r);outL[i]=static_cast<float>(l);if(channels>1&&outR)outR[i]=static_cast<float>(r);}}else if(data.symbolicSampleSize==kSample64){auto*inL=inBus.channelBuffers64[0];auto*outL=outBus.channelBuffers64[0];auto*inR=channels>1?inBus.channelBuffers64[1]:inL;auto*outR=channels>1?outBus.channelBuffers64[1]:outL;if(!inL||!outL)return kResultOk;for(int32 i=0;i<data.numSamples;++i){double l=0,r=0;processFrame(i,inL[i],inR?inR[i]:inL[i],channels>1,l,r);outL[i]=l;if(channels>1&&outR)outR[i]=r;}}else return kResultFalse;inputMeter.publish();outputMeter.publish();outBus.silenceFlags=0;return kResultOk;
}
tresult PLUGIN_API Processor::processMixChannel(int32 index,ProcessData* data){if(!data)return kInvalidArgument;return processMixFxChannelInternal(index,*data);}
#endif


tresult PLUGIN_API Processor::process(ProcessData& data){readParameterChanges(data.inputParameterChanges);if(mixFxEngaged_){syncMixFxTargets();return kResultOk;}if(data.numInputs<1||data.numOutputs<1||data.numSamples<=0)return kResultOk;auto&inBus=data.inputs[0];auto&outBus=data.outputs[0];const int32 channels=std::min(inBus.numChannels,outBus.numChannels);inputMeter_.beginBlock();outputMeter_.beginBlock();const bool bypass=params_[kParamBypass]>=0.5,consoleOn=params_[kParamConsoleOn]>=0.5,tubeOn=params_[kParamTubeOn]>=0.5,tapeOn=params_[kParamTapeOn]>=0.5,glueOn=params_[kParamGlueOn]>=0.5,vinylOn=params_[kParamVinylOn]>=0.5,autoGainOn=params_[kParamAutoGain]>=0.5;const double inputGain=dbToGain((params_[kParamInput]-0.5)*24.0),outputGain=dbToGain((params_[kParamOutput]-0.5)*24.0),inputMatchGain=autoGainOn?1.0/inputGain:1.0,calibrationDb=calibrationReferenceDb(params_[kParamCalibration]),calibrationGain=dbToGain(-calibrationDb),calibrationReturn=1.0/calibrationGain,drive=params_[kParamConsoleDrive];const int mode=std::clamp(static_cast<int>(std::lround(params_[kParamConsoleMode]*3.0)),0,3),tubeType=std::clamp(static_cast<int>(std::lround(params_[kParamTubeType]*2.0)),0,2),tapeSpeed=std::clamp(static_cast<int>(std::lround(params_[kParamTapeSpeed]*2.0)),0,2),osFactor=qualityFactor(params_[kParamQuality]);const double tubeAmount=std::clamp(params_[kParamTubeAmount],0.0,1.0),tapeAmount=std::clamp(params_[kParamTapeAmount],0.0,1.0),tapeStability=std::clamp(params_[kParamTapeStability],0.0,1.0),glueAmount=std::clamp(params_[kParamGlueAmount],0.0,1.0),glueCharacter=std::clamp(params_[kParamGlueCharacter],0.0,1.0),vinylCharacter=std::clamp(params_[kParamVinylCharacter],0.0,1.0),vinylWear=std::clamp(params_[kParamVinylWear],0.0,1.0),widthGain=2.0*std::clamp(params_[kParamWidth],0.0,1.0),depthBipolar=(std::clamp(params_[kParamDepth],0.0,1.0)-0.5)*2.0,lowMono=std::clamp(params_[kParamLowMono],0.0,1.0),autoGain=consoleOn&&autoGainOn?consoleAutoGain(mode,drive):1.0,tubeGain=tubeOn&&autoGainOn?tubeAutoGain(tubeType,tubeAmount):1.0,tapeGain=tapeOn&&autoGainOn?tapeAutoGain(tapeSpeed,tapeAmount):1.0,glueGain=glueOn&&autoGainOn?glueAutoGain(glueAmount,glueCharacter):1.0,vinylGain=vinylOn&&autoGainOn?vinylAutoGain(vinylCharacter,vinylWear):1.0,lowMonoCoeff=stereoOnePoleCoefficient(120.0,sampleRate_),depthCoeff=stereoOnePoleCoefficient(2000.0,sampleRate_),depthGain=stereoDepthGain(depthBipolar),correlationCoeff=stereoOnePoleCoefficient(4.0,sampleRate_);const int osIslands=bypass?0:(static_cast<int>(consoleOn)+static_cast<int>(tubeOn)+static_cast<int>(tapeOn)+static_cast<int>(vinylOn&&(vinylCharacter>0.0||vinylWear>0.0)));const int latencyDelay=latencyCompensation(osFactor,osIslands);
 auto processFrame=[&](double leftIn,double rightIn,bool stereo,double&leftOut,double&rightOut){const double meterL=bypass?leftIn:leftIn*inputGain,meterR=bypass?rightIn:rightIn*inputGain;inputMeter_.push(meterL,stereo?meterR:meterL);if(bypass){leftOut=latencyAligner_[0].process(leftIn,kFixedLatencySamples);rightOut=stereo?latencyAligner_[1].process(rightIn,kFixedLatencySamples):leftOut;outputMeter_.push(leftOut,stereo?rightOut:leftOut);return;}double l=meterL,r=meterR;if(consoleOn){l*=calibrationGain;r*=calibrationGain;l=processConsoleSample(l,consoleState_[0],0,0,mode,drive);r=stereo?processConsoleSample(r,consoleState_[1],0,1,mode,drive):l;l*=calibrationReturn*autoGain;r*=calibrationReturn*autoGain;}if(tubeOn){const double tubeSampleRate=sampleRate_*static_cast<double>(osFactor);if(nonlinearOversamplingFactor_[0]!=osFactor){nonlinearOversampling_[0].reset();nonlinearOversamplingFactor_[0]=osFactor;tubeState_[0].reset();}l=nonlinearOversampling_[0].process(l*calibrationGain,osFactor,[&](double v){return processTubeSample(v,tubeState_[0],tubeType,tubeAmount,tubeSampleRate);})*calibrationReturn*tubeGain;if(stereo){if(nonlinearOversamplingFactor_[1]!=osFactor){nonlinearOversampling_[1].reset();nonlinearOversamplingFactor_[1]=osFactor;tubeState_[1].reset();}r=nonlinearOversampling_[1].process(r*calibrationGain,osFactor,[&](double v){return processTubeSample(v,tubeState_[1],tubeType,tubeAmount,tubeSampleRate);})*calibrationReturn*tubeGain;}else r=l;}if(tapeOn){l=processTapeSample(l*calibrationGain,tapeState_[0],0,0,tapeSpeed,tapeAmount,tapeStability)*calibrationReturn*tapeGain;r=stereo?processTapeSample(r*calibrationGain,tapeState_[1],0,1,tapeSpeed,tapeAmount,tapeStability)*calibrationReturn*tapeGain:l;}if(glueOn){const double glueL=l*calibrationGain,glueR=r*calibrationGain,detector=stereo?std::max(std::abs(glueL),std::abs(glueR)):std::abs(glueL),linkedGain=processGlueGain(detector,glueState_[0],glueAmount,glueCharacter)*glueGain;l=glueL*linkedGain*calibrationReturn;r=stereo?glueR*linkedGain*calibrationReturn:l;}if(vinylOn){l=processVinylSample(l*calibrationGain,vinylState_[0],0,0,vinylCharacter,vinylWear)*calibrationReturn*vinylGain;r=stereo?processVinylSample(r*calibrationGain,vinylState_[1],0,1,vinylCharacter,vinylWear)*calibrationReturn*vinylGain:l;}if(stereo){auto&st=stereoState_[0];processStereoFieldSample(l,r,st,widthGain,lowMono,lowMonoCoeff,depthGain,depthCoeff,correlationCoeff);}leftOut=latencyAligner_[0].process(l*outputGain*inputMatchGain,latencyDelay);rightOut=stereo?latencyAligner_[1].process(r*outputGain*inputMatchGain,latencyDelay):leftOut;outputMeter_.push(leftOut,stereo?rightOut:leftOut);};
 if(data.symbolicSampleSize==kSample32){auto*inL=channels>0?inBus.channelBuffers32[0]:nullptr;auto*outL=channels>0?outBus.channelBuffers32[0]:nullptr;auto*inR=channels>1?inBus.channelBuffers32[1]:inL;auto*outR=channels>1?outBus.channelBuffers32[1]:outL;if(inL&&outL)for(int32 i=0;i<data.numSamples;++i){double l=0,r=0;processFrame(inL[i],inR?inR[i]:inL[i],channels>1,l,r);outL[i]=static_cast<float>(l);if(channels>1&&outR)outR[i]=static_cast<float>(r);}}else if(data.symbolicSampleSize==kSample64){auto*inL=channels>0?inBus.channelBuffers64[0]:nullptr;auto*outL=channels>0?outBus.channelBuffers64[0]:nullptr;auto*inR=channels>1?inBus.channelBuffers64[1]:inL;auto*outR=channels>1?outBus.channelBuffers64[1]:outL;if(inL&&outL)for(int32 i=0;i<data.numSamples;++i){double l=0,r=0;processFrame(inL[i],inR?inR[i]:inL[i],channels>1,l,r);outL[i]=l;if(channels>1&&outR)outR[i]=r;}}inputMeter_.publish();outputMeter_.publish();const bool outputSource=params_[kParamMeterSource]>=0.5;const Metering& selected=outputSource?outputMeter_:inputMeter_;publishMeterParameters(data.outputParameterChanges,selected.vuL(),selected.vuR(),selected.peakL(),selected.peakR(),outputSource,data.numSamples);// Never forward input silence metadata blindly: the fixed latency line and DSP state
 // can still emit valid tail samples after the host marks the input block silent.
 outBus.silenceFlags=0;return kResultOk;
}
tresult PLUGIN_API Processor::setState(IBStream* state){if(!state)return kResultFalse;IBStreamer streamer(state,kLittleEndian);for(ParamID id=0;id<kParamCount;++id){double loaded=0.0;if(!streamer.readDouble(loaded)){if(id==kParamTubeType){params_[kParamTubeType]=0.5;params_[kParamMeterSource]=1.0;break;}if(id==kParamMeterSource){params_[kParamMeterSource]=1.0;break;}if(id==kParamTapeHiss){params_[kParamTapeHiss]=params_[kParamConsoleNoise];params_[kParamVinylNoise]=params_[kParamConsoleNoise];break;}if(id==kParamVinylNoise){params_[kParamVinylNoise]=params_[kParamConsoleNoise];break;}return kResultFalse;}params_[id]=std::clamp(loaded,0.0,1.0);}syncMixFxTargets();resetConsoleState();return kResultOk;}
tresult PLUGIN_API Processor::getState(IBStream* state){if(!state)return kResultFalse;IBStreamer streamer(state,kLittleEndian);for(const auto value:params_)if(!streamer.writeDouble(value))return kResultFalse;return kResultOk;}
} // namespace MixEngine
