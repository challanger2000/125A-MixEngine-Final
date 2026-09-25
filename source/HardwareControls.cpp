#include "HardwareControls.h"
#include "pluginids.h"
#include "metering.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "branding_master.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <utility>
#include <string_view>
#include <vector>

namespace MixEngine {
namespace {
constexpr VSTGUI::CColor kLogoSilver {217,217,217,255};
constexpr VSTGUI::CColor kLogoRed {215,25,32,255};
constexpr VSTGUI::CColor kNeedleHub {46,48,53,255};
constexpr double kPi=3.14159265358979323846;
constexpr VSTGUI::CColor kAccentGold {168,136,82,255};
constexpr VSTGUI::CColor kAccentRed {205,42,46,255};

void fillRoundGradient(VSTGUI::CDrawContext* context,const VSTGUI::CRect& rect,double radius,
                       const VSTGUI::CColor& top,const VSTGUI::CColor& bottom)
{
    auto* path=context->createRoundRectGraphicsPath(rect,radius);
    if(!path) return;
    auto* gradient=VSTGUI::CGradient::create(0.0,1.0,top,bottom);
    if(gradient) {
        context->fillLinearGradient(path,*gradient,{rect.left,rect.top},{rect.left,rect.bottom});
        gradient->forget();
    }
    path->forget();
}

void strokeRound(VSTGUI::CDrawContext* context,const VSTGUI::CRect& rect,double radius,
                 const VSTGUI::CColor& color,double width)
{
    auto* path=context->createRoundRectGraphicsPath(rect,radius);
    if(!path) return;
    context->setFrameColor(color);
    context->setLineWidth(width);
    context->drawGraphicsPath(path,VSTGUI::CDrawContext::kPathStroked);
    path->forget();
}

void fillRadialEllipse(VSTGUI::CDrawContext* context,const VSTGUI::CRect& rect,
                       const VSTGUI::CColor& inner,const VSTGUI::CColor& outer,
                       const VSTGUI::CPoint& highlightOffset={0.0,0.0})
{
    auto* path=context->createGraphicsPath();
    if(!path) return;
    path->addEllipse(rect);
    auto* gradient=VSTGUI::CGradient::create(0.0,1.0,inner,outer);
    if(gradient) {
        const auto radius=std::max(rect.getWidth(),rect.getHeight())*0.52;
        context->fillRadialGradient(path,*gradient,rect.getCenter(),radius,highlightOffset);
        gradient->forget();
    }
    path->forget();
}

struct LogoSubpath {
    std::vector<VSTGUI::CPoint> points;
};

struct LogoPath {
    std::vector<LogoSubpath> subpaths;
    bool red {false};
};


std::vector<LogoPath> parseMasterLogo()
{
    // CMake extracts these path strings directly from the authoritative SVG.
    // Each source entry is one original SVG path, avoiding any raster or redraw.
    std::vector<LogoPath> result;
    result.reserve(Branding::kMasterPathCount);

    for(const auto& source:Branding::kMasterPaths) {
        const std::string_view d {source.d};
        LogoPath path;
        path.red=source.red;

        const char* p=d.data();
        const char* end=d.data()+d.size();
        char command=0;
        LogoSubpath* current=nullptr;

        while(p<end) {
            while(p<end&&(std::isspace(static_cast<unsigned char>(*p))||*p==',')) ++p;
            if(p>=end) break;
            if(std::isalpha(static_cast<unsigned char>(*p))) {
                command=*p++;
                if(command=='Z'||command=='z') {
                    command=0;
                    current=nullptr;
                    continue;
                }
            }
            if(command!='M'&&command!='m'&&command!='L'&&command!='l') {
                ++p;
                continue;
            }

            char* next=nullptr;
            const double x=std::strtod(p,&next);
            if(next==p||next>end) break;
            p=next;
            while(p<end&&(std::isspace(static_cast<unsigned char>(*p))||*p==',')) ++p;
            const double y=std::strtod(p,&next);
            if(next==p||next>end) break;
            p=next;

            if(command=='M'||command=='m') {
                path.subpaths.emplace_back();
                current=&path.subpaths.back();
                current->points.emplace_back(x,y);
                command=(command=='M')?'L':'l';
            } else if(current) {
                current->points.emplace_back(x,y);
            }
        }

        if(!path.subpaths.empty()) result.emplace_back(std::move(path));
    }
    return result;
}


std::string formatKnobValue(int32_t tag,double normalized)
{
    normalized=std::clamp(normalized,0.0,1.0);
    char text[32] {};
    switch(tag) {
        case kParamInput:
        case kParamOutput: {
            const double db=-12.0+24.0*normalized;
            std::snprintf(text,sizeof(text),"%+.1f dB",db);
            break;
        }
        case kParamDepth: {
            const int value=static_cast<int>(std::lround(-100.0+200.0*normalized));
            std::snprintf(text,sizeof(text),"%+d%%",value);
            break;
        }
        case kParamWidth: {
            const int value=static_cast<int>(std::lround(200.0*normalized));
            std::snprintf(text,sizeof(text),"%d%%",value);
            break;
        }
        default: {
            const int value=static_cast<int>(std::lround(100.0*normalized));
            std::snprintf(text,sizeof(text),"%d%%",value);
            break;
        }
    }
    return text;
}

} // anonymous namespace

HardwareFaceplate::HardwareFaceplate(const VSTGUI::CRect& size)
: VSTGUI::CView(size)
{
    setMouseEnabled(false);
}

HardwareFaceplate::HardwareFaceplate(const HardwareFaceplate& other)
: VSTGUI::CView(other)
{
}

void HardwareFaceplate::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    const double ox=r.left, oy=r.top;
    const auto rect=[&](double x,double y,double w,double h){return VSTGUI::CRect(ox+x,oy+y,ox+x+w,oy+y+h);};
    const auto line=[&](double x1,double y1,double x2,double y2,const VSTGUI::CColor& c,double w=1.0){
        context->setFrameColor(c); context->setLineWidth(w);
        context->drawLine({ox+x1,oy+y1},{ox+x2,oy+y2});
    };
    const auto raised=[&](double x,double y,double w,double h,double radius){
        const auto rr=rect(x,y,w,h);
        fillRoundGradient(context,rr,radius,{35,45,57,255},{15,22,30,255});
        strokeRound(context,rr,radius,{83,105,126,150},1.0);
        auto hi=rr; hi.inset(2.0,2.0); hi.bottom=hi.top+1.0;
        context->setFillColor({255,255,255,16});
        context->drawRect(hi,VSTGUI::kDrawFilled);
    };
    const auto well=[&](double x,double y,double w,double h,double radius=7.0){
        const auto shadow=rect(x+1,y+2,w,h);
        fillRoundGradient(context,shadow,radius,{4,5,7,185},{0,0,0,225});
        const auto rr=rect(x,y,w,h);
        fillRoundGradient(context,rr,radius,{8,13,19,255},{18,27,37,255});
        strokeRound(context,rr,radius,{65,88,108,145},1.0);
    };
    const auto screw=[&](double x,double y){
        const auto sr=rect(x-4.5,y-4.5,9,9);
        fillRadialEllipse(context,sr,{104,109,116,255},{20,22,26,255},{-2.0,-2.0});
        context->setFrameColor({4,5,7,255}); context->setLineWidth(1.0);
        context->drawEllipse(sr,VSTGUI::kDrawStroked);
        line(x-2.2,y,x+2.2,y,{6,7,9,230},1.0);
    };

