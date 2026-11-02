// Test-only OpenXR loader seam. It is never part of a production/package target.
// The instance metadata in device-unavailable mode exists only to reach the real
// xrGetSystem gate. No mode produces a system, graphics binding, session or pose.
#include "vr/openxr/OpenXrApi.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>

namespace {
std::atomic<std::uint64_t> counters[7]{};
const auto fakeInstance = reinterpret_cast<XrInstance>(static_cast<std::uintptr_t>(0x56524C455353u));

bool DeviceUnavailableMode() noexcept {
    char value[64]{};
    const DWORD written = GetEnvironmentVariableA("GAKU_VRIFY_TEST_XR_MODE", value, sizeof(value));
    return written > 0 && written < sizeof(value) && std::strcmp(value, "device-unavailable") == 0;
}

XrResult XRAPI_CALL EnumerateExtensions(const char*, uint32_t capacity,
    uint32_t* count, XrExtensionProperties* properties) {
    ++counters[1];
    if (!count || (capacity && !properties)) return XR_ERROR_VALIDATION_FAILURE;
    *count = 1;
    if (!capacity) return XR_SUCCESS;
    properties[0].type = XR_TYPE_EXTENSION_PROPERTIES;
    properties[0].next = nullptr;
    strcpy_s(properties[0].extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
    properties[0].extensionVersion = XR_KHR_D3D11_enable_SPEC_VERSION;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL EnumerateLayers(uint32_t, uint32_t* count, XrApiLayerProperties*) {
    if (!count) return XR_ERROR_VALIDATION_FAILURE;
    *count = 0;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) {
    ++counters[2];
    if (!info || !instance) return XR_ERROR_VALIDATION_FAILURE;
    *instance = XR_NULL_HANDLE;
    if (!DeviceUnavailableMode()) return XR_ERROR_RUNTIME_UNAVAILABLE;
    *instance = fakeInstance;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroyInstance(XrInstance instance) {
    ++counters[5];
    return instance == fakeInstance ? XR_SUCCESS : XR_ERROR_HANDLE_INVALID;
}

XrResult XRAPI_CALL GetInstanceProperties(XrInstance instance, XrInstanceProperties* properties) {
    ++counters[3];
    if (instance != fakeInstance || !properties) return XR_ERROR_HANDLE_INVALID;
    properties->runtimeVersion = XR_MAKE_VERSION(0, 0, 1);
    strcpy_s(properties->runtimeName, "VR-less test backend: no device or session");
    return XR_SUCCESS;
}

XrResult XRAPI_CALL GetSystem(XrInstance instance, const XrSystemGetInfo*, XrSystemId* system) {
    ++counters[4];
    if (instance != fakeInstance || !system) return XR_ERROR_HANDLE_INVALID;
    *system = XR_NULL_SYSTEM_ID;
    return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
}

XrResult XRAPI_CALL ResultToString(XrInstance, XrResult result, char output[XR_MAX_RESULT_STRING_SIZE]) {
    const char* name = result == XR_ERROR_RUNTIME_UNAVAILABLE ? "XR_ERROR_RUNTIME_UNAVAILABLE" :
        result == XR_ERROR_FORM_FACTOR_UNAVAILABLE ? "XR_ERROR_FORM_FACTOR_UNAVAILABLE" :
        result == XR_SUCCESS ? "XR_SUCCESS" : "VRLESS_TEST_OTHER_RESULT";
    strcpy_s(output, XR_MAX_RESULT_STRING_SIZE, name);
    return XR_SUCCESS;
}

// Exact API signatures are retained even for functions which must never execute.
// Resolving the required dispatch table is not evidence of implemented APIs.
template<class> struct Unsupported;
template<class R, class... Args> struct Unsupported<R (XRAPI_CALL *)(Args...)> {
    static R XRAPI_CALL Call(Args...) {
        ++counters[6];
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }
};
}

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrGetInstanceProcAddr(
    XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    ++counters[0];
    if (!name || !function) return XR_ERROR_VALIDATION_FAILURE;
    *function = nullptr;
#define VRLESS_FUNCTION(api, implementation) \
    if (std::strcmp(name, #api) == 0) { \
        *function = reinterpret_cast<PFN_xrVoidFunction>(&implementation); return XR_SUCCESS; \
    }
    VRLESS_FUNCTION(xrEnumerateInstanceExtensionProperties, EnumerateExtensions)
    VRLESS_FUNCTION(xrEnumerateApiLayerProperties, EnumerateLayers)
    VRLESS_FUNCTION(xrCreateInstance, CreateInstance)
    if (instance != fakeInstance) return XR_ERROR_HANDLE_INVALID;
    VRLESS_FUNCTION(xrDestroyInstance, DestroyInstance)
    VRLESS_FUNCTION(xrGetInstanceProperties, GetInstanceProperties)
    VRLESS_FUNCTION(xrGetSystem, GetSystem)
    VRLESS_FUNCTION(xrResultToString, ResultToString)
#undef VRLESS_FUNCTION
#define VRLESS_UNSUPPORTED(api) \
    if (std::strcmp(name, #api) == 0) { \
        *function = reinterpret_cast<PFN_xrVoidFunction>(&Unsupported<PFN_##api>::Call); return XR_SUCCESS; \
    }
    VRLESS_UNSUPPORTED(xrPollEvent)
    VRLESS_UNSUPPORTED(xrGetSystemProperties)
    VRLESS_UNSUPPORTED(xrEnumerateEnvironmentBlendModes)
    VRLESS_UNSUPPORTED(xrGetD3D11GraphicsRequirementsKHR)
    VRLESS_UNSUPPORTED(xrCreateSession)
    VRLESS_UNSUPPORTED(xrDestroySession)
    VRLESS_UNSUPPORTED(xrEnumerateReferenceSpaces)
    VRLESS_UNSUPPORTED(xrCreateReferenceSpace)
    VRLESS_UNSUPPORTED(xrCreateActionSpace)
    VRLESS_UNSUPPORTED(xrLocateSpace)
    VRLESS_UNSUPPORTED(xrDestroySpace)
    VRLESS_UNSUPPORTED(xrEnumerateViewConfigurations)
    VRLESS_UNSUPPORTED(xrEnumerateViewConfigurationViews)
    VRLESS_UNSUPPORTED(xrEnumerateSwapchainFormats)
    VRLESS_UNSUPPORTED(xrCreateSwapchain)
    VRLESS_UNSUPPORTED(xrDestroySwapchain)
    VRLESS_UNSUPPORTED(xrEnumerateSwapchainImages)
    VRLESS_UNSUPPORTED(xrAcquireSwapchainImage)
    VRLESS_UNSUPPORTED(xrWaitSwapchainImage)
    VRLESS_UNSUPPORTED(xrReleaseSwapchainImage)
    VRLESS_UNSUPPORTED(xrBeginSession)
    VRLESS_UNSUPPORTED(xrEndSession)
    VRLESS_UNSUPPORTED(xrRequestExitSession)
    VRLESS_UNSUPPORTED(xrWaitFrame)
    VRLESS_UNSUPPORTED(xrBeginFrame)
    VRLESS_UNSUPPORTED(xrEndFrame)
    VRLESS_UNSUPPORTED(xrLocateViews)
    VRLESS_UNSUPPORTED(xrStringToPath)
    VRLESS_UNSUPPORTED(xrCreateActionSet)
    VRLESS_UNSUPPORTED(xrDestroyActionSet)
    VRLESS_UNSUPPORTED(xrCreateAction)
    VRLESS_UNSUPPORTED(xrDestroyAction)
    VRLESS_UNSUPPORTED(xrSuggestInteractionProfileBindings)
    VRLESS_UNSUPPORTED(xrAttachSessionActionSets)
    VRLESS_UNSUPPORTED(xrGetActionStateBoolean)
    VRLESS_UNSUPPORTED(xrGetActionStateFloat)
    VRLESS_UNSUPPORTED(xrGetActionStateVector2f)
    VRLESS_UNSUPPORTED(xrGetActionStatePose)
    VRLESS_UNSUPPORTED(xrSyncActions)
#undef VRLESS_UNSUPPORTED
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

extern "C" __declspec(dllexport) std::uint64_t GakuVrifyTestXrCounter(uint32_t id) {
    return id < 7 ? counters[id].load() : 0;
}
