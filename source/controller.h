#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "vstgui/lib/cview.h"
#include "vstgui/uidescription/uidescription.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "pluginterfaces/vst/ivstdataexchange.h"
#include "public.sdk/source/vst/utility/dataexchange.h"
#include "meter_exchange.h"

namespace MixEngine {

class Controller final : public Steinberg::Vst::EditControllerEx1,
                         public VSTGUI::VST3EditorDelegate,
                         public Steinberg::Vst::IDataExchangeReceiver {
public:
    Controller();
    ~Controller() override = default;
    static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller()); }
    Steinberg::uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return Steinberg::Vst::EditControllerEx1::addRef(); }
    Steinberg::uint32 PLUGIN_API release() SMTG_OVERRIDE { return Steinberg::Vst::EditControllerEx1::release(); }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;
    void PLUGIN_API queueOpened(Steinberg::Vst::DataExchangeUserContextID userContextID,
                                Steinberg::uint32 blockSize,
                                Steinberg::TBool& dispatchOnBackgroundThread) SMTG_OVERRIDE;
    void PLUGIN_API queueClosed(Steinberg::Vst::DataExchangeUserContextID userContextID) SMTG_OVERRIDE;
    void PLUGIN_API onDataExchangeBlocksReceived(Steinberg::Vst::DataExchangeUserContextID userContextID,
                                                 Steinberg::uint32 numBlocks,
                                                 Steinberg::Vst::DataExchangeBlock* blocks,
                                                 Steinberg::TBool onBackgroundThread) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name,
                                     const VSTGUI::UIAttributes& attributes,
                                     const VSTGUI::IUIDescription* description,
                                     VSTGUI::VST3Editor* editor) override;
private:
    Steinberg::Vst::DataExchangeReceiverHandler meterExchangeReceiver_;
};

} // namespace MixEngine
