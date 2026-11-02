"""Compile real begin hook bodies and Motion handshake against the provider."""
from pathlib import Path
import subprocess, os, argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--maintainer-motion-root", type=Path, help="Opt in with separately supplied Motion implementation source")
args = parser.parse_args()
if args.maintainer_motion_root is None:
    print("SKIP: optional Motion implementation integration; use --maintainer-motion-root explicitly")
    raise SystemExit(0)
r = Path(__file__).resolve().parents[1]
m = args.maintainer_motion_root.resolve()
def function(text, marker):
    start = text.index(marker)
    brace = text.index('{', start)
    level = 1; end = brace + 1
    while level:
        level += (text[end] == '{') - (text[end] == '}'); end += 1
    return text[start:end]
consumer = (m/'src/GkCameraBridge.hpp').read_text(encoding='utf-8')
hooks = (r/'src/vr/unity/RenderBeginHooks.inc.cpp').read_text(encoding='utf-8')
assert (r/'src/host/GakuVrifyCameraBegin.h').read_bytes() == (m/'src/GakuVrifyCameraBegin.h').read_bytes()
source = r'''
#include "host/CameraBeginProvider.hpp"
#include <cassert>
#include <atomic>
#include <vector>
#include <string_view>
#include <iostream>
std::vector<int> events;
namespace GakuMotion::host { bool active=true; GakuHostApi api{}; }
struct { void (*logWarn)(const char*,...)=nullptr; void (*logInfo)(const char*,...)=nullptr; } g_hostContext;
GakuVrifyCameraBeginService sharedBegin{};
bool sharedBeginSubscribed=false;
std::atomic<bool> sharedBeginEnabled{false};
bool unityThread=true;
bool OnUnityThread(){return unityThread;}
struct Ctrl { void BeforeRender(void*) { events.push_back(3); } } ctrl;
Ctrl& Controller(){return ctrl;}
struct Renderer {
    bool IsEyeCamera(void* c){return c == (void*)2;}
    void OnBeginCamera(void*) {events.push_back(1);}
    const char* ClassifyCamera(void*) {return "source";}
} unityStereoRenderer;
namespace gakumas::vr { bool LiveSourcePhotoProtectionActive(){return false;} namespace camera { bool IsVrFreeCameraFirstPerson(){return false;} } }
void SetFpHeadColorSkip(bool,const char*){}
void ResetActorShadowLeftReuse(){}
void ReportVrUnityHookException(){}
void Original(void*,void*,void*){events.push_back(2);}
auto BeginCameraRendering_Orig = Original;
auto BeginCameraRenderingManager_Orig = Original;
#define DEFINE_HOOK(ret,name,args) ret name args
'''
source += function(consumer, '\tinline void SharedCameraBegin(') + '\n'
source += function(consumer, '\tinline bool ConnectSharedCameraBegin(') + '\n'
for name in ('BeginCameraRendering', 'BeginCameraRenderingManager'):
    source += function(hooks, '    DEFINE_HOOK(void, '+name+',') + '\n'
