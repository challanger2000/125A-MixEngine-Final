#include "controller.h"
#include "pluginids.h"
#include "HardwareControls.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/iuidescription.h"
#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstparameters.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace MixEngine {
using namespace Steinberg;
using namespace Steinberg::Vst;
Controller::Controller()
: meterExchangeReceiver_(this)
{
}

tresult PLUGIN_API Controller::queryInterface(const TUID iid,void** obj){
    if(!obj)return kInvalidArgument;
    if(std::memcmp(iid,IDataExchangeReceiver::iid,16)==0){
        *obj=static_cast<IDataExchangeReceiver*>(this);
        EditControllerEx1::addRef();
        return kResultOk;
    }
    return EditControllerEx1::queryInterface(iid,obj);
}

namespace {
void setListDefault(StringListParameter* p,ParamValue d){
    if(!p)return;
    p->getInfo().defaultNormalizedValue=std::clamp(d,0.0,1.0);
    p->setNormalized(d);
}
void addToggle(ParameterContainer& p,const TChar* title,ParamID id,ParamValue d,int32 flags=0){
    auto* x=new StringListParameter(title,id,nullptr,ParameterInfo::kCanAutomate|flags);
    x->appendString(STR16("Off"));x->appendString(STR16("On"));
    setListDefault(x,d);
    p.addParameter(x);
}
void addRange(ParameterContainer& p,const TChar* title,const TChar* units,ParamID id,double minPlain,double maxPlain,double defaultPlain,int32 precision=0){
    auto* x=new RangeParameter(title,id,units,minPlain,maxPlain,defaultPlain,0,ParameterInfo::kCanAutomate);
    x->setPrecision(precision);p.addParameter(x);
}
}

