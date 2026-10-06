#include "../source/controller.h"
#include "../source/pluginids.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <cmath>
#include <iostream>

using namespace Steinberg;
using namespace Steinberg::Vst;

int main(){
    for(int cycle=0;cycle<32;++cycle){
        auto* controller=new MixEngine::Controller();
        if(controller->initialize(nullptr)!=kResultOk){
            std::cerr<<"Controller initialize failed at cycle "<<cycle<<"\n";
            controller->release();
            return 1;
        }

        if(controller->getParameterCount() < MixEngine::kParamCount){
            std::cerr<<"Controller parameter table incomplete\n";
            controller->terminate();
            controller->release();
            return 2;
        }

        auto* depth=controller->getParameterObject(MixEngine::kParamDepth);
        auto* width=controller->getParameterObject(MixEngine::kParamWidth);
        auto* input=controller->getParameterObject(MixEngine::kParamInput);
        auto* output=controller->getParameterObject(MixEngine::kParamOutput);
        if(!depth||!width||!input||!output){
            std::cerr<<"Required parameter object missing\n";
            controller->terminate();
            controller->release();
            return 3;
        }
        if(std::abs(depth->getInfo().defaultNormalizedValue-0.5)>1e-12 ||
           std::abs(width->getInfo().defaultNormalizedValue-0.5)>1e-12 ||
           std::abs(input->getInfo().defaultNormalizedValue-0.5)>1e-12 ||
           std::abs(output->getInfo().defaultNormalizedValue-0.5)>1e-12){
            std::cerr<<"Core normalized defaults drifted\n";
            controller->terminate();
            controller->release();
            return 4;
        }

        // Do not create/open the VSTGUI editor in this headless executable.
        // VST3Editor requires a real host/frame context; forcing it here is an
        // invalid harness and can crash independently of the plugin lifecycle.
        if(controller->terminate()!=kResultOk){
            std::cerr<<"Controller terminate failed at cycle "<<cycle<<"\n";
            controller->release();
            return 5;
        }
        controller->release();
    }
    std::cout<<"PASS: controller lifecycle x32\n";
    return 0;
}