source += r'''
namespace gakumas::vr {
enum class VrRuntimeState { Dormant, Unavailable, Faulted, SessionRunning, PoseReady, Stopped };
struct VrRuntime {
    VrRuntimeState state{VrRuntimeState::Dormant};
    static VrRuntime& Instance(){static VrRuntime value;return value;}
    VrRuntimeState State(){return state;}
};
}
std::atomic<bool> sharedCameraConnected{true}, unityReady{true};
bool accepting=true;
bool IsUnityAccepting(){return accepting;}
auto VrCameraActive =
'''
source += function((r/'src/vr/unity/HostedUnity.inc.cpp').read_text(encoding='utf-8'), '[](void*) -> uint32_t') + ';\n'
source += r'''
int queryMode=0;
GakuStatus Query(void*,const char*,uint32_t,uint32_t,void* out){
    if(queryMode==1)return GAKU_E_UNAVAILABLE;
    auto s=GakumasVR::Hosted::cameraBeginProvider.Service();
    if(queryMode==2)s.abiVersion=9;
    *static_cast<GakuVrifyCameraBeginService*>(out)=s;
    return GAKU_OK;
}
void Throwing(void*,void*){throw 7;}
void Reenter(void*,void*){auto& p=GakumasVR::Hosted::cameraBeginProvider; assert(p.Target(0)==(void*)10);assert(p.Unsubscribe(Reenter,nullptr)==GAKU_OK);}
int main(){
    using S=gakumas::vr::VrRuntimeState;
    for(auto state : {S::Dormant,S::Unavailable,S::Faulted,S::Stopped}) {
        gakumas::vr::VrRuntime::Instance().state=state;assert(!VrCameraActive(nullptr));
    }
    gakumas::vr::VrRuntime::Instance().state=S::SessionRunning;assert(VrCameraActive(nullptr));
    gakumas::vr::VrRuntime::Instance().state=S::PoseReady;assert(VrCameraActive(nullptr));
    accepting=false;assert(!VrCameraActive(nullptr));accepting=true;
    unityReady=false;assert(!VrCameraActive(nullptr));unityReady=true;
    sharedCameraConnected=false;assert(!VrCameraActive(nullptr));sharedCameraConnected=true;
    auto& p=GakumasVR::Hosted::cameraBeginProvider;
    GakuMotion::host::api.queryService=Query;
    bool selected=true;
    queryMode=1; assert(ConnectSharedCameraBegin((void*)10,(void*)11,selected)&&!selected);
    queryMode=2; assert(!ConnectSharedCameraBegin((void*)10,(void*)11,selected)&&selected);
    queryMode=0; assert(ConnectSharedCameraBegin((void*)10,(void*)11,selected)&&!selected);
    p.SetTarget(0,(void*)10);p.SetTarget(1,(void*)11);
    assert(!ConnectSharedCameraBegin((void*)12,(void*)11,selected)&&selected);
    assert(ConnectSharedCameraBegin((void*)10,(void*)11,selected)&&selected);
    BeginCameraRendering(nullptr,(void*)1,nullptr);assert((events==std::vector<int>{1,2,3}));events.clear();
    BeginCameraRenderingManager(nullptr,(void*)1,nullptr);assert((events==std::vector<int>{1,2,3}));events.clear();
    BeginCameraRendering(nullptr,(void*)2,nullptr);assert((events==std::vector<int>{1,2}));events.clear();
    unityThread=false;BeginCameraRendering(nullptr,(void*)1,nullptr);assert((events==std::vector<int>{1,2}));events.clear();unityThread=true;
    assert(p.Subscribe(Throwing,nullptr)==GAKU_E_CONFLICT);
    sharedBeginEnabled=false;p.Unsubscribe(SharedCameraBegin,nullptr);
    assert(p.Subscribe(Throwing,nullptr)==GAKU_OK);p.Dispatch((void*)1,false);p.Unsubscribe(Throwing,nullptr);
    assert(p.Subscribe(Reenter,nullptr)==GAKU_OK);p.Dispatch((void*)1,false);
    p.SetTarget(1,(void*)10);assert(ConnectSharedCameraBegin((void*)10,(void*)10,selected)&&selected);
    p.Close();p.Dispatch((void*)1,false);assert(events.empty());assert(p.Subscribe(SharedCameraBegin,nullptr)==GAKU_E_SHUTTING_DOWN);
    std::cout<<"PASS real begin hooks + Motion consumer: order, no-XR, eye exclusion, thread, mismatch, disabled, alias, reentry, exception, shutdown\n";
}
'''
build=r/'build/camera-begin-tests';build.mkdir(parents=True,exist_ok=True)
cpp=build/'test.cpp';cpp.write_text(source,encoding='utf-8')
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-prerelease','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
cmd=f'call "{vs}/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64 && cl /nologo /std:c++20 /EHsc /MT /DGAKU_VRIFY_HOSTED /DGKMS_WINDOWS /I"{r}/src" /I"{r}/sdk/gakuloader" /Fe:"{build}/test.exe" /Fo:"{build}/test.obj" "{cpp}"'
batch=build/'compile.cmd';batch.write_text('@echo off\n'+cmd+'\n',encoding='utf-8')
subprocess.run(f'"{os.environ["ComSpec"]}" /d /s /c ""{batch}""',check=True)
subprocess.run([str(build/'test.exe')],check=True,timeout=15)