tresult PLUGIN_API Controller::initialize(FUnknown* context){
    auto result=EditControllerEx1::initialize(context); if(result!=kResultOk)return result;

    auto* bypass=new StringListParameter(STR16("Bypass"),kParamBypass,nullptr,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass);
    bypass->appendString(STR16("Active"));bypass->appendString(STR16("Bypass"));parameters.addParameter(bypass);bypass->setNormalized(0.);

    addRange(parameters,STR16("Input Gain"),STR16("dB"),kParamInput,-12.0,12.0,0.0,1);
    auto* calibration=new StringListParameter(STR16("Reference Level"),kParamCalibration);
    calibration->appendString(STR16("-18 dBFS"));calibration->appendString(STR16("-14 dBFS"));calibration->appendString(STR16("-10 dBFS"));setListDefault(calibration,.5);parameters.addParameter(calibration);setParamNormalized(kParamCalibration,.5);
    addToggle(parameters,STR16("Level Match"),kParamAutoGain,1.);

    addToggle(parameters,STR16("Console On"),kParamConsoleOn,1.);
    auto* cm=new StringListParameter(STR16("Console Mode"),kParamConsoleMode);
    for(auto s:{STR16("Clean"),STR16("Classic"),STR16("Vintage"),STR16("Modern")})cm->appendString(s);
    setListDefault(cm,1./3.);parameters.addParameter(cm);setParamNormalized(kParamConsoleMode,1./3.);
    addRange(parameters,STR16("Console Drive"),STR16("%"),kParamConsoleDrive,0.0,100.0,25.0,0);
#ifdef MIXENGINE_CHANNEL_BUILD
    // Keep parameter ID 7 in the state ABI, but a normal insert instance has no
    // access to neighbouring DAW channels, so the control is deliberately hidden.
    auto* legacyCrosstalk=new RangeParameter(STR16("Legacy Crosstalk"),kParamConsoleCrosstalk,STR16("%"),0.0,100.0,10.0,0,ParameterInfo::kIsHidden);
    parameters.addParameter(legacyCrosstalk);
#else
    // PreSonus Mix FX sees all mix channels together; here this is real
    // channel-to-channel console crosstalk, not L/R bleed inside one stereo bus.
    addRange(parameters,STR16("Crosstalk"),STR16("%"),kParamConsoleCrosstalk,0.0,100.0,10.0,0);
#endif
    addRange(parameters,STR16("Console Noise"),STR16("%"),kParamConsoleNoise,0.0,100.0,0.0,0);

    addToggle(parameters,STR16("Tube On"),kParamTubeOn,0.);
    auto* tt=new StringListParameter(STR16("Tube Voice"),kParamTubeType);
    tt->appendString(STR16("Soft"));tt->appendString(STR16("Balanced"));tt->appendString(STR16("Hot"));setListDefault(tt,.5);parameters.addParameter(tt);setParamNormalized(kParamTubeType,.5);
    addRange(parameters,STR16("Tube Amount"),STR16("%"),kParamTubeAmount,0.0,100.0,20.0,0);

    addToggle(parameters,STR16("Tape On"),kParamTapeOn,0.);
    addRange(parameters,STR16("Tape Amount"),STR16("%"),kParamTapeAmount,0.0,100.0,20.0,0);
    auto* ts=new StringListParameter(STR16("Tape Speed"),kParamTapeSpeed);
    ts->appendString(STR16("7.5 ips"));ts->appendString(STR16("15 ips"));ts->appendString(STR16("30 ips"));setListDefault(ts,.5);parameters.addParameter(ts);setParamNormalized(kParamTapeSpeed,.5);
    addRange(parameters,STR16("Tape Stability"),STR16("%"),kParamTapeStability,0.0,100.0,90.0,0);
    addRange(parameters,STR16("Tape Hiss"),STR16("%"),kParamTapeHiss,0.0,100.0,0.0,0);

    addToggle(parameters,STR16("Glue On"),kParamGlueOn,0.);
    addRange(parameters,STR16("Glue Amount"),STR16("%"),kParamGlueAmount,0.0,100.0,15.0,0);
    addRange(parameters,STR16("Glue Response"),STR16("%"),kParamGlueCharacter,0.0,100.0,50.0,0);

    addToggle(parameters,STR16("Vinyl On"),kParamVinylOn,0.);
    addRange(parameters,STR16("Vinyl Color"),STR16("%"),kParamVinylCharacter,0.0,100.0,25.0,0);
    addRange(parameters,STR16("Vinyl Wear"),STR16("%"),kParamVinylWear,0.0,100.0,0.0,0);
    addRange(parameters,STR16("Vinyl Surface"),STR16("%"),kParamVinylNoise,0.0,100.0,0.0,0);

    addRange(parameters,STR16("Depth"),STR16("%"),kParamDepth,-100.0,100.0,0.0,0);
    addRange(parameters,STR16("Width"),STR16("%"),kParamWidth,0.0,200.0,100.0,0);
    addRange(parameters,STR16("Low Mono"),STR16("%"),kParamLowMono,0.0,100.0,0.0,0);

    auto* q=new StringListParameter(STR16("Quality"),kParamQuality);
    q->appendString(STR16("Eco (1x)"));q->appendString(STR16("Normal (2x)"));q->appendString(STR16("High (4x)"));setListDefault(q,.5);parameters.addParameter(q);setParamNormalized(kParamQuality,.5);
    addRange(parameters,STR16("Output Gain"),STR16("dB"),kParamOutput,-12.0,12.0,0.0,1);

    auto* meterSource=new StringListParameter(STR16("VU Source"),kParamMeterSource);
    meterSource->appendString(STR16("Input"));meterSource->appendString(STR16("Output"));setListDefault(meterSource,1.0);parameters.addParameter(meterSource);setParamNormalized(kParamMeterSource,1.0);

    // Keep processor-output meter parameters host-visible. Studio One must
    // forward outputParameterChanges back to the controller/editor; Steinberg's
    // AGain reference meter likewise uses kIsReadOnly without kIsHidden.
    constexpr int32 meterFlags=ParameterInfo::kIsReadOnly;
    parameters.addParameter(STR16("VU Left"),nullptr,0,0.0,meterFlags,kParamMeterL);
    parameters.addParameter(STR16("VU Right"),nullptr,0,0.0,meterFlags,kParamMeterR);
    parameters.addParameter(STR16("Clip Left"),nullptr,1,0.0,meterFlags,kParamClipL);
    parameters.addParameter(STR16("Clip Right"),nullptr,1,0.0,meterFlags,kParamClipR);
    return kResultOk;
}

