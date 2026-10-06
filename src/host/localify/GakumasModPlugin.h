#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GAKUMAS_MOD_API_VERSION 1

#ifdef __cplusplus
extern "C" {
  #define GAKUMAS_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
  #define GAKUMAS_PLUGIN_EXPORT __declspec(dllexport)
#endif

// Plugin metadata structure
typedef struct GakumasPluginInfo {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
} GakumasPluginInfo;

// Host context provided by the Localify Native Mod Loader
typedef struct GakumasModContext {
    uint32_t apiVersion;
    const wchar_t* gameRoot;       // e.g. L"F:\\Games\\gakumas"
    const wchar_t* localRoot;      // e.g. L"F:\\Games\\gakumas\\gakumas-local"
    const wchar_t* pluginsRoot;    // e.g. L"F:\\Games\\gakumas\\gakumas-local\\plugins"
    void* gameAssemblyHandle;      // HMODULE to GameAssembly.dll
    
    // Host logger callbacks
    void (*logInfo)(const char* fmt, ...);
    void (*logWarn)(const char* fmt, ...);
    void (*logError)(const char* fmt, ...);

    // Environment queries
    bool (*isVRActive)(void);
    bool (*isVRifyLiveGazeActive)(void);
} GakumasModContext;

// Plugin lifecycle function pointer typedefs
typedef bool (*FnGakumasPlugin_GetInfo)(GakumasPluginInfo* outInfo);
typedef bool (*FnGakumasPlugin_Init)(const GakumasModContext* context);
typedef void (*FnGakumasPlugin_OnUnityReady)(void);
typedef void (*FnGakumasPlugin_OnLateUpdate)(void* activeActor, float deltaTime);
typedef void (*FnGakumasPlugin_OnRenderGui)(void);
typedef void (*FnGakumasPlugin_Shutdown)(void);

#ifdef __cplusplus
}
#endif