    context->setDrawMode(VSTGUI::kAntiAliasing);

    // Main 125A chassis: one continuous instrument instead of eight boxed cells.
    context->setFillColor({4,8,12,255});
    context->drawRect(r,VSTGUI::kDrawFilled);
    auto chassis=r; chassis.inset(7.0,7.0);
    fillRoundGradient(context,chassis,16.0,{39,54,68,255},{13,22,31,255});
    strokeRound(context,chassis,16.0,{1,4,7,255},2.0);
    auto inner=chassis; inner.inset(4.0,4.0);
    strokeRound(context,inner,13.0,{126,157,181,42},1.0);

    // Restrained anodised grain: enough material cue without visual shimmer.
    for(int y=18;y<786;y+=8) {
        const uint8_t a=(y%32==0)?7:3;
        line(14,y,1426,y,{168,192,210,a},1.0);
    }
    line(20,14,1420,14,{205,225,239,34},1.0);
    line(20,786,1420,786,{0,0,0,210},1.0);

    // Upper bridge: logo, two recessed meters, centre utility strip and master block.
    raised(20,26,194,234,12.0);
    raised(224,26,992,234,14.0);
    raised(1226,26,194,234,12.0);

    well(232,44,372,210,10.0);
    well(836,44,372,210,10.0);
    well(630,92,180,66,9.0);
    well(1234,104,178,58,9.0);

    // A restrained brand datum ties the meter bridge together without glowing at the user.
    line(226,267,1214,267,{205,42,46,66},1.25);
    line(20,284,1420,284,{144,150,159,25},1.0);
    line(20,287,1420,287,{0,0,0,215},2.0);

    // Lower console: distinct hardware modules seated into one common chassis.
    raised(20,300,152,464,12.0);      // input
    raised(1068,300,166,464,12.0);    // stereo
    raised(1246,300,174,464,12.0);    // output

    // Five independent analogue daughter-panels. They share one blue-anodised
    // chassis, but each stage has its own recessed edge and subtle material tint.
    // This is intentionally independent from the knob artwork so future rendered
    // knob strips can be dropped in without redesigning the faceplate again.
    struct ModulePlate { double x; VSTGUI::CColor top; VSTGUI::CColor bottom; };
    constexpr ModulePlate modulePlates[] {
        {184.0,{42,55,68,255},{18,27,36,255}},   // Console
        {360.0,{46,55,66,255},{20,27,35,255}},   // Tube
        {536.0,{39,52,65,255},{17,26,35,255}},   // Tape
        {712.0,{38,50,62,255},{16,24,32,255}},   // Glue
        {888.0,{43,52,64,255},{18,25,33,255}}    // Vinyl
    };
    for(const auto& plate : modulePlates) {
        const auto shadow=rect(plate.x+2.0,303.0,168.0,464.0);
        fillRoundGradient(context,shadow,10.0,{1,3,5,150},{0,0,0,230});

        const auto panel=rect(plate.x,300.0,168.0,464.0);
        fillRoundGradient(context,panel,10.0,plate.top,plate.bottom);
        strokeRound(context,panel,10.0,{90,114,136,145},1.0);

        VSTGUI::CRect moduleInset=rect(plate.x+6.0,306.0,156.0,452.0);
        strokeRound(context,moduleInset,8.0,{4,10,15,220},1.0);
        VSTGUI::CRect moduleHighlight=moduleInset;
        moduleHighlight.inset(2.0,2.0);
        moduleHighlight.bottom=moduleHighlight.top+1.0;
        context->setFillColor({218,235,246,22});
        context->drawRect(moduleHighlight,VSTGUI::kDrawFilled);

        // Narrow header rail makes module boundaries obvious before the eye
        // reaches the controls, without turning the interface into coloured boxes.
        VSTGUI::CRect header=rect(plate.x+8.0,307.0,152.0,35.0);
        fillRoundGradient(context,header,5.0,{51,67,82,145},{25,36,47,75});
        strokeRound(context,header,5.0,{118,146,169,38},1.0);
    }

    // I/O and stereo sections use the same material family but remain visually
    // broader structural bays rather than analogue daughter modules.
    for(const auto& bay : {VSTGUI::CRect{26,306,166,758},
                           VSTGUI::CRect{1074,306,1228,758},
                           VSTGUI::CRect{1252,306,1414,758}}) {
        strokeRound(context,bay,8.0,{101,129,151,34},1.0);
    }

    // Engraved datum line under every title row.
    for(double x : {188.0,364.0,540.0,716.0,892.0})
        line(x,344,x+160.0,344,{151,181,203,58},1.0);
    line(24,344,168,344,{145,174,196,44},1.0);
    line(1072,344,1230,344,{145,174,196,44},1.0);
    line(1250,344,1416,344,{145,174,196,44},1.0);

    // Recesses under discrete selectors keep controls seated in the metal.
    well(34,586,124,42,7.0);
    well(190,574,164,44,7.0);
    well(374,574,148,44,7.0);
    well(550,574,148,44,7.0);

    // Flagship restraint: fasteners are structural accents, not repeating decoration.
    for(auto p : {VSTGUI::CPoint{18,18},VSTGUI::CPoint{1422,18},
                  VSTGUI::CPoint{18,782},VSTGUI::CPoint{1422,782},
                  VSTGUI::CPoint{196,314},VSTGUI::CPoint{1044,314},
                  VSTGUI::CPoint{196,750},VSTGUI::CPoint{1044,750}})
        screw(p.x,p.y);

    // Quiet hierarchy labels are part of the chassis, not additional controls.
    context->setFont(VSTGUI::kNormalFont,8.8,0);
    context->setFontColor({142,148,156,225});
    context->drawString(VSTGUI::UTF8String("v3.0.0"),rect(1260,74,128,12),VSTGUI::kCenterText);

    setDirty(false);
}

HardwareLogo::HardwareLogo(const VSTGUI::CRect& size)
: VSTGUI::CView(size)
{
    setMouseEnabled(false);
}

HardwareLogo::HardwareLogo(const HardwareLogo& other)
: VSTGUI::CView(other)
{
}

void HardwareLogo::draw(VSTGUI::CDrawContext* context)
{
    static const auto logo=parseMasterLogo();
    const auto r=getViewSize();
    constexpr double masterWidth=1774.0;
    constexpr double masterHeight=887.0;
    const double scale=std::min(r.getWidth()/masterWidth,r.getHeight()/masterHeight);
    const double x0=r.left+(r.getWidth()-masterWidth*scale)*0.5;
    const double y0=r.top+(r.getHeight()-masterHeight*scale)*0.5;

    context->setDrawMode(VSTGUI::kAntiAliasing);
    for(const auto& sourcePath:logo) {
        auto* path=context->createGraphicsPath();
        if(!path) continue;
        for(const auto& subpath:sourcePath.subpaths) {
            if(subpath.points.empty()) continue;
            const auto toView=[&](const VSTGUI::CPoint& p) {
                return VSTGUI::CPoint{x0+p.x*scale,y0+p.y*scale};
            };
            path->beginSubpath(toView(subpath.points.front()));
            for(std::size_t i=1;i<subpath.points.size();++i)
                path->addLine(toView(subpath.points[i]));
            path->closeSubpath();
        }
        context->setFillColor(sourcePath.red?kLogoRed:kLogoSilver);
        context->drawGraphicsPath(path,VSTGUI::CDrawContext::kPathFilledEvenOdd);
        path->forget();
    }
    setDirty(false);
}

