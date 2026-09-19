#include "controller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <cstring>

namespace MixEngine {

Steinberg::IPlugView* PLUGIN_API Controller::createView(Steinberg::FIDString name)
{
    if (std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
#ifdef MIXENGINE_CHANNEL_BUILD
        auto* editor=new VSTGUI::VST3Editor(this, "view", "mixengine_channel.uidesc");
#else
        auto* editor=new VSTGUI::VST3Editor(this, "view", "mixengine.uidesc");
#endif
        editor->setAllowedZoomFactors({0.75,1.0,1.25,1.5});
        return editor;
    }
    return nullptr;
}

} // namespace MixEngine
