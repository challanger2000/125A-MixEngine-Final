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
constexpr double kSr=48000.0;
constexpr int kMaxBlock=127;
constexpr double kPi=3.14159265358979323846;
using Param=std::pair<ParamID,double>;

void setParam(ParameterChanges& changes,ParamID id,double value){
    int32 queueIndex=0;
    auto* queue=changes.addParameterData(id,queueIndex);
    if(!queue)throw 10;
    int32 pointIndex=0;
    if(queue->addPoint(0,std::clamp(value,0.0,1.0),pointIndex)!=kResultTrue)throw 11;
}

void fillAutomation(ParameterChanges& c,int block){
    const double t=static_cast<double>(block);
    const auto wave=[&](double speed,double phase=0.0){
        return 0.5+0.5*std::sin(t*speed+phase);
    };

    setParam(c,MixEngine::kParamBypass,(block%37)==0?1.0:0.0);
    setParam(c,MixEngine::kParamInput,0.35+0.30*wave(0.071));
    setParam(c,MixEngine::kParamCalibration,(block%3)/2.0);
    setParam(c,MixEngine::kParamAutoGain,(block%5)==0?0.0:1.0);

    setParam(c,MixEngine::kParamConsoleOn,(block%11)==0?0.0:1.0);
    setParam(c,MixEngine::kParamConsoleMode,(block%4)/3.0);
    setParam(c,MixEngine::kParamConsoleDrive,0.05+0.80*wave(0.053,0.4));
    setParam(c,MixEngine::kParamConsoleCrosstalk,wave(0.037)); // retired: must be inert
    setParam(c,MixEngine::kParamConsoleNoise,0.0);

    setParam(c,MixEngine::kParamTubeOn,(block%3)==0?1.0:0.0);
    setParam(c,MixEngine::kParamTubeAmount,0.75*wave(0.091,0.2));
    setParam(c,MixEngine::kParamTubeType,(block%3)/2.0);

    setParam(c,MixEngine::kParamTapeOn,(block%4)<2?1.0:0.0);
    setParam(c,MixEngine::kParamTapeAmount,0.70*wave(0.067,0.5));
    setParam(c,MixEngine::kParamTapeSpeed,(block%3)/2.0);
    setParam(c,MixEngine::kParamTapeStability,0.55+0.45*wave(0.041,0.3));
    setParam(c,MixEngine::kParamTapeHiss,0.0);

    setParam(c,MixEngine::kParamGlueOn,(block%5)<3?1.0:0.0);
    setParam(c,MixEngine::kParamGlueAmount,0.55*wave(0.083,0.1));
    setParam(c,MixEngine::kParamGlueCharacter,wave(0.047,0.8));

    setParam(c,MixEngine::kParamVinylOn,(block%7)<3?1.0:0.0);
    setParam(c,MixEngine::kParamVinylCharacter,0.45*wave(0.059,0.6));
    setParam(c,MixEngine::kParamVinylWear,0.30*wave(0.073,0.9));
    setParam(c,MixEngine::kParamVinylNoise,0.0);

    setParam(c,MixEngine::kParamDepth,0.15+0.70*wave(0.061,0.7));
    setParam(c,MixEngine::kParamWidth,0.20+0.70*wave(0.043,0.2));
    setParam(c,MixEngine::kParamLowMono,0.65*wave(0.077,0.4));
    setParam(c,MixEngine::kParamQuality,(block%3)/2.0);
    setParam(c,MixEngine::kParamOutput,0.40+0.20*wave(0.051,0.5));
    setParam(c,MixEngine::kParamMeterSource,(block&1)?1.0:0.0);
}

struct Harness {
    std::unique_ptr<MixEngine::Processor> channel=std::make_unique<MixEngine::Processor>();
    std::unique_ptr<MixEngine::Processor> mixfx=std::make_unique<MixEngine::Processor>();

    Harness(){
        ProcessSetup setup{};
        setup.processMode=kRealtime;
        setup.symbolicSampleSize=kSample64;
        setup.maxSamplesPerBlock=kMaxBlock;
        setup.sampleRate=kSr;
        if(channel->setupProcessing(setup)!=kResultOk || mixfx->setupProcessing(setup)!=kResultOk)throw 20;
        if(channel->setProcessing(true)!=kResultOk || mixfx->setProcessing(true)!=kResultOk)throw 21;
        SpeakerArrangement arrangement=SpeakerArr::kStereo;
        if(mixfx->setMixChannelArrangements(&arrangement,1)!=kResultOk)throw 22;
    }
};