HardwareKnob::HardwareKnob(const VSTGUI::CRect& size,VSTGUI::IControlListener* listener,int32_t tag,Style style,VSTGUI::CBitmap* filmstrip)
: VSTGUI::CAnimKnob(size,listener,tag,nullptr),style_(style),filmstrip_(filmstrip)
{
    if(filmstrip_) filmstrip_->remember();
    setStartAngle(static_cast<float>(135.0/180.0*kPi));
    setRangeAngle(static_cast<float>(270.0/180.0*kPi));
    setTransparency(true);
    setWantsFocus(true);
}

HardwareKnob::HardwareKnob(const HardwareKnob& other)
: VSTGUI::CAnimKnob(other),style_(other.style_),filmstrip_(other.filmstrip_)
{
    if(filmstrip_) filmstrip_->remember();
}

HardwareKnob::~HardwareKnob()
{
    if(filmstrip_) filmstrip_->forget();
}

void HardwareKnob::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    const auto v=std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0);
    const auto cx=r.getCenter().x, cy=r.getCenter().y;
    const double radius=std::min(r.getWidth(),r.getHeight())*0.40;
    const double angle=(135.0+270.0*v)*kPi/180.0;

    context->setDrawMode(VSTGUI::kAntiAliasing);

    // Production path: 16x8 atlas prepared from the approved 384 px master.
    // Keeping each GPU bitmap below common texture limits avoids the oversized
    // vertical-filmstrip failure while preserving enough source resolution for HiDPI.
    if(filmstrip_ && filmstrip_->isLoaded()) {
        constexpr int kFrames=128;
        constexpr int kColumns=16;
        const double kSourceFrame=style_==Style::Large?128.0:(style_==Style::Medium?96.0:64.0);
        const int frame=std::clamp(static_cast<int>(std::lround(v*static_cast<double>(kFrames-1))),0,kFrames-1);

        // Secondary controls get a restrained vector scale. It stays sharp at every
        // editor zoom and deliberately has no numbers, leaving Vernier knobs dominant.
        if(style_!=Style::Large) {
            constexpr int kTicks=9;
            const double outer=std::min(r.getWidth(),r.getHeight())*0.48;
            const double inner=outer-(style_==Style::Small?4.5:6.0);
            for(int i=0;i<kTicks;++i) {
                const double t=static_cast<double>(i)/static_cast<double>(kTicks-1);
                const double a=(135.0+270.0*t)*kPi/180.0;
                const bool datum=(i==0||i==kTicks-1||i==kTicks/2);
                context->setFrameColor(datum?VSTGUI::CColor{210,216,220,175}:VSTGUI::CColor{142,153,162,120});
                context->setLineWidth(datum?1.25:0.9);
                context->drawLine({cx+std::cos(a)*inner,cy+std::sin(a)*inner},
                                  {cx+std::cos(a)*outer,cy+std::sin(a)*outer});
            }
        }

        const int col=frame%kColumns;
        const int row=frame/kColumns;
        const VSTGUI::CRect src(kSourceFrame*col,kSourceFrame*row,
                                kSourceFrame*(col+1),kSourceFrame*(row+1));
        context->fillRectWithBitmap(filmstrip_,src,r,1.f);

        if(isEditing()) {
            const auto valueText=formatKnobValue(getTag(),v);
            const double badgeW=(style_==Style::Small)?44.0:58.0;
            const double badgeH=(style_==Style::Small)?16.0:19.0;
            VSTGUI::CRect badge(cx-badgeW*.5,cy-badgeH*.5,cx+badgeW*.5,cy+badgeH*.5);
            VSTGUI::CRect badgeShadow=badge; badgeShadow.offset(0.0,1.5);
            fillRoundGradient(context,badgeShadow,badgeH*.38,{2,3,4,205},{0,0,0,235});
            fillRoundGradient(context,badge,badgeH*.38,{39,43,49,248},{12,15,18,248});
            strokeRound(context,badge,badgeH*.38,{104,143,171,190},1.0);
            context->setFont(VSTGUI::kNormalFont,style_==Style::Small?7.2:8.6,VSTGUI::kBoldFace);
            context->setFontColor({232,242,248,255});
            context->drawString(VSTGUI::UTF8String(valueText.c_str()),badge,VSTGUI::kCenterText);
        }
        setDirty(false);
        return;
    }

    // Vector fallback remains available if a resource is missing.
    // Soft contact shadow gives the control real separation from the panel.
    context->setFillColor({0,0,0,105});
    context->drawEllipse({cx-radius-7,cy-radius-3,cx+radius+7,cy+radius+10},VSTGUI::kDrawFilled);

    // Substantial machined skirt: broad satin metal, dark recessed shoulder.
    const VSTGUI::CRect skirt(cx-radius-5,cy-radius-5,cx+radius+5,cy+radius+5);
    fillRadialEllipse(context,skirt,{150,154,160,255},{34,38,44,255},{-radius*.30,-radius*.32});
    context->setFrameColor({4,5,7,255}); context->setLineWidth(1.5);
    context->drawEllipse(skirt,VSTGUI::kDrawStroked);

    VSTGUI::CRect skirtInner=skirt; skirtInner.inset(5.5,5.5);
    fillRadialEllipse(context,skirtInner,{75,79,86,255},{19,22,27,255},{-radius*.24,-radius*.28});
    context->setFrameColor({214,218,224,42}); context->setLineWidth(1.0);
    context->drawEllipse(skirtInner,VSTGUI::kDrawStroked);

    // Scale ticks float around the metal skirt.
    const int ticks=(style_==Style::Small)?9:13;
    for(int i=0;i<ticks;++i){
        const double t=static_cast<double>(i)/(ticks-1);
        const double a=(135.0+270.0*t)*kPi/180.0;
        const double ro=radius+10.0, ri=radius+5.0;
        const bool datum=(i==0||i==ticks-1||i==(ticks-1)/2);
        context->setFrameColor(datum?VSTGUI::CColor{226,221,209,205}:VSTGUI::CColor{164,169,177,135});
        context->setLineWidth(datum?1.6:1.0);
        context->drawLine({cx+std::cos(a)*ri,cy+std::sin(a)*ri},
                          {cx+std::cos(a)*ro,cy+std::sin(a)*ro});
    }

    // Sparse grip notches: enough tactile cue without the previous spiky look.
    const int gripMarks=(style_==Style::Small)?8:12;
    for(int i=0;i<gripMarks;++i){
        const double a=2.0*kPi*static_cast<double>(i)/static_cast<double>(gripMarks);
        const double r1=radius*0.89, r2=radius*0.97;
        context->setFrameColor({216,220,225,46});
        context->setLineWidth(1.0);
        context->drawLine({cx+std::cos(a)*r1,cy+std::sin(a)*r1},
                          {cx+std::cos(a)*r2,cy+std::sin(a)*r2});
    }

    // Module-specific metal nuance: one instrument, but distinct analogue stages.
    VSTGUI::CColor ringColor {176,145,89,195}; // default champagne
    if(style_!=Style::Small) {
        switch(getTag()) {
            case kParamConsoleDrive:    ringColor={171,145,96,198}; break;  // aged console brass
            case kParamTubeAmount:      ringColor={184,128,74,198}; break;  // warm valve amber
            case kParamTapeAmount:      ringColor={162,119,75,198}; break;  // bronze transport
            case kParamGlueAmount:      ringColor={142,151,160,190}; break; // cool compressor steel
            case kParamVinylCharacter:  ringColor={166,107,82,194}; break;  // muted copper
            case kParamInput:
            case kParamOutput:          ringColor={158,163,169,190}; break; // neutral I/O metal
            default: break;
        }
    }
    const double ring=radius*0.73;
    context->setFrameColor(ringColor); context->setLineWidth(1.5);
    context->drawEllipse({cx-ring,cy-ring,cx+ring,cy+ring},VSTGUI::kDrawStroked);

    const double cap=radius*0.70;
    const VSTGUI::CRect capRect(cx-cap,cy-cap,cx+cap,cy+cap);
    fillRadialEllipse(context,capRect,
                      style_==Style::Small?VSTGUI::CColor{58,61,67,255}:VSTGUI::CColor{66,69,75,255},
                      {10,12,15,255},{-cap*.32,-cap*.35});
    context->setFrameColor({3,4,5,255}); context->setLineWidth(1.0);
    context->drawEllipse(capRect,VSTGUI::kDrawStroked);
    VSTGUI::CRect capShoulder=capRect; capShoulder.inset(cap*.13,cap*.13);
    context->setFrameColor({202,205,210,24}); context->setLineWidth(0.9);
    context->drawEllipse(capShoulder,VSTGUI::kDrawStroked);

    // A restrained specular crescent prevents the cap from reading as a flat circle.
    context->setFrameColor({255,255,255,42}); context->setLineWidth(1.2);
    context->drawLine({cx-cap*.48,cy-cap*.40},{cx+cap*.18,cy-cap*.57});

    // Brand-red index with an ivory centre catches the eye without colouring the whole UI.
    const double p1=cap*0.18,p2=radius*0.82;
    context->setFrameColor(kAccentRed); context->setLineWidth(style_==Style::Small?2.0:2.5);
    context->drawLine({cx+std::cos(angle)*p1,cy+std::sin(angle)*p1},
                      {cx+std::cos(angle)*p2,cy+std::sin(angle)*p2});
    const auto tx=cx+std::cos(angle)*p2, ty=cy+std::sin(angle)*p2;
    context->setFillColor({242,231,205,255});
    context->drawEllipse({tx-2.1,ty-2.1,tx+2.1,ty+2.1},VSTGUI::kDrawFilled);

    context->setFillColor({12,14,17,255}); context->setFrameColor({121,126,134,180});
    context->drawEllipse({cx-4,cy-4,cx+4,cy+4},VSTGUI::kDrawFilledAndStroked);

    // While the user is editing, temporarily replace visual ambiguity with an exact value.
    // The badge disappears again as soon as the edit gesture ends.
    if(isEditing()) {
        const auto valueText=formatKnobValue(getTag(),v);
        const double badgeW=(style_==Style::Small)?44.0:58.0;
        const double badgeH=(style_==Style::Small)?16.0:19.0;
        VSTGUI::CRect badge(cx-badgeW*.5,cy-badgeH*.5,cx+badgeW*.5,cy+badgeH*.5);
        VSTGUI::CRect badgeShadow=badge; badgeShadow.offset(0.0,1.5);
        fillRoundGradient(context,badgeShadow,badgeH*.38,{2,3,4,205},{0,0,0,235});
        fillRoundGradient(context,badge,badgeH*.38,{39,43,49,248},{12,15,18,248});
        strokeRound(context,badge,badgeH*.38,{188,153,87,185},1.0);
        context->setFont(VSTGUI::kNormalFont,style_==Style::Small?7.2:8.6,VSTGUI::kBoldFace);
        context->setFontColor({245,239,222,255});
        context->drawString(VSTGUI::UTF8String(valueText.c_str()),badge,VSTGUI::kCenterText);
    }

    setDirty(false);
}

