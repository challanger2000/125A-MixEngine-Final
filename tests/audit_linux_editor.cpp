// Actual Linux/X11 editor attachment. This is not Windows or Studio One QA.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <poll.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
using Clock=std::chrono::steady_clock;
class Frame final:public IPlugFrame,public Steinberg::Linux::IRunLoop {
public:
 Display* display=nullptr;Window window=0;uint32 refs=1;
 struct Event {Steinberg::Linux::IEventHandler* handler;int fd;};
 struct Timer {Steinberg::Linux::ITimerHandler* handler;std::chrono::milliseconds interval;Clock::time_point next;};
 std::vector<Event> events;std::vector<Timer> timers;
 tresult PLUGIN_API queryInterface(const TUID id,void** result) override {
  *result=nullptr;
  if(FUnknownPrivate::iidEqual(id,IPlugFrame::iid)||FUnknownPrivate::iidEqual(id,FUnknown::iid))*result=static_cast<IPlugFrame*>(this);
  else if(FUnknownPrivate::iidEqual(id,Steinberg::Linux::IRunLoop::iid))*result=static_cast<Steinberg::Linux::IRunLoop*>(this);
  else return kNoInterface;addRef();return kResultOk;
 }
 uint32 PLUGIN_API addRef() override{return ++refs;}
 uint32 PLUGIN_API release() override{auto n=--refs;if(!n)delete this;return n;}
 tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* size) override {
  XResizeWindow(display,window,size->getWidth(),size->getHeight());XSync(display,false);return view->onSize(size);
 }
 tresult PLUGIN_API registerEventHandler(Steinberg::Linux::IEventHandler* h,int fd) override {h->addRef();events.push_back({h,fd});return kResultOk;}
 tresult PLUGIN_API unregisterEventHandler(Steinberg::Linux::IEventHandler* h) override {
  for(auto i=events.begin();i!=events.end();++i)if(i->handler==h){h->release();events.erase(i);return kResultOk;}return kResultFalse;
 }
 tresult PLUGIN_API registerTimer(Steinberg::Linux::ITimerHandler* h,Steinberg::Linux::TimerInterval ms) override {
  h->addRef();timers.push_back({h,std::chrono::milliseconds(ms),Clock::now()+std::chrono::milliseconds(ms)});return kResultOk;
 }
 tresult PLUGIN_API unregisterTimer(Steinberg::Linux::ITimerHandler* h) override {
  for(auto i=timers.begin();i!=timers.end();++i)if(i->handler==h){h->release();timers.erase(i);return kResultOk;}return kResultFalse;
 }
 void pump(int milliseconds){
  auto until=Clock::now()+std::chrono::milliseconds(milliseconds);
  while(Clock::now()<until){
   auto copied=events;for(auto& e:copied)e.handler->addRef();
   for(auto& e:copied){pollfd fd{e.fd,POLLIN,0};if(poll(&fd,1,0)>0)e.handler->onFDIsSet(e.fd);e.handler->release();}
   std::vector<Steinberg::Linux::ITimerHandler*> ready;
   for(auto& t:timers)if(Clock::now()>=t.next){t.next=Clock::now()+t.interval;t.handler->addRef();ready.push_back(t.handler);}
   for(auto* t:ready){t->onTimer();t->release();}
   XSync(display,false);std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
 }
};
class Host final:public HostApplication {
public:
 IPtr<Frame> frame;
 explicit Host(Frame* f):frame(f){}
 tresult PLUGIN_API queryInterface(const TUID id,void** result) override {
  if(FUnknownPrivate::iidEqual(id,Steinberg::Linux::IRunLoop::iid))return frame->queryInterface(id,result);
  return HostApplication::queryInterface(id,result);
 }
};
struct ViewRemoval {
 IPlugView* view;bool active=false;
 ~ViewRemoval(){if(active){view->removed();view->setFrame(nullptr);}}
};
void capture(Display* d,Window w,const char* path,int width,int height){
 auto* image=XGetImage(d,w,0,0,width,height,AllPlanes,ZPixmap);if(!image)throw 20;
 std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<" "<<height<<"\n255\n";
 std::size_t nonblack=0;
 for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto p=XGetPixel(image,x,y);char rgb[]{char(p>>16),char(p>>8),char(p)};out.write(rgb,3);if(p&0xffffff)++nonblack;}
 XDestroyImage(image);if(!out||nonblack<width*height/2u)throw 21;
}
int main(int argc,char** argv){try{
 if(argc!=4)return 1;auto frame=owned(new Frame);frame->display=XOpenDisplay(nullptr);if(!frame->display)return 2;
 auto host=owned(new Host(frame));
 std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);if(!module){std::cerr<<error;return 3;}
 int attachments=0;
 module->getFactory().setHostContext(host);
 for(const auto& info:module->getFactory().classInfos())if(info.category()==kVstComponentControllerClass){
  auto controller=module->getFactory().createInstance<IEditController>(info.ID());
  if(!controller||controller->initialize(host)!=kResultOk)return 4;
  for(int cycle=0;cycle<20;++cycle){
   std::cerr<<"cycle "<<cycle<<" createView\n";
   IPtr<IPlugView> view=owned(controller->createView(ViewType::kEditor));if(!view)return 5;
   ViewRemoval removal{view};
   if(view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID)!=kResultTrue)return 6;
   ViewRect size{};if(view->getSize(&size)!=kResultOk)return 7;
   frame->window=XCreateSimpleWindow(frame->display,DefaultRootWindow(frame->display),0,0,size.getWidth(),size.getHeight(),0,0,0);
   XMapWindow(frame->display,frame->window);XSync(frame->display,false);
   std::cerr<<"attach\n";
   if(view->setFrame(frame)!=kResultOk||view->attached(reinterpret_cast<void*>(frame->window),kPlatformTypeX11EmbedWindowID)!=kResultOk)return 8;
   removal.active=true;frame->pump(50);
   Window rootWindow=0,parentWindow=0,*children=nullptr;unsigned count=0;
   if(!XQueryTree(frame->display,frame->window,&rootWindow,&parentWindow,&children,&count)||count==0)return 22;
   for(unsigned i=0;i<count;++i){
    XEvent event{};event.xclient.type=ClientMessage;event.xclient.display=frame->display;
    event.xclient.window=children[i];event.xclient.message_type=XInternAtom(frame->display,"_XEMBED",false);
    event.xclient.format=32;event.xclient.data.l[0]=CurrentTime;event.xclient.data.l[1]=0;
    event.xclient.data.l[3]=frame->window;event.xclient.data.l[4]=0;
    XSendEvent(frame->display,children[i],false,NoEventMask,&event);
   }
   XFree(children);XSync(frame->display,false);frame->pump(200);
   if(cycle==0){capture(frame->display,frame->window,argv[2],size.getWidth(),size.getHeight());
    FUnknownPtr<IPlugViewContentScaleSupport> scale(view);
    if(!scale||scale->setContentScaleFactor(1.5f)!=kResultOk)return 9;
    std::cerr<<"pump\n";frame->pump(150);if(view->getSize(&size)!=kResultOk)return 10;
    XWindowAttributes attributes{};XGetWindowAttributes(frame->display,frame->window,&attributes);
    if(attributes.width!=size.getWidth()||attributes.height!=size.getHeight())return 11;
    capture(frame->display,frame->window,argv[3],size.getWidth(),size.getHeight());
    std::cout<<"150% native editor geometry="<<size.getWidth()<<"x"<<size.getHeight()<<"\n";
   }
   if(view->removed()!=kResultOk)return 12;removal.active=false;view->setFrame(nullptr);view=nullptr;
   ++attachments;XDestroyWindow(frame->display,frame->window);frame->window=0;frame->pump(10);
  }
  if(controller->terminate()!=kResultOk)return 13;controller=nullptr;
 }
 if(attachments!=20)return 14;
 module=nullptr;XCloseDisplay(frame->display);frame->display=nullptr;
 std::cout<<"PASS: Linux X11 editor attach/detach x20 and 100/150% pixel captures. Native mouse/Studio One NOT VERIFIED.\n";return 0;
}catch(int error){std::cerr<<"Editor probe failure "<<error<<"\n";return error;}}
