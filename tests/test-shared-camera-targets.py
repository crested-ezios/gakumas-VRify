"""Compile production shared-target selection; no game or XR required."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
production = (root / 'src/vr/unity/HostedUnity.inc.cpp').read_text(encoding='utf-8')


def function(marker):
    start = production.index(marker)
    end = production.index('{', start) + 1
    depth = 1
    while depth:
        depth += (production[end] == '{') - (production[end] == '}')
        end += 1
    return production[start:end]


source = r'''
#include "host/GakuLocalifyCameraRender.h"
#include <atomic>
#include <cassert>
#include <cstdio>
namespace GakumasLocal {
namespace Log { template<class... T> void InfoFmt(const char*, T...) {} }
namespace HookMain {
void ObserveMainCamera(void*) {} void BeforeCameraState(void*,void*) {}
void AfterCameraState(void*) {} void AfterEndCamera(void*) {}
}
}
namespace UnityResolve::UnityType { struct Camera {}; }
namespace gakumas::vr {
enum class VrRuntimeState { SessionRunning, PoseReady };
struct VrRuntime {
static VrRuntime& Instance() { static VrRuntime r; return r; }
VrRuntimeState State() { return VrRuntimeState::SessionRunning; }
};
}
GakuLocalifyCameraRenderService sharedCamera{};
bool sharedCameraSelected{}, sharedCameraValid{}, sharedCameraTargetMismatch{};
bool sharedCameraTargets[GAKULOCALIFY_CAMERA_RENDER_COUNT]{};
std::atomic<bool> sharedCameraConnected{false}, unityReady{true};
unsigned char sharedCameraUser{};
bool IsUnityAccepting() { return true; }
void* published[3]{};
int subscriptions{};
GakuStatus subscriptionResult{GAKU_OK};
void* Target(void*,uint32_t k) { return published[k]; }
GakuStatus Subscribe(void*, const GakuLocalifyCameraRenderCallbacks*) {
    ++subscriptions; return subscriptionResult;
}
void Reset() {
    sharedCamera = {}; sharedCamera.getTarget = Target; sharedCamera.subscribe = Subscribe;
    sharedCameraSelected = sharedCameraValid = true;
    sharedCameraTargetMismatch = false; sharedCameraConnected = false;
    for(int i=0;i<3;++i) { published[i]=nullptr; sharedCameraTargets[i]=false; }
    subscriptions=0; subscriptionResult=GAKU_OK;
}
'''
for name in ('bool UseSharedCameraTarget(', 'bool ConnectSharedCameraRender(', 'bool SharedCameraTargetReady('):
    source += function(name) + '\n'
source += r'''
int main() {
    void* targets[]{(void*)0x10, (void*)0x20, (void*)0x30};
    // Missing service and all-null published service both retain exclusive ownership.
    Reset(); sharedCameraSelected=false;
    assert(!UseSharedCameraTarget(0,targets[0])); assert(ConnectSharedCameraRender());
    for(unsigned mask=0;mask<8;++mask) {
        Reset();
        for(unsigned i=0;i<3;++i) {
            if(mask & (1u<<i)) published[i]=targets[i];
            assert(UseSharedCameraTarget(i,targets[i]) == bool(mask & (1u<<i)));
        }
        assert(ConnectSharedCameraRender());
        assert(subscriptions == (mask ? 1 : 0));
        for(unsigned i=0;i<3;++i)
            assert(SharedCameraTargetReady(i) == bool(mask & (1u<<i)));
    }
    // A non-null mismatch or unavailable metadata never authorizes competing hooks.
    for(unsigned bad=0;bad<3;++bad) {
        Reset(); published[bad]=(void*)0x99;
        assert(UseSharedCameraTarget(bad,targets[bad]));
        assert(!ConnectSharedCameraRender()); assert(subscriptions==0);
        Reset(); published[bad]=targets[bad];
        assert(UseSharedCameraTarget(bad,nullptr));
        assert(!ConnectSharedCameraRender()); assert(subscriptions==0);
    }
    Reset(); sharedCameraValid=false;
    assert(UseSharedCameraTarget(0,targets[0]));
    assert(!ConnectSharedCameraRender()); assert(subscriptions==0);
    Reset(); published[1]=targets[1]; subscriptionResult=GAKU_E_CONFLICT;
    assert(UseSharedCameraTarget(1,targets[1])); assert(!ConnectSharedCameraRender());
    assert(!SharedCameraTargetReady(1));
    assert(!SharedCameraTargetReady(99));
    std::puts("PASS production camera target selection: absent, all-null, all 8 ownership subsets, mismatch, missing metadata, invalid service, subscription failure");
}
'''
build = root / 'build/shared-camera-target-tests'
build.mkdir(parents=True, exist_ok=True)
cpp = build / 'test.cpp'
cpp.write_text(source, encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-prerelease', '-products', '*',
    '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    '-property', 'installationPath'], text=True).strip()
command = (f'call "{vs}/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64 && '
    f'cl /nologo /std:c++20 /EHsc /MT /I"{root}/src" '
    f'/I"{root}/sdk/gakuloader" /Fe:"{build}/test.exe" '
    f'/Fo:"{build}/test.obj" "{cpp}"')
batch = build / 'compile.cmd'
batch.write_text('@echo off\n' + command + '\n', encoding='utf-8')
subprocess.run(f'"{os.environ["ComSpec"]}" /d /s /c ""{batch}""', check=True)
subprocess.run([str(build / 'test.exe')], check=True, timeout=15)