HardwareToggle::HardwareToggle(const VSTGUI::CRect& size,VSTGUI::IControlListener* listener,int32_t tag,VSTGUI::CBitmap* filmstrip,bool ledLeft,bool moduleLedAbove,bool blueLed)
: VSTGUI::COnOffButton(size,listener,tag,nullptr),filmstrip_(filmstrip),ledLeft_(ledLeft),moduleLedAbove_(moduleLedAbove),blueLed_(blueLed)
{
    if(filmstrip_) filmstrip_->remember();
    setTransparency(true);
    setWantsFocus(true);
}

HardwareToggle::HardwareToggle(const HardwareToggle& other)
: VSTGUI::COnOffButton(other),filmstrip_(other.filmstrip_),ledLeft_(other.ledLeft_),moduleLedAbove_(other.moduleLedAbove_),blueLed_(other.blueLed_)
{
    if(filmstrip_) filmstrip_->remember();
}

HardwareToggle::~HardwareToggle()
{
    if(filmstrip_) filmstrip_->forget();
}

void HardwareToggle::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    const bool on=getValueNormalized()>=0.5;
    context->setDrawMode(VSTGUI::kAntiAliasing);

    const auto ledColor=blueLed_?VSTGUI::CColor{90,139,255,255}:VSTGUI::CColor{86,224,125,255};
    const auto ledCore=blueLed_?VSTGUI::CColor{182,205,255,255}:VSTGUI::CColor{193,255,205,255};
    const auto ledOff=blueLed_?VSTGUI::CColor{20,29,48,255}:VSTGUI::CColor{19,42,27,255};

    auto drawLed=[&](double x,double y,double d){
        const VSTGUI::CRect lr(x,y,x+d,y+d);
        if(on) {
            const auto c=lr.getCenter();
            for(int n=2;n>=1;--n) {
                const double grow=n*2.6;
                context->setFillColor(blueLed_?VSTGUI::CColor{72,118,255,static_cast<uint8_t>(13*n)}
                                                   :VSTGUI::CColor{68,220,104,static_cast<uint8_t>(12*n)});
                context->drawEllipse({lr.left-grow,lr.top-grow,lr.right+grow,lr.bottom+grow},VSTGUI::kDrawFilled);
            }
        }
        fillRadialEllipse(context,lr,on?ledCore:ledOff,on?ledColor:VSTGUI::CColor{5,9,7,255},{-d*.18,-d*.18});
        context->setFrameColor({4,5,7,255}); context->setLineWidth(1.0);
        context->drawEllipse(lr,VSTGUI::kDrawStroked);
    };

    // Style-prototype path: use the current Knob Designer push-button filmstrip,
    // while retaining the existing 125A LED behaviour as a separate overlay.
    if(filmstrip_ && filmstrip_->isLoaded()) {
        constexpr int kStates=3;
        constexpr double kFrameSize=64.0;
        const int frame=on?2:0;
        filmstrip_->draw(context,r,{0.0,kFrameSize*static_cast<double>(frame)},1.f);

        // Subtle blue-gunmetal glaze ties the existing button artwork to the new
        // Vernier/Gunmetal control language without throwing the asset away.
        const auto cc=r.getCenter();
        VSTGUI::CRect cap;
        if(moduleLedAbove_)
            cap={cc.x-22.0,r.top+24.0,cc.x+22.0,r.top+50.0};
        else
            cap={cc.x-22.0,cc.y-11.0,cc.x+22.0,cc.y+11.0};
        fillRoundGradient(context,cap,4.5,
                          on?VSTGUI::CColor{54,67,78,72}:VSTGUI::CColor{28,40,50,78},
                          on?VSTGUI::CColor{11,17,23,76}:VSTGUI::CColor{6,10,14,92});
        strokeRound(context,cap,4.5,on?VSTGUI::CColor{137,158,174,82}:VSTGUI::CColor{93,112,128,66},1.0);

        if(moduleLedAbove_) {
            const auto cx=r.getCenter().x;
            drawLed(cx-5.0,r.top+3.0,10.0);
        } else if(ledLeft_) {
            drawLed(r.left+4.0,r.getCenter().y-5.0,10.0);
        } else {
            drawLed(r.right-14.0,r.getCenter().y-5.0,10.0);
        }
        setDirty(false);
        return;
    }

    auto drawPush=[&](const VSTGUI::CRect& sw){
        // Metal bezel + inset latching cap: visually closer to a studio hardware switch.
        VSTGUI::CRect bezel=sw; bezel.inset(-2.0,-2.0);
        VSTGUI::CRect shadow=bezel; shadow.offset(0.0,2.5);
        fillRoundGradient(context,shadow,6.5,{2,3,4,210},{0,0,0,245});
        fillRoundGradient(context,bezel,6.5,{96,101,109,255},{25,28,33,255});
        strokeRound(context,bezel,6.5,{5,6,8,255},1.0);

        VSTGUI::CRect cap=sw;
        if(on) cap.offset(0.0,1.2);
        fillRoundGradient(context,cap,5.0,
                          on?VSTGUI::CColor{65,69,76,255}:VSTGUI::CColor{52,56,62,255},
                          on?VSTGUI::CColor{22,25,30,255}:VSTGUI::CColor{16,19,23,255});
        strokeRound(context,cap,5.0,on?VSTGUI::CColor{153,159,168,175}:VSTGUI::CColor{103,109,118,160},1.0);
        VSTGUI::CRect inset=cap; inset.inset(4.0,4.0);
        strokeRound(context,inset,3.5,{255,255,255,static_cast<uint8_t>(on?24:14)},1.0);

        const auto cc=cap.getCenter();
        context->setFrameColor(on?kAccentGold:VSTGUI::CColor{88,93,100,165});
        context->setLineWidth(1.5);
        context->drawLine({cc.x-5.5,cc.y},{cc.x+5.5,cc.y});
    };

    if(moduleLedAbove_){
        const auto cx=r.getCenter().x;
        drawLed(cx-6.0,r.top+1.0,12.0);
        drawPush({cx-23.0,r.top+24.0,cx+23.0,r.top+51.0});
    }else{
        constexpr double swW=62.0,swH=34.0,ledD=13.0,gap=10.0;
        const double group=swW+gap+ledD;
        const double left=r.getCenter().x-group*.5;
        const double swX=ledLeft_?left+ledD+gap:left;
        const double ledX=ledLeft_?left:left+swW+gap;
        drawPush({swX,r.getCenter().y-swH*.5,swX+swW,r.getCenter().y+swH*.5});
        drawLed(ledX,r.getCenter().y-ledD*.5,ledD);
    }
    setDirty(false);
}

