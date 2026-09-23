#include "processor.h"
#include "console_oversampling_live.h"
#include "media_oversampling_live.h"
#include "nonlinear_cores.h"
#include "character_morph.h"

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
    0.0, 0.5, 0.5, 1.0,
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
    const double e = d * (1.40 - 0.40 * d) + 0.32 * creativeZone(d);
    double amount = 0.70 * e;
    switch (mode) { case 0: amount = 0.30*e; break; case 1: amount = e; break; case 2: amount = e; break; default: break; }
    amount = std::clamp(amount,0.0,1.0);
    if (amount <= 0.0) return 1.0;
    const double shape = 1.0 + amount;
    const double norm = std::tanh(shape);
    if (norm <= 0.0) return 1.0;
    const double saturatedSlope = shape / norm;
    const double blendedSlope = (1.0 - amount) + amount * saturatedSlope;
    const double slopeComp = blendedSlope > 0.0 ? 1.0 / blendedSlope : 1.0;
    // Measured V2 Classic/drive sweeps show roughly -6 dB residual level loss
    // at full drive after slope compensation. Restore that loss progressively
    // so Level Match compares character rather than simple loudness reduction.
    return slopeComp * dbToGain(5.8 * d);
}
inline double tubeAutoGain(double typeMorph, double amount) {
    const double a = std::clamp(amount,0.0,1.0); if (a<=0.0) return 1.0;
    const double e=a*(1.35-0.35*a)+0.24*creativeZone(a);
    return dbToGain(-tubeCharacter(typeMorph).autoGainDb*e*e);
}
inline double tapeAutoGain(double amount){const double a=std::clamp(amount,0.0,1.0);return a<=0.0?1.0:dbToGain(3.00*a*a);}
inline double glueAutoGain(double amount,double character){const double a=std::clamp(amount,0.0,1.0);if(a<=0.0)return 1.0;const double c=std::clamp(character,0.0,1.0),strength=a*(0.55+0.45*a),s2=strength*strength;const double makeupDb=(2.0+1.0*c)*s2*s2;return dbToGain(makeupDb);}
inline double vinylAutoGain(double character,double wear){const double c=std::clamp(character,0.0,1.0),w=std::clamp(wear,0.0,1.0);return dbToGain(1.05*c+0.48*w);}
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
 mixFxSnapshotCapacity_=std::max<int32>(1,setup.maxSamplesPerBlock);
 prepareMixFxSnapshotBuffers();
 for(auto& lane:mixFxAutomation_){
  lane.clear();
  lane.reserve(static_cast<std::size_t>(mixFxSnapshotCapacity_));
 }
 mixFxBlockStartParams_=params_;
 mixFxAutomationSamples_=0;
#endif
 resetConsoleState();return AudioEffect::setupProcessing(setup);}
tresult PLUGIN_API Processor::setActive(TBool state){
 if(state){resetConsoleState();meterExchange_.onActivate(processSetup);}
 else{processing_=false;meterExchange_.onDeactivate();}
 return AudioEffect::setActive(state);
}
tresult PLUGIN_API Processor::setProcessing(TBool state){const bool p=state!=0;if(p&&!processing_)resetConsoleState();processing_=p;return kResultOk;}

