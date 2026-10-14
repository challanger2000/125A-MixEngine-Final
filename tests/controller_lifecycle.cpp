#include "../source/pluginids.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace Steinberg;
using namespace Steinberg::Vst;
int main(int argc,char** argv){
 std::filesystem::path bundle;
 if(argc>1)bundle=argv[1];
 else{
  auto dir=std::filesystem::absolute(argv[0]).parent_path();
  for(int i=0;i<5&&bundle.empty();++i,dir=dir.parent_path()){
   auto candidate=dir/"VST3"/"Release"/"125A-MixEngine-V3-Channel.vst3";
   if(std::filesystem::is_directory(candidate))bundle=candidate;
  }
 }
 if(bundle.empty()){std::cerr<<"Provide compiled Channel VST3 path\n";return 1;}
 auto host=owned(new HostApplication);
 std::string error;auto module=VST3::Hosting::Module::create(bundle.string(),error);
 if(!module){std::cerr<<error<<"\n";return 2;}
 const auto factory=module->getFactory();
 factory.setHostContext(host);
 int tested=0;
 for(const auto& info:factory.classInfos()){
  if(info.category()!=kVstComponentControllerClass)continue;
  for(int cycle=0;cycle<32;++cycle){
   auto controller=factory.createInstance<IEditController>(info.ID());
   if(!controller||controller->initialize(host)!=kResultOk)return 3;
   std::array<double,MixEngine::kParamCount> before{};std::array<bool,MixEngine::kParamCount> found{};
   for(int32 i=0;i<controller->getParameterCount();++i){
    ParameterInfo p{};if(controller->getParameterInfo(i,p)!=kResultOk)return 4;
    for(double value:{0.,1.}){
     String128 text{};ParamValue parsed=0;
     if(controller->getParamStringByValue(p.id,value,text)!=kResultOk)return 18;
     if(controller->getParamValueByString(p.id,text,parsed)!=kResultOk||std::abs(parsed-value)>1e-6){
      std::cerr<<"Parameter string roundtrip failure id="<<p.id<<" value="<<value<<"\n";return 19;
     }
    }
    if(p.id<MixEngine::kParamCount){found[p.id]=true;before[p.id]=controller->getParamNormalized(p.id);}
    if((p.id==MixEngine::kParamDepth||p.id==MixEngine::kParamWidth||p.id==MixEngine::kParamInput||p.id==MixEngine::kParamOutput)&&std::abs(p.defaultNormalizedValue-.5)>1e-12)return 5;
   }
   for(bool f:found)if(!f)return 6;
   // Verify the actual DLL controller rejects partial doubles atomically.
   MemoryStream data;IBStreamer writer(&data,kLittleEndian);
   for(int i=0;i<MixEngine::kParamCount;++i)if(!writer.writeDouble(.75))return 7;
   for(int bytes:{8,199,207,209,215,217,223,225,231,233,239}){
    MemoryStream truncated(data.getData(),bytes);
    if(controller->setComponentState(&truncated)==kResultOk)return 8;
    for(ParamID id=0;id<MixEngine::kParamCount;++id)if(controller->getParamNormalized(id)!=before[id])return 9;
   }
   // The five published complete state lengths retain their legacy defaults.
   for(int count:{26,27,28,29,30}){
    MemoryStream legacy(data.getData(),count*8);
    if(controller->setComponentState(&legacy)!=kResultOk)return 10;
    for(ParamID id=0;id<ParamID(count);++id)if(controller->getParamNormalized(id)!=.75)return 11;
    if(count==26&&controller->getParamNormalized(MixEngine::kParamTubeType)!=.5)return 12;
    if(count<=27&&controller->getParamNormalized(MixEngine::kParamMeterSource)!=1.)return 13;
    if(count<=28&&controller->getParamNormalized(MixEngine::kParamTapeHiss)!=.75)return 14;
    if(count<=29&&controller->getParamNormalized(MixEngine::kParamVinylNoise)!=.75)return 15;
   }
   if(controller->terminate()!=kResultOk)return 16;
   ++tested;
  }
 }
 if(tested!=32)return 17;
 std::cout<<"PASS: compiled VST3 controller lifecycle x32, defaults, atomic malformed states and five legacy lengths. Editor attach/mouse not tested.\n";
 return 0;
}