HardwareSelector::HardwareSelector(const VSTGUI::CRect& size,VSTGUI::IControlListener* listener,int32_t tag,std::vector<std::string> labels)
: VSTGUI::CControl(size,listener,tag,nullptr),labels_(std::move(labels))
{
    setTransparency(true);
    setWantsFocus(true);
}

HardwareSelector::HardwareSelector(const HardwareSelector& other)
: VSTGUI::CControl(other),labels_(other.labels_)
{
}

void HardwareSelector::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    if(labels_.empty()) {
        setDirty(false);
        return;
    }

    context->setDrawMode(VSTGUI::kAntiAliasing);

    VSTGUI::CRect shadow=r; shadow.offset(0.0,2.0);
    fillRoundGradient(context,shadow,7.0,{2,3,5,200},{0,0,0,235});
    fillRoundGradient(context,r,7.0,{75,91,105,255},{20,29,38,255});
    strokeRound(context,r,7.0,{4,8,12,255},1.0);

    VSTGUI::CRect inner=r;
    inner.inset(3.5,3.5);
    fillRoundGradient(context,inner,5.0,{13,21,29,255},{7,12,18,255});
    strokeRound(context,inner,5.0,{105,134,157,92},1.0);
    const auto count=static_cast<int>(labels_.size());
    const auto selected=std::clamp(
        static_cast<int>(std::lround(getValueNormalized()*static_cast<double>(std::max(1,count-1)))),
        0,count-1);
    const double segW=inner.getWidth()/static_cast<double>(count);

    double fontSize=count>=4?8.2:9.4;
    while(fontSize>6.6) {
        context->setFont(VSTGUI::kNormalFont,fontSize,VSTGUI::kBoldFace);
        bool fits=true;
        for(const auto& label:labels_) {
            if(context->getStringWidth(label.c_str())>segW-5.0) { fits=false; break; }
        }
        if(fits) break;
        fontSize-=0.35;
    }
    context->setFont(VSTGUI::kNormalFont,fontSize,VSTGUI::kBoldFace);

    for(int i=0;i<count;++i) {
        VSTGUI::CRect seg(inner.left+i*segW,inner.top,
                          i==count-1?inner.right:inner.left+(i+1)*segW,inner.bottom);
        VSTGUI::CRect face=seg;
        face.inset(1.2,1.0);

        if(i==selected) {
            VSTGUI::CRect selectedShadow=face; selectedShadow.offset(0.0,1.3);
            fillRoundGradient(context,selectedShadow,4.0,{3,4,5,180},{0,0,0,220});
            fillRoundGradient(context,face,4.0,{55,84,106,255},{25,48,66,255});
            strokeRound(context,face,4.0,{118,168,201,190},1.0);
            VSTGUI::CRect glint=face; glint.inset(2.0,2.0); glint.bottom=glint.top+1.0;
            context->setFillColor({218,240,255,42});
            context->drawRect(glint,VSTGUI::kDrawFilled);
        } else {
            fillRoundGradient(context,face,4.0,{38,49,59,255},{18,26,34,255});
            strokeRound(context,face,4.0,{69,91,108,175},1.0);
        }

        if(i>0) {
            context->setFrameColor({0,0,0,145});
            context->setLineWidth(1.0);
            context->drawLine({seg.left,inner.top+5.0},{seg.left,inner.bottom-5.0});
        }

        context->setFontColor(i==selected?VSTGUI::CColor{226,241,250,255}:VSTGUI::CColor{177,194,207,245});
        context->drawString(VSTGUI::UTF8String(labels_[i].c_str()),face,VSTGUI::kCenterText);
    }

    setDirty(false);
}