void Processor::resetConsoleState(){
 for(auto&s:consoleState_)s={};for(auto&source:mixFxConsoleState_)for(auto&s:source)s={};for(auto&s:tapeState_)s={};for(auto&source:mixFxTapeState_)for(auto&s:source)s={};for(auto&s:glueState_)s={};for(auto&source:mixFxGlueState_)for(auto&s:source)s={};for(auto&s:vinylState_)s={};for(auto&source:mixFxVinylState_)for(auto&s:source)s={};for(auto&s:stereoState_)s={};for(auto&source:mixFxStereoState_)for(auto&s:source)s={};
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
 tubeTypeMorphState_=std::clamp(params_[kParamTubeType],0.0,1.0);
 tapeSpeedMorphState_=std::clamp(params_[kParamTapeSpeed],0.0,1.0);
 const double resetTubeGain=(params_[kParamAutoGain]>=0.5&&params_[kParamTubeOn]>=0.5)?tubeAutoGain(tubeTypeMorphState_,params_[kParamTubeAmount]):1.0;
 tubeCompGainState_=resetTubeGain;
 mixFxTubeTypeMorphState_.fill(tubeTypeMorphState_);
 mixFxTapeSpeedMorphState_.fill(tapeSpeedMorphState_);
 mixFxTubeCompGainState_.fill(resetTubeGain);
#ifndef MIXENGINE_CHANNEL_BUILD
 mixFxSnapshotSamples_.store(0,std::memory_order_relaxed);
 mixFxSnapshotChannels_.fill(0);
 for(auto& lane:mixFxAutomation_)lane.clear();
 mixFxBlockStartParams_=params_;
 mixFxAutomationSamples_=0;
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
double Processor::processConsoleSample(double x,ConsoleChannelState& state,int sourceIndex,int lane,int mode,double drive,int osFactor,double noiseAmount,double calibrationNorm){const double d=std::clamp(drive,0.0,1.0),variation=stableVariation(sourceIndex,lane),tolerance=1.0+0.008*variation*d,bias=0.0015*variation;x=x*tolerance+bias*d;if(mode==2&&d>0.0){const double magneticDrive=1.35+0.95*d,target=std::tanh(magneticDrive*x+0.24*d*state.transformerMemory),memoryCoeff=0.20+0.16*d;state.transformerMemory+=memoryCoeff*(target-state.transformerMemory);const double staticMag=std::tanh(magneticDrive*x),hysteresis=state.transformerMemory-staticMag;x+=0.16*d*hysteresis;}else state.transformerMemory*=0.995;state.lowMemory+=lowCoeff_*(x-state.lowMemory);const double low=state.lowMemory,high=x-state.lowMemory;OversamplingEngine* engine=nullptr;int* currentFactor=nullptr;const int factor=OversamplingEngine::sanitiseFactor(osFactor);if(mixFxEngaged_){engine=&mixFxConsoleOversampling_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];currentFactor=&mixFxConsoleOversamplingFactor_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];}else{engine=&consoleOversampling_[static_cast<std::size_t>(lane)];currentFactor=&consoleOversamplingFactor_[static_cast<std::size_t>(lane)];}double y=x;if(d>0.0){y=processConsoleOversampledCore(*engine,*currentFactor,factor,x,low,high,mode,drive);y=dcBlock(y,state);}else{state.dcX1=x;state.dcY1=x;}noiseAmount=std::clamp(noiseAmount,0.0,1.0);if(noiseAmount>0.0){if(state.noiseRng==0u)state.noiseRng=makeNoiseSeed(sourceIndex,lane,0xC01150E1u);const double white=randomBipolar(state.noiseRng),coeff=1.0-std::exp(-2.0*kPi*6500.0/sampleRate_);state.noiseMemory+=coeff*(white-state.noiseMemory);const double colored=0.72*white+0.28*state.noiseMemory,n=noiseAmount*noiseAmount,sourceScale=(mixFxEngaged_&&mixFxChannelCount_>1)?1.0/std::sqrt(static_cast<double>(mixFxChannelCount_)):1.0;y+=colored*0.00025*n*sourceScale*calibrationNorm;}return y;}
double Processor::processTubeSample(double x,double typeMorph,double amount)const{return processTubeNonlinearCore(x,typeMorph,amount);}
double Processor::processTapeSample(double x,TapeChannelState& state,int sourceIndex,int lane,double speedMorph,double amount,double stability,int osFactor,double noiseAmount,double calibrationNorm){
 const double a=std::clamp(amount,0.0,1.0);
 if(a<=0.0)return x;
 const auto model=tapeCharacter(speedMorph);
 const double instability=1.0-std::clamp(stability,0.0,1.0);
 const double cutoff=std::min(model.cutoffHz,sampleRate_*0.45);
 const double bumpFreq=model.bumpFreqHz,bumpAmount=model.bumpAmount,wowHz=model.wowHz,flutterHz=model.flutterHz,speedTone=model.hissTone;
 const double highCoeff=1.0-std::exp(-2.0*kPi*cutoff/sampleRate_),bumpCoeff=1.0-std::exp(-2.0*kPi*bumpFreq/sampleRate_);
 state.wowPhase+=2.0*kPi*wowHz/sampleRate_;
 state.flutterPhase+=2.0*kPi*flutterHz/sampleRate_;
 if(state.wowPhase>=2.0*kPi)state.wowPhase-=2.0*kPi;
 if(state.flutterPhase>=2.0*kPi)state.flutterPhase-=2.0*kPi;

 const double derivative=x-state.previousInput;
 state.previousInput=x;
 const double zone=creativeZone(a);
 const double motion=0.75*std::sin(state.wowPhase)+0.25*std::sin(state.flutterPhase);
 const double transportDepth=1.20*std::pow(instability,1.5);
 const double transport=x+derivative*motion*transportDepth;

 // FINAL v1.1.0 program-dependent tape compression is retained.
 const double strength=a*(0.55+0.45*a);
 const double attack=std::exp(-1.0/(0.001*2.5*sampleRate_));
 const double release=std::exp(-1.0/(0.001*85.0*sampleRate_));
 const double level=std::abs(transport);
 const double envCoeff=level>state.compressionEnvelope?attack:release;
 state.compressionEnvelope=envCoeff*state.compressionEnvelope+(1.0-envCoeff)*level;
 const double over=std::max(0.0,state.compressionEnvelope-0.20);
 const double dynamicGain=1.0/(1.0+1.10*strength*over);
 const double compressedTransport=transport*dynamicGain;

 // V2 adds bounded magnetic memory/hysteresis after the FINAL compression stage.
 const double magneticDrive=1.4+0.9*a+0.4*zone;
 const double feedback=0.10+0.16*a+0.08*zone;
 const double targetMag=std::tanh(magneticDrive*compressedTransport+feedback*state.magneticMemory);
 const double memoryCoeff=0.32+0.20*a;
 state.magneticMemory+=memoryCoeff*(targetMag-state.magneticMemory);
 const double staticMag=std::tanh(magneticDrive*compressedTransport);
 const double hysteresis=state.magneticMemory-staticMag;
 const double magneticTransport=compressedTransport+0.10*a*hysteresis;

 const double shape=1.0+0.75*a+0.55*zone;
 OversamplingEngine* osEngine=nullptr;
 int* osCurrentFactor=nullptr;
 osFactor=OversamplingEngine::sanitiseFactor(osFactor);
 if(mixFxEngaged_){
  osEngine=&mixFxTapeOversampling_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];
  osCurrentFactor=&mixFxTapeOversamplingFactor_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];
 }else{
  osEngine=&tapeOversampling_[static_cast<std::size_t>(lane)];
  osCurrentFactor=&tapeOversamplingFactor_[static_cast<std::size_t>(lane)];
 }
 const double saturated=processTapeOversampledCore(*osEngine,*osCurrentFactor,osFactor,magneticTransport,shape);
 state.highMemory+=highCoeff*(saturated-state.highMemory);
 state.lowMemory+=bumpCoeff*(state.highMemory-state.lowMemory);
 const double tape=state.highMemory+(0.75+0.50*a+0.35*zone)*bumpAmount*a*state.lowMemory;
 const double wet=std::min(0.96,(0.22+0.58*a)*a+0.16*zone);
 LatencyAligner* dryAligner=mixFxEngaged_?&mixFxTapeDryAligner_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)]:&tapeDryAligner_[static_cast<std::size_t>(lane)];
 const double dry=dryAligner->process(x,oversamplingBulkDelay(osFactor,1));
 double y=dry+(tape-dry)*wet;

 noiseAmount=std::clamp(noiseAmount,0.0,1.0);
 if(noiseAmount>0.0){
  if(state.noiseRng==0u)state.noiseRng=makeNoiseSeed(sourceIndex,lane,0x7A9E51A5u);
  const double white=randomBipolar(state.noiseRng),hissCoeff=1.0-std::exp(-2.0*kPi*1200.0/sampleRate_);
  state.hissMemory+=hissCoeff*(white-state.hissMemory);
  const double hiss=white-0.78*state.hissMemory,n=noiseAmount*noiseAmount;
  const double sourceScale=(mixFxEngaged_&&mixFxChannelCount_>1)?1.0/std::sqrt(static_cast<double>(mixFxChannelCount_)):1.0;
  y+=hiss*0.0070*speedTone*n*sourceScale*calibrationNorm;
 }
 return y;
}
double Processor::processGlueGain(double detector,GlueChannelState& state,double amount,double character)const{
 const double a=std::clamp(amount,0.0,1.0);
 if(a<=0.0)return 1.0;
 const double c=std::clamp(character,0.0,1.0);
 const double strength=a*(0.55+0.45*a);
 const double zone=creativeZone(a);

 // Keep the useful range from sitting permanently below threshold. Higher
 // Amount increases authority mostly through ratio/blend/GR ceiling rather
 // than simply forcing ever more of the low-level programme into compression.
 const double thresholdDb=-6.0-2.0*strength-1.0*zone;
 const double ratio=1.0+2.7*strength+0.9*c*strength+2.0*zone;
 const double kneeDb=8.5-2.5*c;

 detector=std::abs(detector);
 const double attackMs=(18.0+28.0*strength)*(1.0-0.45*c);
 const double attackCoeff=std::exp(-1.0/(0.001*attackMs*sampleRate_));

 if(detector>state.envelope){
  state.envelope=attackCoeff*state.envelope+(1.0-attackCoeff)*detector;
 }else{
  // Program-dependent release: shallow gain reduction lets go quickly while
  // deep events recover more slowly, avoiding both chatter and flat pumping.
  const double currentOver=gainToDb(state.envelope)-thresholdDb;
  const double programme=std::clamp((currentOver+2.0)/12.0,0.0,1.0);
  const double fastReleaseMs=62.0-27.0*c;
  const double slowReleaseMs=185.0-75.0*c;
  const double releaseMs=fastReleaseMs+(slowReleaseMs-fastReleaseMs)*programme;
  const double releaseCoeff=std::exp(-1.0/(0.001*releaseMs*sampleRate_));
  state.envelope=releaseCoeff*state.envelope+(1.0-releaseCoeff)*detector;
 }

 const double envDb=gainToDb(state.envelope);
 const double overDb=envDb-thresholdDb;
 double gr=0.0;
 if(overDb>kneeDb*0.5)gr=overDb-overDb/ratio;
 else if(overDb>-kneeDb*0.5){
  const double p=overDb+kneeDb*0.5;
  gr=(1.0-1.0/ratio)*p*p/(2.0*kneeDb);
 }
 gr=std::min(gr,7.5+2.5*c+7.0*zone);

 // Parallel-style blend keeps transients and low-level detail alive at normal
 // settings, while the upper range can still become intentionally forceful.
 const double blend=strength*(0.52+0.48*strength);
 const double compressedGain=dbToGain(-gr);
 return 1.0+(compressedGain-1.0)*blend;
}
double Processor::processVinylSample(double x,VinylChannelState& state,int sourceIndex,int lane,double character,double wear,int osFactor,double noiseAmount,double calibrationNorm){const double c=std::clamp(character,0.0,1.0),w=std::clamp(wear,0.0,1.0);double y=x;if(c>0.0||w>0.0){const double colorZone=creativeZone(c),wearCurve=w*(0.70+0.30*w);double cutoff=20000.0-1400.0*c-10800.0*wearCurve-700.0*colorZone;cutoff=std::clamp(cutoff,5200.0,sampleRate_*0.45);const double highCoeff=1.0-std::exp(-2.0*kPi*cutoff/sampleRate_),bodyCoeff=1.0-std::exp(-2.0*kPi*190.0/sampleRate_);state.highMemory+=highCoeff*(x-state.highMemory);state.lowMemory+=bodyCoeff*(state.highMemory-state.lowMemory);const double stylusCutoff=6500.0-3000.0*wearCurve,stylusCoeff=1.0-std::exp(-2.0*kPi*stylusCutoff/sampleRate_);state.stylusMemory+=stylusCoeff*(state.highMemory-state.stylusMemory);const double compliance=0.20*wearCurve*wearCurve,worn=state.highMemory+compliance*(state.stylusMemory-state.highMemory);const double bodyBoost=0.052*c+0.002*w+0.024*colorZone,colored=worn+bodyBoost*state.lowMemory,drive=1.0+0.66*c+0.025*w+0.58*colorZone;OversamplingEngine* osEngine=nullptr;int* osCurrentFactor=nullptr;osFactor=OversamplingEngine::sanitiseFactor(osFactor);if(mixFxEngaged_){osEngine=&mixFxVinylOversampling_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];osCurrentFactor=&mixFxVinylOversamplingFactor_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)];}else{osEngine=&vinylOversampling_[static_cast<std::size_t>(lane)];osCurrentFactor=&vinylOversamplingFactor_[static_cast<std::size_t>(lane)];}const double shaped=processVinylOversampledCore(*osEngine,*osCurrentFactor,osFactor,colored,drive,c),wet=std::clamp(0.08+0.52*c+0.34*wearCurve+0.12*colorZone,0.0,0.98);LatencyAligner* dryAligner=mixFxEngaged_?&mixFxVinylDryAligner_[static_cast<std::size_t>(sourceIndex)][static_cast<std::size_t>(lane)]:&vinylDryAligner_[static_cast<std::size_t>(lane)];const double dry=dryAligner->process(x,oversamplingBulkDelay(osFactor,1));y=dry+(shaped-dry)*wet;}noiseAmount=std::clamp(noiseAmount,0.0,1.0);if(noiseAmount>0.0){if(state.noiseRng==0u)state.noiseRng=makeNoiseSeed(sourceIndex,lane,0xB17E4A11u);const double white=randomBipolar(state.noiseRng),surfaceCoeff=1.0-std::exp(-2.0*kPi*7000.0/sampleRate_);state.surfaceMemory+=surfaceCoeff*(white-state.surfaceMemory);const double surface=0.52*white+0.48*state.surfaceMemory,n=noiseAmount*noiseAmount,sourceScale=(mixFxEngaged_&&mixFxChannelCount_>1)?1.0/std::sqrt(static_cast<double>(mixFxChannelCount_)):1.0,surfaceLevel=0.0070*(0.75+0.75*w);y+=surface*surfaceLevel*n*sourceScale*calibrationNorm;const double clickRateHz=(0.12+2.2*w*w)*noiseAmount,eventProbe=0.5*(randomBipolar(state.noiseRng)+1.0);if(eventProbe<clickRateHz/sampleRate_){const double randomLevel=0.5*(randomBipolar(state.noiseRng)+1.0);state.clickEnvelope=0.018+0.055*randomLevel*(0.35+0.65*w);state.clickPolarity=randomBipolar(state.noiseRng)>=0.0?1.0:-1.0;}const double clickDecayMs=0.7+1.8*w,clickDecay=std::exp(-1.0/(0.001*clickDecayMs*sampleRate_));y+=state.clickPolarity*state.clickEnvelope*n*sourceScale*calibrationNorm;state.clickEnvelope*=clickDecay;if(state.clickEnvelope<1.0e-10)state.clickEnvelope=0.0;}return y;}
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
 if(data){
  // Preserve the values that are active at the first sample of this Mix FX
  // block, then copy every host automation point before committing the final
  // values to params_/atomics for persistence and the next block.
  mixFxBlockStartParams_=params_;
  mixFxAutomationSamples_=std::max<int32>(0,data->numSamples);
  for(auto& lane:mixFxAutomation_)lane.clear();

  if(auto* changes=data->inputParameterChanges){
   const int32 queueCount=changes->getParameterCount();
   for(int32 qi=0;qi<queueCount;++qi){
    auto* queue=changes->getParameterData(qi);
    if(!queue)continue;
    const ParamID id=queue->getParameterId();
    if(id>=kParamCount)continue;
    auto& lane=mixFxAutomation_[static_cast<std::size_t>(id)];
    const int32 pointCount=queue->getPointCount();
    for(int32 pi=0;pi<pointCount;++pi){
     int32 offset=0; ParamValue value=0.0;
     if(queue->getPoint(pi,offset,value)!=kResultTrue)continue;
     const int32 clampedOffset=data->numSamples>0?std::clamp<int32>(offset,0,data->numSamples-1):0;
     lane.push_back({clampedOffset,std::clamp(static_cast<double>(value),0.0,1.0)});
    }
   }
  }

  readParameterChanges(data->inputParameterChanges);
 }else{
  mixFxBlockStartParams_=params_;
  mixFxAutomationSamples_=0;
  for(auto& lane:mixFxAutomation_)lane.clear();
 }

 syncMixFxTargets();
 if(data)captureMixFxInputSnapshot(*data);

 if(data){
  double inSqL=0.0,inSqR=0.0,outSqL=0.0,outSqR=0.0,inPeakL=0.0,inPeakR=0.0,outPeakL=0.0,outPeakR=0.0;
  const int32 count=std::clamp<int32>(mixFxChannelCount_,0,kMaxMixFxChannels);
  for(int32 i=0;i<count;++i){
   const auto& in=mixFxInputMeters_[static_cast<std::size_t>(i)];
   const auto& out=mixFxOutputMeters_[static_cast<std::size_t>(i)];
   const double ivl=in.vuL(),ivr=in.vuR(),ovl=out.vuL(),ovr=out.vuR();
   inSqL+=ivl*ivl;inSqR+=ivr*ivr;outSqL+=ovl*ovl;outSqR+=ovr*ovr;
   inPeakL=std::max(inPeakL,in.peakL());inPeakR=std::max(inPeakR,in.peakR());
   outPeakL=std::max(outPeakL,out.peakL());outPeakR=std::max(outPeakR,out.peakR());
  }
  const double inVuL=std::sqrt(inSqL),inVuR=std::sqrt(inSqR),
               outVuL=std::sqrt(outSqL),outVuR=std::sqrt(outSqR);
  const bool outputSource=mixFxMeterSource_.load(std::memory_order_relaxed)>=0.5;
  if(outputSource)
   publishMeterParameters(data->outputParameterChanges,outVuL,outVuR,
                          std::max(outPeakL,outVuL),std::max(outPeakR,outVuR),true,data->numSamples);
  else
   publishMeterParameters(data->outputParameterChanges,inVuL,inVuR,
                          std::max(inPeakL,inVuL),std::max(inPeakR,inVuR),false,data->numSamples);
 }
 return kResultOk;
}

