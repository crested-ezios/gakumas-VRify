// Native support for the hosted VR translation units. No proxy, nested loader,
// console bootstrap, Localify reload, WndProc ownership or resource downloads.
#include <Windows.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include "windowsPlatform.hpp"
#include "GakumasLocalify/Il2cppUtils.hpp"
#include "host/localify/WindowsIntegration.hpp"

std::filesystem::path gakumasLocalPath;
std::filesystem::path ProgramConfigJson;
std::filesystem::path ConfigJson;
std::filesystem::path VrConfigJson;
bool g_has_config_file = false;
bool g_enable_console = false;
bool g_useRemoteAssets = false;
bool g_useAPIAssets = false;
bool g_useAPITextureAssets = false;
bool g_delTextureRemoteAfterUpdate = true;
std::string g_remoteResourceUrl;
std::string g_useAPIAssetsURL;
std::string g_useAPITextureAssetsURL;
std::function<void()> g_on_close;
std::function<void()> on_hotKey_0;
std::function<void()> g_reload_all_data = [] {};

void loadConfig(const std::string&, GakumasLocal::Config::ConfigLoadPurpose) {}
void loadConfig(const std::filesystem::path&, GakumasLocal::Config::ConfigLoadPurpose) {}
void readProgramConfig() {}
void reload_all_data() {}
bool getCurrentLodingProgress(int* total, int* current, int* classes, int* done) {
    if (total) *total = 0;
    if (current) *current = 0;
    if (classes) *classes = 0;
    if (done) *done = 0;
    return false;
}

namespace GakumasLocal::WinHooks {
void* LoadAssetBundle(const std::string&) { return nullptr; }
namespace Keyboard { void InstallWndProcHook() {} }
}
namespace GakumasVR::Localify {
void* LoadBundle(const std::string&) { return nullptr; }
bool WindowMessage(HWND, UINT, WPARAM, LPARAM, WNDPROC, LRESULT&) { return false; }
void InstallWindowProcedure(WNDPROC, WNDPROC*) {}
}
