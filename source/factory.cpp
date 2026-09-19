#include "processor.h"
#include "controller.h"
#include "pluginids.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

#define stringPluginName "125A MixEngine"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("125A",
                  "",
                  "")

// Dedicated PreSonus Studio One Mix FX class.
DEF_CLASS2(INLINE_UID_FROM_FUID(MixEngine::kProcessorUID),
           PClassInfo::kManyInstances,
           "Audio Mix Processor",
           stringPluginName,
           Vst::kDistributable,
           Vst::PlugType::kFx,
           MIXENGINE_VERSION,
           kVstVersionString,
           MixEngine::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(MixEngine::kControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           stringPluginName " Controller",
           0,
           "",
           MIXENGINE_VERSION,
           kVstVersionString,
           MixEngine::Controller::createInstance)

END_FACTORY