tresult Processor::processMixFxChannelInternal(int32 index,ProcessData& data){
 if(index<0||index>=kMaxMixFxChannels)return kInvalidArgument;
 if(mixFxChannelCount_>0&&index>=mixFxChannelCount_)return kInvalidArgument;
 if(data.numInputs<1||data.numOutputs<1||data.numSamples<=0)return kResultOk;

 auto& inBus=data.inputs[0];
 auto& outBus=data.outputs[0];
 const int32 channels=std::min<int32>(std::min(inBus.numChannels,outBus.numChannels),kMaxAudioChannels);
 if(channels<=0)return kResultOk;

 auto& inputMeter=mixFxInputMeters_[static_cast<std::size_t>(index)];
 auto& outputMeter=mixFxOutputMeters_[static_cast<std::size_t>(index)];
 inputMeter.beginBlock();
 outputMeter.beginBlock();

 std::array<double,kParamCount> localParams{};
 if(mixFxAutomationSamples_==data.numSamples)localParams=mixFxBlockStartParams_;
 else localParams=params_;

 std::array<std::size_t,kParamCount> automationIndex{};
 const bool haveAutomation=mixFxAutomationSamples_==data.numSamples;

 const auto applyAutomationAt=[&](int32 sampleOffset){
  if(!haveAutomation)return false;
  bool changed=false;
  for(ParamID id=0;id<kParamCount;++id){
   auto& idx=automationIndex[static_cast<std::size_t>(id)];
   const auto& lane=mixFxAutomation_[static_cast<std::size_t>(id)];
   while(idx<lane.size()&&lane[idx].offset<=sampleOffset){
    localParams[id]=lane[idx].value;
    ++idx;
    changed=true;
   }
  }
  return changed;
 };

 bool bypass=false,consoleOn=false,tubeOn=false,tapeOn=false,glueOn=false,vinylOn=false,autoGainOn=false;
 double inputGain=1.0,outputGain=1.0,inputMatchGain=1.0,calibrationGain=1.0,calibrationReturn=1.0,drive=0.0,crosstalk=0.0;
 int mode=0,osFactor=1,latencyDelay=kFixedLatencySamples;
 double tubeTypeTarget=0.5,tapeSpeedTarget=0.5,tubeAmount=0.0,tapeAmount=0.0,tapeStability=1.0;
 double glueAmount=0.0,glueCharacter=0.5,vinylCharacter=0.0,vinylWear=0.0;
 double widthGain=1.0,depthBipolar=0.0,lowMono=0.0,autoGain=1.0,tubeGainTarget=1.0;
 double tapeGain=1.0,glueGain=1.0,vinylGain=1.0,consoleNoise=0.0,tapeHiss=0.0,vinylNoise=0.0,lowMonoCoeff=0.0,depthCoeff=0.0,depthGain=1.0;
 const double characterRamp=1.0-std::exp(-1.0/(0.012*sampleRate_));

 const auto refreshDerived=[&](){
  bypass=localParams[kParamBypass]>=0.5;
  consoleOn=localParams[kParamConsoleOn]>=0.5;
  tubeOn=localParams[kParamTubeOn]>=0.5;
  tapeOn=localParams[kParamTapeOn]>=0.5;
  glueOn=localParams[kParamGlueOn]>=0.5;
  vinylOn=localParams[kParamVinylOn]>=0.5;
  autoGainOn=localParams[kParamAutoGain]>=0.5;

  inputGain=dbToGain((localParams[kParamInput]-0.5)*24.0);
  outputGain=dbToGain((localParams[kParamOutput]-0.5)*24.0);
  inputMatchGain=autoGainOn?1.0/inputGain:1.0;
  const double calibrationDb=calibrationReferenceDb(localParams[kParamCalibration]);
  calibrationGain=dbToGain(-calibrationDb);
  calibrationReturn=1.0/calibrationGain;
  drive=localParams[kParamConsoleDrive];
  crosstalk=std::clamp(localParams[kParamConsoleCrosstalk],0.0,1.0)*0.018;

  mode=std::clamp(static_cast<int>(std::lround(localParams[kParamConsoleMode]*3.0)),0,3);
  osFactor=qualityFactor(localParams[kParamQuality]);
  tubeTypeTarget=std::clamp(localParams[kParamTubeType],0.0,1.0);
  tapeSpeedTarget=std::clamp(localParams[kParamTapeSpeed],0.0,1.0);
  tubeAmount=std::clamp(localParams[kParamTubeAmount],0.0,1.0);
  tapeAmount=std::clamp(localParams[kParamTapeAmount],0.0,1.0);
  tapeStability=std::clamp(localParams[kParamTapeStability],0.0,1.0);
  glueAmount=std::clamp(localParams[kParamGlueAmount],0.0,1.0);
  glueCharacter=std::clamp(localParams[kParamGlueCharacter],0.0,1.0);
  vinylCharacter=std::clamp(localParams[kParamVinylCharacter],0.0,1.0);
  vinylWear=std::clamp(localParams[kParamVinylWear],0.0,1.0);
  consoleNoise=std::clamp(localParams[kParamConsoleNoise],0.0,1.0);
  tapeHiss=std::clamp(localParams[kParamTapeHiss],0.0,1.0);
  vinylNoise=std::clamp(localParams[kParamVinylNoise],0.0,1.0);
  widthGain=2.0*std::clamp(localParams[kParamWidth],0.0,1.0);
  depthBipolar=(std::clamp(localParams[kParamDepth],0.0,1.0)-0.5)*2.0;
  lowMono=std::clamp(localParams[kParamLowMono],0.0,1.0);

  autoGain=consoleOn&&autoGainOn?consoleAutoGain(mode,drive):1.0;
  tubeGainTarget=tubeOn&&autoGainOn?tubeAutoGain(tubeTypeTarget,tubeAmount):1.0;
  tapeGain=tapeOn&&autoGainOn?tapeAutoGain(tapeAmount):1.0;
  glueGain=glueOn&&autoGainOn?glueAutoGain(glueAmount,glueCharacter):1.0;
  vinylGain=vinylOn&&autoGainOn?vinylAutoGain(vinylCharacter,vinylWear):1.0;
  lowMonoCoeff=stereoOnePoleCoefficient(120.0,sampleRate_);
  depthCoeff=stereoOnePoleCoefficient(2000.0,sampleRate_);
  depthGain=stereoDepthGain(depthBipolar);

  const int osIslands=bypass?0:
      (static_cast<int>(consoleOn&&drive>0.0)+
       static_cast<int>(tubeOn&&tubeAmount>0.0)+
       static_cast<int>(tapeOn&&tapeAmount>0.0)+
       static_cast<int>(vinylOn&&(vinylCharacter>0.0||vinylWear>0.0)));
  latencyDelay=latencyCompensation(osFactor,osIslands);
 };

 applyAutomationAt(0);
 refreshDerived();

 auto& tubeTypeState=mixFxTubeTypeMorphState_[static_cast<std::size_t>(index)];
 auto& tapeSpeedState=mixFxTapeSpeedMorphState_[static_cast<std::size_t>(index)];
 auto& tubeGainState=mixFxTubeCompGainState_[static_cast<std::size_t>(index)];

 const auto processFrame=[&](int32 sampleIndex,double leftIn,double rightIn,bool stereo,double&leftOut,double&rightOut){
  tubeTypeState+=characterRamp*(tubeTypeTarget-tubeTypeState);
  tapeSpeedState+=characterRamp*(tapeSpeedTarget-tapeSpeedState);
  tubeGainState+=characterRamp*(tubeGainTarget-tubeGainState);
  const double tubeType=tubeTypeState,tapeSpeed=tapeSpeedState,tubeGain=tubeGainState;

  const double meterL=bypass?leftIn:leftIn*inputGain;
  const double meterR=bypass?rightIn:rightIn*inputGain;
  inputMeter.push(meterL,stereo?meterR:meterL);

  if(bypass){
   auto& align=mixFxLatencyAligner_[static_cast<std::size_t>(index)];
   leftOut=align[0].process(leftIn,kFixedLatencySamples);
   rightOut=stereo?align[1].process(rightIn,kFixedLatencySamples):leftOut;
   outputMeter.push(leftOut,stereo?rightOut:leftOut);
   return;
  }

  double l=meterL,r=meterR;
  if(consoleOn){
   if(crosstalk>0.0&&mixFxSnapshotSamples_.load(std::memory_order_acquire)==data.numSamples){
    l+=mixFxCrosstalkSource(index,0,sampleIndex)*inputGain*crosstalk;
    r+=mixFxCrosstalkSource(index,1,sampleIndex)*inputGain*crosstalk;
   }
   l*=calibrationGain;r*=calibrationGain;
   auto& states=mixFxConsoleState_[static_cast<std::size_t>(index)];
   l=processConsoleSample(l,states[0],index,0,mode,drive,osFactor,consoleNoise,calibrationGain);
   r=stereo?processConsoleSample(r,states[1],index,1,mode,drive,osFactor,consoleNoise,calibrationGain):l;
   l*=calibrationReturn*autoGain;r*=calibrationReturn*autoGain;
  }
  if(tubeOn&&tubeAmount>0.0){
   auto& engines=mixFxNonlinearOversampling_[static_cast<std::size_t>(index)];
   auto& factors=mixFxNonlinearOversamplingFactor_[static_cast<std::size_t>(index)];
   if(factors[0]!=osFactor){engines[0].reset();factors[0]=osFactor;}
   l=engines[0].process(l*calibrationGain,osFactor,[&](double v){
    return processTubeSample(v,tubeType,tubeAmount);
   })*calibrationReturn*tubeGain;
   if(stereo){
    if(factors[1]!=osFactor){engines[1].reset();factors[1]=osFactor;}
    r=engines[1].process(r*calibrationGain,osFactor,[&](double v){
     return processTubeSample(v,tubeType,tubeAmount);
    })*calibrationReturn*tubeGain;
   }else r=l;
  }
  if(tapeOn){
   auto& states=mixFxTapeState_[static_cast<std::size_t>(index)];
   l=processTapeSample(l*calibrationGain,states[0],index,0,tapeSpeed,tapeAmount,tapeStability,osFactor,tapeHiss,calibrationGain)*calibrationReturn*tapeGain;
   r=stereo?processTapeSample(r*calibrationGain,states[1],index,1,tapeSpeed,tapeAmount,tapeStability,osFactor,tapeHiss,calibrationGain)*calibrationReturn*tapeGain:l;
  }
  if(glueOn){
   auto& states=mixFxGlueState_[static_cast<std::size_t>(index)];
   const double glueL=l*calibrationGain,glueR=r*calibrationGain;
   const double detector=stereo?std::max(std::abs(glueL),std::abs(glueR)):std::abs(glueL);
   const double linkedGain=processGlueGain(detector,states[0],glueAmount,glueCharacter)*glueGain;
   l=glueL*linkedGain*calibrationReturn;
   r=stereo?glueR*linkedGain*calibrationReturn:l;
  }
  if(vinylOn){
   auto& states=mixFxVinylState_[static_cast<std::size_t>(index)];
   l=processVinylSample(l*calibrationGain,states[0],index,0,vinylCharacter,vinylWear,osFactor,vinylNoise,calibrationGain)*calibrationReturn*vinylGain;
   r=stereo?processVinylSample(r*calibrationGain,states[1],index,1,vinylCharacter,vinylWear,osFactor,vinylNoise,calibrationGain)*calibrationReturn*vinylGain:l;
  }
  if(stereo){
   auto& st=mixFxStereoState_[static_cast<std::size_t>(index)][0];
   processStereoFieldSample(l,r,st,widthGain,lowMono,lowMonoCoeff,depthGain,depthCoeff);
  }

  auto& align=mixFxLatencyAligner_[static_cast<std::size_t>(index)];
  leftOut=align[0].process(l*outputGain*inputMatchGain,latencyDelay);
  rightOut=stereo?align[1].process(r*outputGain*inputMatchGain,latencyDelay):leftOut;
  outputMeter.push(leftOut,stereo?rightOut:leftOut);
 };

 if(data.symbolicSampleSize==kSample32){
  auto* inL=inBus.channelBuffers32[0];auto* outL=outBus.channelBuffers32[0];
  auto* inR=channels>1?inBus.channelBuffers32[1]:inL;
  auto* outR=channels>1?outBus.channelBuffers32[1]:outL;
  if(!inL||!outL)return kResultOk;
  for(int32 i=0;i<data.numSamples;++i){
   if(i>0&&applyAutomationAt(i))refreshDerived();
   double l=0,r=0;
   processFrame(i,inL[i],inR?inR[i]:inL[i],channels>1,l,r);
   outL[i]=static_cast<float>(l);
   if(channels>1&&outR)outR[i]=static_cast<float>(r);
  }
 }else if(data.symbolicSampleSize==kSample64){
  auto* inL=inBus.channelBuffers64[0];auto* outL=outBus.channelBuffers64[0];
  auto* inR=channels>1?inBus.channelBuffers64[1]:inL;
  auto* outR=channels>1?outBus.channelBuffers64[1]:outL;
  if(!inL||!outL)return kResultOk;
  for(int32 i=0;i<data.numSamples;++i){
   if(i>0&&applyAutomationAt(i))refreshDerived();
   double l=0,r=0;
   processFrame(i,inL[i],inR?inR[i]:inL[i],channels>1,l,r);
   outL[i]=l;
   if(channels>1&&outR)outR[i]=r;
  }
 }else return kResultFalse;

 inputMeter.publish();
 outputMeter.publish();
 outBus.silenceFlags=0;
 return kResultOk;
}