VSTGUI::CMouseEventResult HardwareSelector::onMouseDown(VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons)
{
    if(!buttons.isLeftButton()||labels_.empty())
        return VSTGUI::kMouseEventNotHandled;

    const auto r=getViewSize();
    if(!r.pointInside(where))
        return VSTGUI::kMouseEventNotHandled;

    const auto count=static_cast<int>(labels_.size());
    const double normalized=(where.x-r.left)/std::max(1.0,r.getWidth());
    const int index=std::clamp(static_cast<int>(normalized*count),0,count-1);
    const float value=count<=1?0.f:static_cast<float>(index)/static_cast<float>(count-1);

    beginEdit();
    setValueNormalized(value);
    valueChanged();
    endEdit();
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

HardwareLabel::HardwareLabel(const VSTGUI::CRect& size,std::string text,double fontSize,bool bold,bool muted)
: VSTGUI::CView(size),text_(std::move(text)),fontSize_(fontSize),bold_(bold),muted_(muted)
{
    setMouseEnabled(false);
    setTransparency(true);
}

HardwareLabel::HardwareLabel(const HardwareLabel& other)
: VSTGUI::CView(other),text_(other.text_),fontSize_(other.fontSize_),bold_(other.bold_),muted_(other.muted_)
{
}

void HardwareLabel::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    context->setDrawMode(VSTGUI::kAntiAliasing);
    context->setFont(VSTGUI::kNormalFont,fontSize_,bold_?VSTGUI::kBoldFace:0);

    // Engraved/silk-screen hierarchy: bright titles, quieter engineering legends.
    VSTGUI::CRect shadow=r;
    shadow.offset(0.0,1.0);
    context->setFontColor({0,0,0,145});
    context->drawString(VSTGUI::UTF8String(text_.c_str()),shadow,VSTGUI::kCenterText);

    context->setFontColor(muted_?VSTGUI::CColor{151,158,168,245}:VSTGUI::CColor{218,217,211,250});
    context->drawString(VSTGUI::UTF8String(text_.c_str()),r,VSTGUI::kCenterText);
    setDirty(false);
}

HardwareVUMeter::HardwareVUMeter(const VSTGUI::CRect& size,VSTGUI::IControlListener* listener,int32_t tag,VSTGUI::CBitmap* face,VSTGUI::CBitmap* cover)
: VSTGUI::CControl(size,listener,tag,nullptr),filmstrip_(face),cover_(cover)
{
    if(filmstrip_) filmstrip_->remember();
    if(cover_) cover_->remember();
    setTransparency(true);
    setMouseEnabled(false);
}

HardwareVUMeter::HardwareVUMeter(const HardwareVUMeter& other)
: VSTGUI::CControl(other),filmstrip_(other.filmstrip_),cover_(other.cover_)
{
    if(filmstrip_) filmstrip_->remember();
    if(cover_) cover_->remember();
}

HardwareVUMeter::~HardwareVUMeter()
{
    if(filmstrip_) filmstrip_->forget();
    if(cover_) cover_->forget();
}

HardwareUIScale::HardwareUIScale(const VSTGUI::CRect& size,VSTGUI::VST3Editor* editor)
: VSTGUI::CView(size),editor_(editor)
{
    setTransparency(true);
    setMouseEnabled(true);
    setWantsFocus(true);
}

HardwareUIScale::HardwareUIScale(const HardwareUIScale& other)
: VSTGUI::CView(other),editor_(other.editor_)
{
}