tresult PLUGIN_API Controller::notify(IMessage* message){
    if(message&&meterExchangeReceiver_.onMessage(message))return kResultTrue;
    return EditControllerEx1::notify(message);
}

void PLUGIN_API Controller::queueOpened(DataExchangeUserContextID userContextID,uint32 blockSize,TBool& dispatchOnBackgroundThread){
    if(userContextID==kMeterExchangeContext&&blockSize>=sizeof(MeterExchangeData))
        dispatchOnBackgroundThread=false;
}

void PLUGIN_API Controller::queueClosed(DataExchangeUserContextID userContextID){
    if(userContextID!=kMeterExchangeContext)return;
    setParamNormalized(kParamMeterL,0.0);
    setParamNormalized(kParamMeterR,0.0);
    setParamNormalized(kParamClipL,0.0);
    setParamNormalized(kParamClipR,0.0);
}

void PLUGIN_API Controller::onDataExchangeBlocksReceived(DataExchangeUserContextID userContextID,uint32 numBlocks,DataExchangeBlock* blocks,TBool){
    if(userContextID!=kMeterExchangeContext||!blocks||numBlocks==0)return;
    const MeterExchangeData* latest=nullptr;
    for(uint32 i=0;i<numBlocks;++i){
        if(blocks[i].data&&blocks[i].size>=sizeof(MeterExchangeData))
            latest=static_cast<const MeterExchangeData*>(blocks[i].data);
    }
    if(!latest)return;
    setParamNormalized(kParamMeterL,std::clamp(latest->vuL,0.0,1.0));
    setParamNormalized(kParamMeterR,std::clamp(latest->vuR,0.0,1.0));
    setParamNormalized(kParamClipL,std::clamp(latest->clipL,0.0,1.0));
    setParamNormalized(kParamClipR,std::clamp(latest->clipR,0.0,1.0));
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state){
    if(!state)return kResultFalse;IBStreamer s(state,kLittleEndian);
    for(ParamID id=0;id<kParamCount;++id){
        double v=0.;
        if(!s.readDouble(v)){
            if(id==kParamTubeType){const double legacy=getParamNormalized(kParamConsoleNoise);setParamNormalized(kParamTubeType,.5);setParamNormalized(kParamMeterSource,1.0);setParamNormalized(kParamTapeHiss,legacy);setParamNormalized(kParamVinylNoise,legacy);break;}
            if(id==kParamMeterSource){const double legacy=getParamNormalized(kParamConsoleNoise);setParamNormalized(kParamMeterSource,1.0);setParamNormalized(kParamTapeHiss,legacy);setParamNormalized(kParamVinylNoise,legacy);break;}
            if(id==kParamTapeHiss){const double legacy=getParamNormalized(kParamConsoleNoise);setParamNormalized(kParamTapeHiss,legacy);setParamNormalized(kParamVinylNoise,legacy);break;}
            if(id==kParamVinylNoise){setParamNormalized(kParamVinylNoise,getParamNormalized(kParamConsoleNoise));break;}
            return kResultFalse;
        }
        if(std::isfinite(v))setParamNormalized(id,std::clamp(v,0.0,1.0));
        else if(auto* parameter=parameters.getParameter(id))
            setParamNormalized(id,parameter->getInfo().defaultNormalizedValue);
    }
    return kResultOk;
}

VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name,const VSTGUI::UIAttributes& a,const VSTGUI::IUIDescription* description,VSTGUI::VST3Editor* e){
    if(!name||!e)return nullptr;
    VSTGUI::CPoint o{0,0},sz{64,64};
    a.getPointAttribute("origin",o);
    a.getPointAttribute("size",sz);
    VSTGUI::CRect r(o.x,o.y,o.x+sz.x,o.y+sz.y);


    auto knob=[&](const char* n,ParamID id,HardwareKnob::Style st,const char* bitmapName)->VSTGUI::CView*{
        if(std::strcmp(name,n)!=0)return nullptr;
        auto* filmstrip=description?description->getBitmap(bitmapName):nullptr;
        return new HardwareKnob(r,e,id,st,filmstrip);
    };
    auto toggle=[&](const char* n,ParamID id,bool ledLeft=false,bool moduleLedAbove=false,bool blueLed=false)->VSTGUI::CView*{
        if(std::strcmp(name,n)!=0)return nullptr;
        auto* filmstrip=description?description->getBitmap("MixPush"):nullptr;
        return new HardwareToggle(r,e,id,filmstrip,ledLeft,moduleLedAbove,blueLed);
    };
    auto selector=[&](const char* n,ParamID id,std::vector<std::string> labels)->VSTGUI::CView*{
        if(std::strcmp(name,n)==0)return new HardwareSelector(r,e,id,std::move(labels));
        return nullptr;
    };
    auto label=[&](const char* n,const char* text,double fontSize,bool bold=false,bool muted=false)->VSTGUI::CView*{
        if(std::strcmp(name,n)==0)return new HardwareLabel(r,text,fontSize,bold,muted);
        return nullptr;
    };

    if(auto* v=selector("VUSourceSelector",kParamMeterSource,{"INPUT","OUTPUT"}))return v;
    if(auto* v=selector("QualitySelector",kParamQuality,{"ECO 1x","NORMAL 2x","HIGH 4x"}))return v;
    if(auto* v=selector("InputRefSelector",kParamCalibration,{"-18","-14","-10"}))return v;
    if(auto* v=selector("ConsoleModeSelector",kParamConsoleMode,{"CLEAN","CLASSIC","VINTAGE","MODERN"}))return v;
    if(auto* v=selector("TubeVoiceSelector",kParamTubeType,{"SOFT","BALANCED","HOT"}))return v;
    if(auto* v=selector("TapeSpeedSelector",kParamTapeSpeed,{"7.5","15","30"}))return v;

    if(auto* v=toggle("Bypass",kParamBypass,false,true,false))return v;
    if(auto* v=toggle("ConsolePower",kParamConsoleOn,false,true,false))return v;
    if(auto* v=toggle("TubePower",kParamTubeOn,false,true,false))return v;
    if(auto* v=toggle("TapePower",kParamTapeOn,false,true,false))return v;
    if(auto* v=toggle("GluePower",kParamGlueOn,false,true,false))return v;
    if(auto* v=toggle("VinylPower",kParamVinylOn,false,true,false))return v;
    if(auto* v=toggle("LevelMatch",kParamAutoGain,false,true,true))return v;

    if(std::strcmp(name,"HardwareFaceplate")==0)return new HardwareFaceplate(r);
    if(std::strcmp(name,"BrandLogo")==0)return new HardwareLogo(r);
    if(std::strcmp(name,"UIScale")==0)return new HardwareUIScale(r,e);
    if(std::strcmp(name,"VULeft")==0)return new HardwareVUMeter(r,e,kParamMeterL,description?description->getBitmap("MixVUAtlas"):nullptr);
    if(std::strcmp(name,"VURight")==0)return new HardwareVUMeter(r,e,kParamMeterR,description?description->getBitmap("MixVUAtlas"):nullptr);
    if(std::strcmp(name,"ClipL")==0)return new HardwareClipLed(r,e,kParamClipL);
    if(std::strcmp(name,"ClipR")==0)return new HardwareClipLed(r,e,kParamClipR);

    if(auto* v=knob("Input",kParamInput,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
    if(auto* v=knob("ConsoleDrive",kParamConsoleDrive,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
#ifndef MIXENGINE_CHANNEL_BUILD
    if(auto* v=knob("Crosstalk",kParamConsoleCrosstalk,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
#endif
    if(auto* v=knob("ConsoleNoise",kParamConsoleNoise,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("TubeAmount",kParamTubeAmount,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
    if(auto* v=knob("TapeAmount",kParamTapeAmount,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
    if(auto* v=knob("TapeStability",kParamTapeStability,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("TapeHiss",kParamTapeHiss,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("GlueAmount",kParamGlueAmount,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
    if(auto* v=knob("GlueCharacter",kParamGlueCharacter,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("VinylCharacter",kParamVinylCharacter,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;
    if(auto* v=knob("VinylWear",kParamVinylWear,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("VinylNoise",kParamVinylNoise,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("Depth",kParamDepth,HardwareKnob::Style::Medium,"MixKnobGunmetalMAtlas"))return v;
    if(auto* v=knob("Width",kParamWidth,HardwareKnob::Style::Medium,"MixKnobGunmetalMAtlas"))return v;
    if(auto* v=knob("LowMono",kParamLowMono,HardwareKnob::Style::Small,"MixKnobGunmetalSAtlas"))return v;
    if(auto* v=knob("Output",kParamOutput,HardwareKnob::Style::Large,"MixKnobVernierAtlas"))return v;

    // Static full-custom VSTGUI legends. These deliberately avoid CTextLabel so
    // typography and rendering stay under the same hardware-style renderer.
    if(auto* v=label("LabelVUSource","VU SOURCE",10.0,true,true))return v;
    if(auto* v=label("LabelVURef","0 VU = REF LEVEL",9.0,false,true))return v;
    if(auto* v=label("LabelMixTitle","MIX ENGINE V3",13.0,true,false))return v;
    if(auto* v=label("LabelQuality","QUALITY",10.0,true,true))return v;
    if(auto* v=label("LabelBypass","BYPASS",10.0,true,true))return v;

    if(auto* v=label("LabelInputTitle","INPUT",13.0,true,false))return v;
    if(auto* v=label("LabelConsoleTitle","CONSOLE",13.0,true,false))return v;
    if(auto* v=label("LabelTubeTitle","TUBE",13.0,true,false))return v;
    if(auto* v=label("LabelTapeTitle","TAPE",13.0,true,false))return v;
    if(auto* v=label("LabelGlueTitle","GLUE",13.0,true,false))return v;
    if(auto* v=label("LabelVinylTitle","VINYL",13.0,true,false))return v;
    if(auto* v=label("LabelStereoTitle","STEREO",13.0,true,false))return v;
    if(auto* v=label("LabelOutputTitle","OUTPUT",13.0,true,false))return v;

    if(auto* v=label("LabelInputGain","INPUT GAIN",10.5,true,false))return v;
    if(auto* v=label("LabelRefLevel","REF LEVEL",9.5,true,true))return v;
    if(auto* v=label("LabelDbfs","dBFS",8.8,false,true))return v;
    if(auto* v=label("LabelDrive","DRIVE",10.5,true,false))return v;
    if(auto* v=label("LabelMode","MODE",9.5,true,true))return v;
#ifndef MIXENGINE_CHANNEL_BUILD
    if(auto* v=label("LabelCrosstalk","CROSSTALK",8.8,true,false))return v;
#endif
    if(auto* v=label("LabelConsoleNoise","NOISE",8.8,true,false))return v;
    if(auto* v=label("LabelTubeAmount","AMOUNT",10.5,true,false))return v;
    if(auto* v=label("LabelVoice","VOICE",9.5,true,true))return v;
    if(auto* v=label("LabelTapeAmount","AMOUNT",10.5,true,false))return v;
    if(auto* v=label("LabelSpeed","SPEED",9.5,true,true))return v;
    if(auto* v=label("LabelIps","ips",8.8,false,true))return v;
    if(auto* v=label("LabelStability","STABILITY",8.8,true,false))return v;
    if(auto* v=label("LabelHiss","HISS",8.8,true,false))return v;
    if(auto* v=label("LabelGlueAmount","AMOUNT",10.5,true,false))return v;
    if(auto* v=label("LabelResponse","RESPONSE",9.0,true,false))return v;
    if(auto* v=label("LabelColor","COLOR",10.5,true,false))return v;
    if(auto* v=label("LabelWear","WEAR",8.8,true,false))return v;
    if(auto* v=label("LabelSurface","SURFACE",8.8,true,false))return v;
    if(auto* v=label("LabelDepth","DEPTH",9.5,true,false))return v;
    if(auto* v=label("LabelWidth","WIDTH",9.5,true,false))return v;
    if(auto* v=label("LabelLowMono","LOW MONO",9.0,true,false))return v;
    if(auto* v=label("LabelFixed120","FIXED 120 Hz",8.6,false,true))return v;
    if(auto* v=label("LabelOutputGain","OUTPUT GAIN",10.5,true,false))return v;
    if(auto* v=label("LabelLevelMatch","LEVEL MATCH",9.5,true,true))return v;

    return nullptr;
}

} // namespace MixEngine