tresult PLUGIN_API Processor::processMixChannel(int32 index,ProcessData* data){if(!data)return kInvalidArgument;return processMixFxChannelInternal(index,*data);}
#endif


tresult PLUGIN_API Processor::process(ProcessData& data){
    // Mix FX control is handled by processMixControl/processMixChannel. Preserve
    // the existing control path here while the standard VST3 path below applies
    // automation at the exact sample offsets supplied by the host.
    if(mixFxEngaged_){
        readParameterChanges(data.inputParameterChanges);
        syncMixFxTargets();
        return kResultOk;
    }

    // Parameter-only flushes still need to update the persistent state.
    if(data.numInputs<1||data.numOutputs<1||data.numSamples<=0){
        readParameterChanges(data.inputParameterChanges);
        return kResultOk;
    }

    struct AutomationCursor {
        IParamValueQueue* queue=nullptr;
        ParamID id=0;
        int32 point=0;
        int32 count=0;
        int32 offset=0;
        ParamValue value=0.0;
        bool valid=false;
    };
    std::array<AutomationCursor,kParamCount> automation{};
    int32 automationCount=0;

    if(auto* changes=data.inputParameterChanges){
        const int32 queues=changes->getParameterCount();
        for(int32 qIndex=0;qIndex<queues && automationCount<static_cast<int32>(automation.size());++qIndex){
            auto* queue=changes->getParameterData(qIndex);
            if(!queue)continue;
            const ParamID id=queue->getParameterId();
            if(id>=kParamCount)continue;
            const int32 points=queue->getPointCount();
            if(points<=0)continue;

            auto& cursor=automation[static_cast<std::size_t>(automationCount++)];
            cursor.queue=queue;
            cursor.id=id;
            cursor.count=points;
            cursor.point=0;
            int32 offset=0; ParamValue value=0.0;
            if(queue->getPoint(0,offset,value)==kResultTrue){
                cursor.offset=std::clamp<int32>(offset,0,data.numSamples-1);
                cursor.value=std::clamp(value,0.0,1.0);
                cursor.valid=true;
            }
        }
    }

    const auto advanceCursor=[&](AutomationCursor& cursor){
        ++cursor.point;
        if(cursor.point>=cursor.count){cursor.valid=false;return;}
        int32 offset=0; ParamValue value=0.0;
        if(cursor.queue->getPoint(cursor.point,offset,value)==kResultTrue){
            cursor.offset=std::clamp<int32>(offset,0,data.numSamples-1);
            cursor.value=std::clamp(value,0.0,1.0);
        }else cursor.valid=false;
    };

    const auto applyAutomationAt=[&](int32 sampleOffset){
        bool changed=false;
        for(int32 i=0;i<automationCount;++i){
            auto& cursor=automation[static_cast<std::size_t>(i)];
            while(cursor.valid && cursor.offset<=sampleOffset){
                params_[cursor.id]=cursor.value;
                changed=true;
                advanceCursor(cursor);
            }
        }
        return changed;
    };

    auto& inBus=data.inputs[0];
    auto& outBus=data.outputs[0];
    const int32 channels=std::min(inBus.numChannels,outBus.numChannels);
    inputMeter_.beginBlock();
    outputMeter_.beginBlock();

    bool bypass=false,consoleOn=false,tubeOn=false,tapeOn=false,glueOn=false,vinylOn=false,autoGainOn=false;
    double inputGain=1.0,outputGain=1.0,inputMatchGain=1.0,calibrationGain=1.0,calibrationReturn=1.0,drive=0.0;
    int mode=0,osFactor=1,latencyDelay=kFixedLatencySamples;
    double tubeTypeTarget=0.5,tapeSpeedTarget=0.5,tubeAmount=0.0,tapeAmount=0.0,tapeStability=1.0;
    double glueAmount=0.0,glueCharacter=0.5,vinylCharacter=0.0,vinylWear=0.0;
    double widthGain=1.0,depthBipolar=0.0,lowMono=0.0,autoGain=1.0,tubeGainTarget=1.0;
    double tapeGain=1.0,glueGain=1.0,vinylGain=1.0,consoleNoise=0.0,tapeHiss=0.0,vinylNoise=0.0,lowMonoCoeff=0.0,depthCoeff=0.0,depthGain=1.0;
    const double characterRamp=1.0-std::exp(-1.0/(0.012*sampleRate_));

    const auto refreshDerived=[&](){
        bypass=params_[kParamBypass]>=0.5;
        consoleOn=params_[kParamConsoleOn]>=0.5;
        tubeOn=params_[kParamTubeOn]>=0.5;
        tapeOn=params_[kParamTapeOn]>=0.5;
        glueOn=params_[kParamGlueOn]>=0.5;
        vinylOn=params_[kParamVinylOn]>=0.5;
        autoGainOn=params_[kParamAutoGain]>=0.5;

        inputGain=dbToGain((params_[kParamInput]-0.5)*24.0);
        outputGain=dbToGain((params_[kParamOutput]-0.5)*24.0);
        inputMatchGain=autoGainOn?1.0/inputGain:1.0;
        const double calibrationDb=calibrationReferenceDb(params_[kParamCalibration]);
        calibrationGain=dbToGain(-calibrationDb);
        calibrationReturn=1.0/calibrationGain;
        drive=params_[kParamConsoleDrive];

        mode=std::clamp(static_cast<int>(std::lround(params_[kParamConsoleMode]*3.0)),0,3);
        osFactor=qualityFactor(params_[kParamQuality]);
        tubeTypeTarget=std::clamp(params_[kParamTubeType],0.0,1.0);
        tapeSpeedTarget=std::clamp(params_[kParamTapeSpeed],0.0,1.0);
        tubeAmount=std::clamp(params_[kParamTubeAmount],0.0,1.0);
        tapeAmount=std::clamp(params_[kParamTapeAmount],0.0,1.0);
        tapeStability=std::clamp(params_[kParamTapeStability],0.0,1.0);
        glueAmount=std::clamp(params_[kParamGlueAmount],0.0,1.0);
        glueCharacter=std::clamp(params_[kParamGlueCharacter],0.0,1.0);
        vinylCharacter=std::clamp(params_[kParamVinylCharacter],0.0,1.0);
        vinylWear=std::clamp(params_[kParamVinylWear],0.0,1.0);
        consoleNoise=std::clamp(params_[kParamConsoleNoise],0.0,1.0);
        tapeHiss=std::clamp(params_[kParamTapeHiss],0.0,1.0);
        vinylNoise=std::clamp(params_[kParamVinylNoise],0.0,1.0);
        widthGain=2.0*std::clamp(params_[kParamWidth],0.0,1.0);
        depthBipolar=(std::clamp(params_[kParamDepth],0.0,1.0)-0.5)*2.0;
        lowMono=std::clamp(params_[kParamLowMono],0.0,1.0);

        autoGain=consoleOn&&autoGainOn?consoleAutoGain(mode,drive):1.0;
        tubeGainTarget=tubeOn&&autoGainOn?tubeAutoGain(tubeTypeTarget,tubeAmount):1.0;
        tapeGain=tapeOn&&autoGainOn?tapeAutoGain(tapeAmount):1.0;
        glueGain=glueOn&&autoGainOn?glueAutoGain(glueAmount,glueCharacter):1.0;
        vinylGain=vinylOn&&autoGainOn?vinylAutoGain(vinylCharacter,vinylWear):1.0;
        lowMonoCoeff=stereoOnePoleCoefficient(120.0,sampleRate_);
        depthCoeff=stereoOnePoleCoefficient(2000.0,sampleRate_);
        depthGain=stereoDepthGain(depthBipolar);

        const int osIslands=bypass?0:
            (static_cast<int>(consoleOn&&drive>0.0)+
             static_cast<int>(tubeOn&&tubeAmount>0.0)+
             static_cast<int>(tapeOn&&tapeAmount>0.0)+
             static_cast<int>(vinylOn&&(vinylCharacter>0.0||vinylWear>0.0)));
        latencyDelay=latencyCompensation(osFactor,osIslands);
    };

    applyAutomationAt(0);
    refreshDerived();

    const auto processFrame=[&](double leftIn,double rightIn,bool stereo,double&leftOut,double&rightOut){
        tubeTypeMorphState_+=characterRamp*(tubeTypeTarget-tubeTypeMorphState_);
        tapeSpeedMorphState_+=characterRamp*(tapeSpeedTarget-tapeSpeedMorphState_);
        tubeCompGainState_+=characterRamp*(tubeGainTarget-tubeCompGainState_);
        const double tubeType=tubeTypeMorphState_;
        const double tapeSpeed=tapeSpeedMorphState_;
        const double tubeGain=tubeCompGainState_;

        const double meterL=bypass?leftIn:leftIn*inputGain;
        const double meterR=bypass?rightIn:rightIn*inputGain;
        inputMeter_.push(meterL,stereo?meterR:meterL);

        if(bypass){
            leftOut=latencyAligner_[0].process(leftIn,kFixedLatencySamples);
            rightOut=stereo?latencyAligner_[1].process(rightIn,kFixedLatencySamples):leftOut;
            outputMeter_.push(leftOut,stereo?rightOut:leftOut);
            return;
        }

        double l=meterL,r=meterR;
        if(consoleOn){
            l*=calibrationGain;r*=calibrationGain;
            l=processConsoleSample(l,consoleState_[0],0,0,mode,drive,osFactor,consoleNoise,calibrationGain);
            r=stereo?processConsoleSample(r,consoleState_[1],0,1,mode,drive,osFactor,consoleNoise,calibrationGain):l;
            l*=calibrationReturn*autoGain;r*=calibrationReturn*autoGain;
        }
        if(tubeOn&&tubeAmount>0.0){
            if(nonlinearOversamplingFactor_[0]!=osFactor){
                nonlinearOversampling_[0].reset();
                nonlinearOversamplingFactor_[0]=osFactor;
            }
            l=nonlinearOversampling_[0].process(l*calibrationGain,osFactor,[&](double v){
                return processTubeSample(v,tubeType,tubeAmount);
            })*calibrationReturn*tubeGain;
            if(stereo){
                if(nonlinearOversamplingFactor_[1]!=osFactor){
                    nonlinearOversampling_[1].reset();
                    nonlinearOversamplingFactor_[1]=osFactor;
                }
                r=nonlinearOversampling_[1].process(r*calibrationGain,osFactor,[&](double v){
                    return processTubeSample(v,tubeType,tubeAmount);
                })*calibrationReturn*tubeGain;
            }else r=l;
        }
        if(tapeOn){
            l=processTapeSample(l*calibrationGain,tapeState_[0],0,0,tapeSpeed,tapeAmount,tapeStability,osFactor,tapeHiss,calibrationGain)*calibrationReturn*tapeGain;
            r=stereo?processTapeSample(r*calibrationGain,tapeState_[1],0,1,tapeSpeed,tapeAmount,tapeStability,osFactor,tapeHiss,calibrationGain)*calibrationReturn*tapeGain:l;
        }
        if(glueOn){
            const double glueL=l*calibrationGain,glueR=r*calibrationGain;
            const double detector=stereo?std::max(std::abs(glueL),std::abs(glueR)):std::abs(glueL);
            const double linkedGain=processGlueGain(detector,glueState_[0],glueAmount,glueCharacter)*glueGain;
            l=glueL*linkedGain*calibrationReturn;
            r=stereo?glueR*linkedGain*calibrationReturn:l;
        }
        if(vinylOn){
            l=processVinylSample(l*calibrationGain,vinylState_[0],0,0,vinylCharacter,vinylWear,osFactor,vinylNoise,calibrationGain)*calibrationReturn*vinylGain;
            r=stereo?processVinylSample(r*calibrationGain,vinylState_[1],0,1,vinylCharacter,vinylWear,osFactor,vinylNoise,calibrationGain)*calibrationReturn*vinylGain:l;
        }
        if(stereo){
            auto& st=stereoState_[0];
            processStereoFieldSample(l,r,st,widthGain,lowMono,lowMonoCoeff,depthGain,depthCoeff);
        }

        leftOut=latencyAligner_[0].process(l*outputGain*inputMatchGain,latencyDelay);
        rightOut=stereo?latencyAligner_[1].process(r*outputGain*inputMatchGain,latencyDelay):leftOut;
        outputMeter_.push(leftOut,stereo?rightOut:leftOut);
    };

    if(data.symbolicSampleSize==kSample32){
        auto* inL=channels>0?inBus.channelBuffers32[0]:nullptr;
        auto* outL=channels>0?outBus.channelBuffers32[0]:nullptr;
        auto* inR=channels>1?inBus.channelBuffers32[1]:inL;
        auto* outR=channels>1?outBus.channelBuffers32[1]:outL;
        if(inL&&outL){
            for(int32 i=0;i<data.numSamples;++i){
                if(i>0&&applyAutomationAt(i))refreshDerived();
                double l=0,r=0;
                processFrame(inL[i],inR?inR[i]:inL[i],channels>1,l,r);
                outL[i]=static_cast<float>(l);
                if(channels>1&&outR)outR[i]=static_cast<float>(r);
            }
        }
    }else if(data.symbolicSampleSize==kSample64){
        auto* inL=channels>0?inBus.channelBuffers64[0]:nullptr;
        auto* outL=channels>0?outBus.channelBuffers64[0]:nullptr;
        auto* inR=channels>1?inBus.channelBuffers64[1]:inL;
        auto* outR=channels>1?outBus.channelBuffers64[1]:outL;
        if(inL&&outL){
            for(int32 i=0;i<data.numSamples;++i){
                if(i>0&&applyAutomationAt(i))refreshDerived();
                double l=0,r=0;
                processFrame(inL[i],inR?inR[i]:inL[i],channels>1,l,r);
                outL[i]=l;
                if(channels>1&&outR)outR[i]=r;
            }
        }
    }else return kResultFalse;

    inputMeter_.publish();
    outputMeter_.publish();
    const bool outputSource=params_[kParamMeterSource]>=0.5;
    const Metering& selected=outputSource?outputMeter_:inputMeter_;
    publishMeterParameters(data.outputParameterChanges,selected.vuL(),selected.vuR(),
                           selected.peakL(),selected.peakR(),outputSource,data.numSamples);

    // Never forward input silence metadata blindly: the fixed latency line and
    // DSP state can still emit valid tail samples after an input-silent block.
    outBus.silenceFlags=0;
    return kResultOk;
}
tresult PLUGIN_API Processor::setState(IBStream* state){if(!state)return kResultFalse;IBStreamer streamer(state,kLittleEndian);for(ParamID id=0;id<kParamCount;++id){double loaded=0.0;if(!streamer.readDouble(loaded)){if(id==kParamTubeType){params_[kParamTubeType]=0.5;params_[kParamMeterSource]=1.0;break;}if(id==kParamMeterSource){params_[kParamMeterSource]=1.0;break;}if(id==kParamTapeHiss){params_[kParamTapeHiss]=params_[kParamConsoleNoise];params_[kParamVinylNoise]=params_[kParamConsoleNoise];break;}if(id==kParamVinylNoise){params_[kParamVinylNoise]=params_[kParamConsoleNoise];break;}return kResultFalse;}params_[id]=std::clamp(loaded,0.0,1.0);}syncMixFxTargets();resetConsoleState();return kResultOk;}
tresult PLUGIN_API Processor::getState(IBStream* state){if(!state)return kResultFalse;IBStreamer streamer(state,kLittleEndian);for(const auto value:params_)if(!streamer.writeDouble(value))return kResultFalse;return kResultOk;}
} // namespace MixEngine