void HardwareUIScale::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    const double zoom=editor_?editor_->getZoomFactor():1.0;
    const int percent=static_cast<int>(std::lround(zoom*100.0));
    char label[24] {};
    std::snprintf(label,sizeof(label),"UI %d%%",percent);

    context->setDrawMode(VSTGUI::kAntiAliasing);
    VSTGUI::CRect shadow=r; shadow.offset(0.0,2.0);
    fillRoundGradient(context,shadow,6.0,{2,3,5,220},{0,0,0,245});
    fillRoundGradient(context,r,6.0,{36,40,46,255},{17,20,24,255});
    strokeRound(context,r,6.0,{82,89,98,150},1.0);

    VSTGUI::CRect inner=r; inner.inset(3.0,3.0);
    strokeRound(context,inner,4.0,{255,255,255,22},1.0);

    context->setFont(VSTGUI::kNormalFont,8.4,VSTGUI::kBoldFace);
    context->setFontColor({193,186,166,245});
    context->drawString(VSTGUI::UTF8String(label),r,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult HardwareUIScale::onMouseDown(VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons)
{
    if(!editor_||!buttons.isLeftButton()||!getViewSize().pointInside(where))
        return VSTGUI::kMouseEventNotHandled;

    constexpr double factors[] {0.75,1.0,1.25,1.5};
    const double current=editor_->getZoomFactor();
    std::size_t index=0;
    double best=std::abs(current-factors[0]);
    for(std::size_t i=1;i<4;++i) {
        const double d=std::abs(current-factors[i]);
        if(d<best) { best=d; index=i; }
    }
    editor_->setZoomFactor(factors[(index+1)%4]);
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

void HardwareVUMeter::draw(VSTGUI::CDrawContext* context)
{
    const auto r=getViewSize();
    const auto value=std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0);

    // Static KnobMan face + continuously drawn needle. This avoids the huge
    // 128-frame VU filmstrip (which exceeded practical GPU bitmap dimensions)
    // while keeping the calibrated processor value untouched.
    if(filmstrip_ && filmstrip_->isLoaded()) {
        // Runtime face is generated at the exact 320x216 logical view size.
        // HiDPI uses the associated 3x platform bitmap, so no source/destination
        // resampling is attempted here.
        const VSTGUI::CRect faceSrc(0.0,0.0,320.0,216.0);
        context->fillRectWithBitmap(filmstrip_,faceSrc,r,1.f);

        // Geometry follows the approved 400x270 face: pivot near the lower centre,
        // with the needle sweeping over the printed -20..+3 scale.
        const VSTGUI::CPoint pivot(r.left+r.getWidth()*0.5,
                                   r.top+r.getHeight()*(239.0/270.0));
        const double needleAngle=(220.0+100.0*value)*kPi/180.0;
        const double needleLength=r.getHeight()*0.57;
        const VSTGUI::CPoint tip(pivot.x+std::cos(needleAngle)*needleLength,
                                 pivot.y+std::sin(needleAngle)*needleLength);

        // Soft shadow plus satin-grey needle matches the source meter better than
        // the red fallback needle used by the old procedural face.
        context->setFrameColor({0,0,0,95});
        context->setLineWidth(3.2);
        context->drawLine({pivot.x+1.2,pivot.y+1.3},{tip.x+1.2,tip.y+1.3});
        context->setFrameColor({86,88,90,245});
        context->setLineWidth(1.9);
        context->drawLine(pivot,tip);
        context->setFrameColor({235,235,229,100});
        context->setLineWidth(0.7);
        context->drawLine({pivot.x-0.7,pivot.y-0.5},{tip.x-0.7,tip.y-0.5});

        // Transparent exact-size overlay contains only the original NeedleCover.
        // Drawing it last makes the live needle pass mechanically behind the hub.
        if(cover_ && cover_->isLoaded()) {
            const VSTGUI::CRect coverSrc(0.0,0.0,320.0,216.0);
            context->fillRectWithBitmap(cover_,coverSrc,r,1.f);
        }

        setDirty(false);
        return;
    }
    const auto cx=r.getCenter().x;
    const auto cy=r.bottom+30.0;
    const auto radius=std::min(r.getWidth()*0.395,r.getHeight()*0.90);

    context->setDrawMode(VSTGUI::kAntiAliasing);

    // Deep floating shadow: the meter should read as a physical instrument let into the panel.
    VSTGUI::CRect shadow=r;
    shadow.offset(0.0,4.0);
    fillRoundGradient(context,shadow,12.0,{2,3,4,190},{0,0,0,255});

    // Three-stage machined bezel: gunmetal outer frame, satin shoulder, black inner lip.
    fillRoundGradient(context,r,12.0,{108,113,121,255},{23,26,31,255});
    strokeRound(context,r,12.0,{5,6,8,255},1.5);

    VSTGUI::CRect shoulder=r; shoulder.inset(3.0,3.0);
    strokeRound(context,shoulder,10.0,{226,229,233,42},1.0);

    VSTGUI::CRect lip=r; lip.inset(7.0,7.0);
    fillRoundGradient(context,lip,8.0,{29,32,37,255},{7,9,11,255});
    strokeRound(context,lip,8.0,{2,3,4,255},1.2);

    // Warm ivory card with a deeper laminated/glass cavity.
    VSTGUI::CRect face=r; face.inset(12.0,12.0);
    fillRoundGradient(context,face,5.5,{239,229,199,255},{190,170,130,255});
    strokeRound(context,face,5.5,{79,61,38,235},1.0);

    // Warm inner rim: this is the small amber edge that makes the glass look seated
    // above the printed card instead of painted directly onto it.
    VSTGUI::CRect amberRim=face; amberRim.inset(1.5,1.5);
    strokeRound(context,amberRim,4.8,{211,159,78,140},1.4);
    VSTGUI::CRect hotRim=face; hotRim.inset(3.2,3.2);
    strokeRound(context,hotRim,4.0,{244,229,190,78},1.0);

    // Slight edge vignette under the glass. Keep the middle open and luminous.
    auto shadeBand=[&](const VSTGUI::CRect& band,const VSTGUI::CColor& a,const VSTGUI::CColor& b,
                       const VSTGUI::CPoint& p1,const VSTGUI::CPoint& p2,double radius){
        auto* path=context->createRoundRectGraphicsPath(band,radius);
        if(!path) return;
        auto* gradient=VSTGUI::CGradient::create(0.0,1.0,a,b);
        if(gradient){
            context->fillLinearGradient(path,*gradient,p1,p2);
            gradient->forget();
        }
        path->forget();
    };
    const double edge=28.0;
    shadeBand({face.left,face.top,face.left+edge,face.bottom},
              {68,42,12,104},{68,42,12,0},{face.left,face.top},{face.left+edge,face.top},4.0);
    shadeBand({face.right-edge,face.top,face.right,face.bottom},
              {68,42,12,0},{68,42,12,104},{face.right-edge,face.top},{face.right,face.top},4.0);
    shadeBand({face.left,face.bottom-22.0,face.right,face.bottom},
              {70,44,16,0},{70,44,16,70},{face.left,face.bottom-22.0},{face.left,face.bottom},4.0);

    // Deterministic micro-patina: enough texture to stop the face reading as a flat fill,
    // but subtle enough that the scale stays crisp at every zoom factor.
    context->setFrameColor({92,67,35,16});
    context->setLineWidth(0.8);
    for(int i=0;i<42;++i){
        const double px=face.left+10.0+std::fmod(37.0*i+13.0,face.getWidth()-20.0);
        const double py=face.top+28.0+std::fmod(19.0*i+7.0,face.getHeight()-44.0);
        const double len=(i%3==0)?2.4:1.2;
        context->drawLine({px,py},{px+len,py+0.35});
    }

    // Printed identity and status furniture.
    context->setFont(VSTGUI::kNormalFont,8.3,VSTGUI::kBoldFace);
    context->setFontColor({81,72,56,210});
    context->drawString(VSTGUI::UTF8String("125A"),
                        {face.left+13,face.top+10,face.left+53,face.top+23},VSTGUI::kLeftText);
    context->setFont(VSTGUI::kNormalFont,8.0,VSTGUI::kBoldFace);
    context->setFontColor({119,70,58,195});
    context->drawString(VSTGUI::UTF8String("CLIP"),
                        {face.right-66,face.top+10,face.right-27,face.top+23},VSTGUI::kRightText);

    context->setFont(VSTGUI::kNormalFont,10.8,VSTGUI::kBoldFace);
    context->setFontColor({39,36,30,255});
    context->drawString(VSTGUI::UTF8String("VU"),
                        {cx-20,face.top+11,cx+20,face.top+28},VSTGUI::kCenterText);

    constexpr double meterStartDeg=205.0;
    constexpr double meterSweepDeg=130.0;

    // Continuous classic meter arc.
    context->setFrameColor({63,57,46,190});
    context->setLineWidth(1.0);
    VSTGUI::CPoint prev;
    for(int i=0;i<=52;++i){
        const double t=static_cast<double>(i)/52.0;
        const double a=(meterStartDeg+meterSweepDeg*t)*kPi/180.0;
        VSTGUI::CPoint p{cx+std::cos(a)*(radius-1.0),cy+std::sin(a)*(radius-1.0)};
        if(i>0) context->drawLine(prev,p);
        prev=p;
    }

    // Real VU geometry: each dB tick uses the same voltage-domain mapping as the
    // telemetry. This keeps the needle mathematically calibrated to the face.
    const auto isMajorVu=[](int db){
        switch(db){
            case -20: case -10: case -7: case -5: case -3:
            case -2: case -1: case 0: case 1: case 2: case 3: return true;
            default: return false;
        }
    };
    for(int db=-20;db<=3;++db){
        const double t=vuScaleNormalizedFromDb(static_cast<double>(db));
        const double a=(meterStartDeg+meterSweepDeg*t)*kPi/180.0;
        const bool major=isMajorVu(db);
        const double ro=radius+1.0;
        const double ri=radius-(major?13.0:7.0);
        context->setFrameColor(major?VSTGUI::CColor{39,36,30,245}:VSTGUI::CColor{72,66,54,190});
        context->setLineWidth(major?1.35:0.85);
        context->drawLine({cx+std::cos(a)*ri,cy+std::sin(a)*ri},
                          {cx+std::cos(a)*ro,cy+std::sin(a)*ro});
    }

    struct ScaleMark { const char* text; double db; };
    constexpr ScaleMark marks[]={
        {"-20",-20.0},{"-10",-10.0},{"-7",-7.0},{"-5",-5.0},{"-3",-3.0},
        {"-2",-2.0},{"-1",-1.0},{"0",0.0},{"+1",1.0},{"+2",2.0},{"+3",3.0}
    };
    context->setFont(VSTGUI::kNormalFont,9.2,VSTGUI::kBoldFace);
    context->setFontColor({45,41,34,255});
    for(const auto& mark:marks){
        const double t=vuScaleNormalizedFromDb(mark.db);
        const double a=(meterStartDeg+meterSweepDeg*t)*kPi/180.0;
        const double rr=radius-28.0;
        const double px=cx+std::cos(a)*rr;
        const double py=cy+std::sin(a)*rr;
        context->drawString(VSTGUI::UTF8String(mark.text),
                            {px-15.0,py-7.0,px+15.0,py+7.0},VSTGUI::kCenterText);
    }

    // The red overload arc starts exactly at the printed 0 VU position.
    context->setFrameColor({177,47,38,235});
    context->setLineWidth(2.3);
    VSTGUI::CPoint redPrev;
    const double redStart=vuScaleNormalizedFromDb(0.0);
    for(int i=0;i<=14;++i){
        const double t=redStart+(1.0-redStart)*static_cast<double>(i)/14.0;
        const double a=(meterStartDeg+meterSweepDeg*t)*kPi/180.0;
        VSTGUI::CPoint p{cx+std::cos(a)*(radius+1.0),cy+std::sin(a)*(radius+1.0)};
        if(i>0) context->drawLine(redPrev,p);
        redPrev=p;
    }

    // Tapered needle: aim at the exact calibrated point on the scale arc.
    // The mechanical pivot is above the arc centre, so reusing the arc angle as
    // a pivot angle would be geometrically wrong.
    const auto scaleAngle=(meterStartDeg+meterSweepDeg*value)*kPi/180.0;
    const VSTGUI::CPoint pivot(cx,r.bottom-5.0);
    const double targetRadius=radius-15.0;
    const VSTGUI::CPoint tip(cx+std::cos(scaleAngle)*targetRadius,
                             cy+std::sin(scaleAngle)*targetRadius);
    const double needleAngle=std::atan2(tip.y-pivot.y,tip.x-pivot.x);
    const double perpX=-std::sin(needleAngle);
    const double perpY= std::cos(needleAngle);

    auto* needlePath=context->createGraphicsPath();
    if(needlePath){
        needlePath->beginSubpath({pivot.x+perpX*2.2,pivot.y+perpY*2.2});
        needlePath->addLine(tip);
        needlePath->addLine({pivot.x-perpX*2.2,pivot.y-perpY*2.2});
        needlePath->closeSubpath();
        context->setFillColor({181,39,34,255});
        context->drawGraphicsPath(needlePath,VSTGUI::CDrawContext::kPathFilled);
        needlePath->forget();
    }
    context->setFrameColor({255,151,131,95});
    context->setLineWidth(0.8);
    context->drawLine({pivot.x+perpX*0.7,pivot.y+perpY*0.7},
                      {tip.x+perpX*0.4,tip.y+perpY*0.4});

    // Jewel-like pivot: steel collar, brass accent, dark centre.
    fillRadialEllipse(context,{cx-8,pivot.y-8,cx+8,pivot.y+8},
                      {224,226,230,255},{45,49,56,255},{-2.5,-2.5});
    context->setFrameColor({8,10,13,255}); context->setLineWidth(1.0);
    context->drawEllipse({cx-8,pivot.y-8,cx+8,pivot.y+8},VSTGUI::kDrawStroked);
    context->setFrameColor({181,146,82,210}); context->setLineWidth(1.2);
    context->drawEllipse({cx-5.2,pivot.y-5.2,cx+5.2,pivot.y+5.2},VSTGUI::kDrawStroked);
    context->setFillColor(kNeedleHub);
    context->drawEllipse({cx-3.1,pivot.y-3.1,cx+3.1,pivot.y+3.1},VSTGUI::kDrawFilled);

    // Full glass cover: broad curved reflection, a second softer band and hot rim catches.
    // The translucent overlays deliberately sit above the needle/printing like real glass.
    auto* glassBand=context->createGraphicsPath();
    if(glassBand){
        glassBand->beginSubpath({face.left+10.0,face.top+10.0});
        glassBand->addBezierCurve({face.left+95.0,face.top-2.0},
                                  {face.right-92.0,face.top+1.0},
                                  {face.right-10.0,face.top+12.0});
        glassBand->addLine({face.right-14.0,face.top+38.0});
        glassBand->addBezierCurve({face.right-105.0,face.top+25.0},
                                  {face.left+108.0,face.top+24.0},
                                  {face.left+14.0,face.top+42.0});
        glassBand->closeSubpath();
        context->setFillColor({255,252,235,58});
        context->drawGraphicsPath(glassBand,VSTGUI::CDrawContext::kPathFilled);
        glassBand->forget();
    }

    auto* softBand=context->createGraphicsPath();
    if(softBand){
        softBand->beginSubpath({face.left+17.0,face.top+46.0});
        softBand->addBezierCurve({face.left+112.0,face.top+29.0},
                                 {face.right-118.0,face.top+31.0},
                                 {face.right-22.0,face.top+49.0});
        softBand->addLine({face.right-26.0,face.top+62.0});
        softBand->addBezierCurve({face.right-125.0,face.top+46.0},
                                 {face.left+120.0,face.top+44.0},
                                 {face.left+20.0,face.top+60.0});
        softBand->closeSubpath();
        context->setFillColor({255,244,211,32});
        context->drawGraphicsPath(softBand,VSTGUI::CDrawContext::kPathFilled);
        softBand->forget();
    }

    // Crisp top reflection and warm side catches make the pane visibly curved.
    auto* glassLine=context->createGraphicsPath();
    if(glassLine){
        glassLine->beginSubpath({face.left+18.0,face.top+13.0});
        glassLine->addBezierCurve({face.left+102.0,face.top+1.0},
                                  {face.right-112.0,face.top+4.0},
                                  {face.right-22.0,face.top+15.0});
        context->setFrameColor({255,255,247,155});
        context->setLineWidth(1.9);
        context->drawGraphicsPath(glassLine,VSTGUI::CDrawContext::kPathStroked);
        glassLine->forget();
    }

    context->setFrameColor({255,186,78,120});
    context->setLineWidth(1.45);
    context->drawLine({face.left+5.0,face.top+10.0},{face.left+5.0,face.bottom-16.0});
    context->drawLine({face.right-5.0,face.top+11.0},{face.right-5.0,face.bottom-18.0});

    // Small corner bloom: the strongest local catch in the glass, inspired by the reference.
    auto* cornerGlow=context->createGraphicsPath();
    if(cornerGlow){
        cornerGlow->beginSubpath({face.left+4.0,face.top+34.0});
        cornerGlow->addBezierCurve({face.left+7.0,face.top+14.0},
                                   {face.left+22.0,face.top+5.0},
                                   {face.left+48.0,face.top+4.0});
        context->setFrameColor({255,217,144,115});
        context->setLineWidth(2.2);
        context->drawGraphicsPath(cornerGlow,VSTGUI::CDrawContext::kPathStroked);
        cornerGlow->forget();
    }

    // Soft lower reflection: just enough to show another glass plane without whitening the meter.
    context->setFrameColor({255,246,221,38});
    context->setLineWidth(1.0);
    context->drawLine({face.left+42.0,face.bottom-18.0},{face.right-55.0,face.bottom-18.0});

    setDirty(false);
}


HardwareClipLed::HardwareClipLed(const VSTGUI::CRect& size,VSTGUI::IControlListener* listener,int32_t tag)
: VSTGUI::CControl(size,listener,tag,nullptr)
{
    setTransparency(true);
    setMouseEnabled(false);
}

HardwareClipLed::HardwareClipLed(const HardwareClipLed& other)
: VSTGUI::CControl(other)
{
}



void HardwareClipLed::draw(VSTGUI::CDrawContext* context)
{
    const bool on=getValueNormalized()>=0.5;
    const auto r=getViewSize();
    context->setDrawMode(VSTGUI::kAntiAliasing);
    if(on) {
        for(int n=2;n>=1;--n) {
            const double g=n*2.5;
            context->setFillColor({246,62,52,static_cast<uint8_t>(13*n)});
            context->drawEllipse({r.left-g,r.top-g,r.right+g,r.bottom+g},VSTGUI::kDrawFilled);
        }
    }
    fillRadialEllipse(context,r,on?VSTGUI::CColor{255,185,174,255}:VSTGUI::CColor{72,29,25,255},
                              on?VSTGUI::CColor{235,47,39,255}:VSTGUI::CColor{25,10,9,255},{-3.0,-3.0});
    context->setFrameColor({7,8,10,255}); context->setLineWidth(1.0);
    context->drawEllipse(r,VSTGUI::kDrawStroked);
    setDirty(false);
}

} // namespace MixEngine