double runFocusedParity(const std::vector<Param>& params,const char* name){
    Harness h;
    long long samplePos=0;
    double maxDiff=0.0;
    const std::array<int,5> sizes{{1,7,31,64,127}};
    for(int block=0;block<80;++block){
        const int n=sizes[static_cast<std::size_t>(block%sizes.size())];
        std::array<double,kMaxBlock> inL{},inR{},chL{},chR{},mxL{},mxR{};
        for(int i=0;i<n;++i,++samplePos){
            const double t=static_cast<double>(samplePos)/kSr;
            inL[static_cast<std::size_t>(i)]=0.10*std::sin(2.0*kPi*197.0*t)+0.05*std::sin(2.0*kPi*3011.0*t+0.2);
            inR[static_cast<std::size_t>(i)]=0.08*std::sin(2.0*kPi*313.0*t+0.4)+0.04*std::sin(2.0*kPi*7013.0*t+0.7);
        }
        ParameterChanges chChanges{64},mxChanges{64};
        if(block==0){
            for(const auto&q:std::vector<Param>{
                {MixEngine::kParamBypass,0.0},{MixEngine::kParamInput,0.5},{MixEngine::kParamOutput,0.5},
                {MixEngine::kParamCalibration,0.0},{MixEngine::kParamAutoGain,0.0},
                {MixEngine::kParamConsoleOn,0.0},{MixEngine::kParamConsoleNoise,0.0},
                {MixEngine::kParamTubeOn,0.0},{MixEngine::kParamTapeOn,0.0},{MixEngine::kParamTapeHiss,0.0},
                {MixEngine::kParamGlueOn,0.0},{MixEngine::kParamVinylOn,0.0},{MixEngine::kParamVinylNoise,0.0},
                {MixEngine::kParamDepth,0.5},{MixEngine::kParamWidth,0.5},{MixEngine::kParamLowMono,0.0},
                {MixEngine::kParamQuality,1.0}}){
                setParam(chChanges,q.first,q.second);setParam(mxChanges,q.first,q.second);
            }
            for(const auto&q:params){setParam(chChanges,q.first,q.second);setParam(mxChanges,q.first,q.second);}
        }

        double* inPtrs[2]{inL.data(),inR.data()};
        double* chPtrs[2]{chL.data(),chR.data()};
        double* mxPtrs[2]{mxL.data(),mxR.data()};
        AudioBusBuffers chIn{},chOut{},mxIn{},mxOut{};
        chIn.numChannels=2;chIn.channelBuffers64=inPtrs;chOut.numChannels=2;chOut.channelBuffers64=chPtrs;
        mxIn.numChannels=2;mxIn.channelBuffers64=inPtrs;mxOut.numChannels=2;mxOut.channelBuffers64=mxPtrs;

        ProcessData chData{};chData.processMode=kRealtime;chData.symbolicSampleSize=kSample64;chData.numSamples=n;
        chData.numInputs=1;chData.numOutputs=1;chData.inputs=&chIn;chData.outputs=&chOut;
        chData.inputParameterChanges=block==0?&chChanges:nullptr;
        if(h.channel->process(chData)!=kResultOk)throw 40;

        ProcessData control{};control.processMode=kRealtime;control.symbolicSampleSize=kSample64;control.numSamples=n;
        control.inputParameterChanges=block==0?&mxChanges:nullptr;
        if(h.mixfx->processMixControl(&control)!=kResultOk)throw 41;

        ProcessData mxData{};mxData.processMode=kRealtime;mxData.symbolicSampleSize=kSample64;mxData.numSamples=n;
        mxData.numInputs=1;mxData.numOutputs=1;mxData.inputs=&mxIn;mxData.outputs=&mxOut;
        if(h.mixfx->processMixChannel(0,&mxData)!=kResultOk)throw 42;

        for(int i=0;i<n;++i){
            maxDiff=std::max(maxDiff,std::abs(chL[static_cast<std::size_t>(i)]-mxL[static_cast<std::size_t>(i)]));
            maxDiff=std::max(maxDiff,std::abs(chR[static_cast<std::size_t>(i)]-mxR[static_cast<std::size_t>(i)]));
        }
    }
    std::cout<<"FocusedParity "<<name<<" maxDiff="<<maxDiff<<"\n";
    return maxDiff;
}
}

