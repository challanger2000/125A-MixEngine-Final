#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cview.h"
#include <string>
#include <vector>

namespace VSTGUI { class VST3Editor; }

namespace MixEngine {

class HardwareFaceplate final : public VSTGUI::CView {
public:
    explicit HardwareFaceplate(const VSTGUI::CRect& size);
    HardwareFaceplate(const HardwareFaceplate& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareFaceplate(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
};

class HardwareLogo final : public VSTGUI::CView {
public:
    explicit HardwareLogo(const VSTGUI::CRect& size);
    HardwareLogo(const HardwareLogo& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareLogo(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
};

class HardwareKnob final : public VSTGUI::CAnimKnob {
public:
    enum class Style { Large, Medium, Small };
    HardwareKnob(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, Style style, VSTGUI::CBitmap* filmstrip);
    HardwareKnob(const HardwareKnob& other);
    ~HardwareKnob() override;
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareKnob(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
private:
    Style style_;
    VSTGUI::CBitmap* filmstrip_ {nullptr};
};

class HardwareToggle final : public VSTGUI::COnOffButton {
public:
    HardwareToggle(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, VSTGUI::CBitmap* filmstrip, bool ledLeft=false, bool moduleLedAbove=false, bool blueLed=false);
    HardwareToggle(const HardwareToggle& other);
    ~HardwareToggle() override;
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareToggle(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
private:
    VSTGUI::CBitmap* filmstrip_ {nullptr};
    bool ledLeft_ {false};
    bool moduleLedAbove_ {false};
    bool blueLed_ {false};
};

class HardwareSelector final : public VSTGUI::CControl {
public:
    HardwareSelector(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, std::vector<std::string> labels);
    HardwareSelector(const HardwareSelector& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareSelector(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
private:
    std::vector<std::string> labels_;
};

class HardwareLabel final : public VSTGUI::CView {
public:
    HardwareLabel(const VSTGUI::CRect& size, std::string text, double fontSize, bool bold=false, bool muted=false);
    HardwareLabel(const HardwareLabel& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareLabel(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
private:
    std::string text_;
    double fontSize_ {11.0};
    bool bold_ {false};
    bool muted_ {false};
};

class HardwareUIScale final : public VSTGUI::CView {
public:
    HardwareUIScale(const VSTGUI::CRect& size, VSTGUI::VST3Editor* editor);
    HardwareUIScale(const HardwareUIScale& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareUIScale(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
private:
    VSTGUI::VST3Editor* editor_ {nullptr};
};

class HardwareVUMeter final : public VSTGUI::CControl {
public:
    HardwareVUMeter(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag, VSTGUI::CBitmap* filmstrip);
    HardwareVUMeter(const HardwareVUMeter& other);
    ~HardwareVUMeter() override;
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareVUMeter(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
private:
    VSTGUI::CBitmap* filmstrip_ {nullptr};
};

class HardwareClipLed final : public VSTGUI::CControl {
public:
    HardwareClipLed(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);
    HardwareClipLed(const HardwareClipLed& other);
    VSTGUI::CBaseObject* newCopy() const override { return new HardwareClipLed(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
};

} // namespace MixEngine
