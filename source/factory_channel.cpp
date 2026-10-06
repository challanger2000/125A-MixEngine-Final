#include "processor.h"
#include "controller.h"
#include "pluginids.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

#define stringPluginName "125A MixEngine V3 Channel"

using namespace Steinberg;
using namespace Steinberg::Vst;

static_assert(sizeof(MixEngine::Processor) < 64 * 1024,
              "Channel Processor unexpectedly carries large Mix FX-only state banks");

BEGIN_FACTORY_DEF("125A",
                  "",
                  "")

// Standard VST3 insert effect for hosts that do not use the PreSonus Mix FX API.
DEF_CLASS2(INLINE_UID_FROM_FUID(MixEngine::kChannelProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           stringPluginName,
           Vst::kDistributable,
           Vst::PlugType::kFx,
           MIXENGINE_VERSION,
           kVstVersionString,
           MixEngine::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(MixEngine::kChannelControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           stringPluginName " Controller",
           0,
           "",
           MIXENGINE_VERSION,
           kVstVersionString,
           MixEngine::Controller::createInstance)

END_FACTORY