int main(){
    try{
        Harness h;
        long long samplePos=0;
        double maxDiff=0.0;
        double maxAbs=0.0;
        int firstDiffBlock=-1;
        int firstDiffSample=-1;
        int maxDiffBlock=-1;
        int maxDiffSample=-1;
        double firstChL=0.0,firstMxL=0.0,firstChR=0.0,firstMxR=0.0;
        double maxChL=0.0,maxMxL=0.0,maxChR=0.0,maxMxR=0.0;
        const std::array<int,5> sizes{{1,7,31,64,127}};

        for(int block=0;block<240;++block){
            const int n=sizes[static_cast<std::size_t>(block%sizes.size())];
            std::array<double,kMaxBlock> inL{},inR{},chL{},chR{},mxL{},mxR{};
            for(int i=0;i<n;++i,++samplePos){
                const double t=static_cast<double>(samplePos)/kSr;
                inL[static_cast<std::size_t>(i)]=
                    0.10*std::sin(2.0*kPi*197.0*t)+
                    0.05*std::sin(2.0*kPi*3011.0*t+0.2);
                inR[static_cast<std::size_t>(i)]=
                    0.08*std::sin(2.0*kPi*313.0*t+0.4)+
                    0.04*std::sin(2.0*kPi*7013.0*t+0.7);
            }

            ParameterChanges chChanges{64},mxChanges{64},chOutChanges{16},mxOutChanges{16};
            fillAutomation(chChanges,block);
            fillAutomation(mxChanges,block);

            double* inPtrs[2]{inL.data(),inR.data()};
            double* chPtrs[2]{chL.data(),chR.data()};
            double* mxPtrs[2]{mxL.data(),mxR.data()};
            AudioBusBuffers chIn{},chOut{},mxIn{},mxOut{};
            chIn.numChannels=2;chIn.channelBuffers64=inPtrs;
            chOut.numChannels=2;chOut.channelBuffers64=chPtrs;
            mxIn.numChannels=2;mxIn.channelBuffers64=inPtrs;
            mxOut.numChannels=2;mxOut.channelBuffers64=mxPtrs;

            ProcessData chData{};
            chData.processMode=kRealtime;
            chData.symbolicSampleSize=kSample64;
            chData.numSamples=n;
            chData.numInputs=1;chData.numOutputs=1;
            chData.inputs=&chIn;chData.outputs=&chOut;
            chData.inputParameterChanges=&chChanges;
            chData.outputParameterChanges=&chOutChanges;
            if(h.channel->process(chData)!=kResultOk)throw 30;

            ProcessData control{};
            control.processMode=kRealtime;
            control.symbolicSampleSize=kSample64;
            control.numSamples=n;
            control.inputParameterChanges=&mxChanges;
            control.outputParameterChanges=&mxOutChanges;
            if(h.mixfx->processMixControl(&control)!=kResultOk)throw 31;

            ProcessData mxData{};
            mxData.processMode=kRealtime;
            mxData.symbolicSampleSize=kSample64;
            mxData.numSamples=n;
            mxData.numInputs=1;mxData.numOutputs=1;
            mxData.inputs=&mxIn;mxData.outputs=&mxOut;
            if(h.mixfx->processMixChannel(0,&mxData)!=kResultOk)throw 32;

            for(int i=0;i<n;++i){
                for(const double v:{chL[static_cast<std::size_t>(i)],chR[static_cast<std::size_t>(i)],
                                    mxL[static_cast<std::size_t>(i)],mxR[static_cast<std::size_t>(i)]}){
                    if(!std::isfinite(v))throw 33;
                    maxAbs=std::max(maxAbs,std::abs(v));
                }
                const double diffL=std::abs(chL[static_cast<std::size_t>(i)]-mxL[static_cast<std::size_t>(i)]);
                const double diffR=std::abs(chR[static_cast<std::size_t>(i)]-mxR[static_cast<std::size_t>(i)]);
                const double localMax=std::max(diffL,diffR);
                if(localMax>maxDiff){
                    maxDiff=localMax;
                    maxDiffBlock=block;
                    maxDiffSample=i;
                    maxChL=chL[static_cast<std::size_t>(i)];
                    maxMxL=mxL[static_cast<std::size_t>(i)];
                    maxChR=chR[static_cast<std::size_t>(i)];
                    maxMxR=mxR[static_cast<std::size_t>(i)];
                }
                if(firstDiffBlock<0 && localMax>1.0e-10){
                    firstDiffBlock=block;
                    firstDiffSample=i;
                    firstChL=chL[static_cast<std::size_t>(i)];
                    firstMxL=mxL[static_cast<std::size_t>(i)];
                    firstChR=chR[static_cast<std::size_t>(i)];
                    firstMxR=mxR[static_cast<std::size_t>(i)];
                }
            }
        }

        for(const auto& focused:std::vector<std::pair<const char*,std::vector<Param>>>{
            {"Console",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.65}}},
            {"Glue",{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.65},{MixEngine::kParamGlueCharacter,0.5}}},
            {"Vinyl",{{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.5},{MixEngine::kParamVinylWear,0.25}}},
            {"Console+Vinyl",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.65},{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.5},{MixEngine::kParamVinylWear,0.25}}},
            {"Glue+Vinyl",{{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.65},{MixEngine::kParamGlueCharacter,0.5},{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.5},{MixEngine::kParamVinylWear,0.25}}},
            {"Console+Glue+Vinyl",{{MixEngine::kParamConsoleOn,1.0},{MixEngine::kParamConsoleDrive,0.65},{MixEngine::kParamGlueOn,1.0},{MixEngine::kParamGlueAmount,0.65},{MixEngine::kParamGlueCharacter,0.5},{MixEngine::kParamVinylOn,1.0},{MixEngine::kParamVinylCharacter,0.5},{MixEngine::kParamVinylWear,0.25}}}
        }){
            runFocusedParity(focused.second,focused.first);
        }

        std::cout<<"Automation stress maxAbs="<<maxAbs
                 <<" Channel/MixFX maxDiff="<<maxDiff<<"\n";
        if(firstDiffBlock>=0){
            std::cout<<"First parity mismatch block="<<firstDiffBlock
                     <<" sample="<<firstDiffSample
                     <<" blockSize="<<sizes[static_cast<std::size_t>(firstDiffBlock%sizes.size())]
                     <<" chL="<<firstChL<<" mxL="<<firstMxL
                     <<" chR="<<firstChR<<" mxR="<<firstMxR
                     <<" bypass="<<(((firstDiffBlock%37)==0)?1:0)
                     <<" console="<<(((firstDiffBlock%11)==0)?0:1)
                     <<" tube="<<(((firstDiffBlock%3)==0)?1:0)
                     <<" tape="<<(((firstDiffBlock%4)<2)?1:0)
                     <<" glue="<<(((firstDiffBlock%5)<3)?1:0)
                     <<" vinyl="<<(((firstDiffBlock%7)<3)?1:0)
                     <<" quality="<<((firstDiffBlock%3)/2.0)
                     <<"\n";
        }

        if(maxDiffBlock>=0){
            std::cout<<"Maximum parity mismatch block="<<maxDiffBlock
                     <<" sample="<<maxDiffSample
                     <<" blockSize="<<sizes[static_cast<std::size_t>(maxDiffBlock%sizes.size())]
                     <<" chL="<<maxChL<<" mxL="<<maxMxL
                     <<" chR="<<maxChR<<" mxR="<<maxMxR
                     <<" bypass="<<(((maxDiffBlock%37)==0)?1:0)
                     <<" console="<<(((maxDiffBlock%11)==0)?0:1)
                     <<" tube="<<(((maxDiffBlock%3)==0)?1:0)
                     <<" tape="<<(((maxDiffBlock%4)<2)?1:0)
                     <<" glue="<<(((maxDiffBlock%5)<3)?1:0)
                     <<" vinyl="<<(((maxDiffBlock%7)<3)?1:0)
                     <<" quality="<<((maxDiffBlock%3)/2.0)
                     <<"\n";
        }

        if(maxAbs>8.0){
            std::cerr<<"Automation stress produced implausible output peak\n";
            return 1;
        }
        if(maxDiff>1.0e-10){
            std::cerr<<"Automation stress path parity FAILED\n";
            return 2;
        }

        std::cout<<"Automation stress PASSED: finite processing, bounded output, exact Channel/MixFX parity\n";
        return 0;
    }catch(int code){
        std::cerr<<"Automation stress setup FAIL: "<<code<<"\n";
        return code;
    }catch(...){
        std::cerr<<"Automation stress unknown exception\n";
        return 90;
    }
}
