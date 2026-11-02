// Real loader/runtime availability probe. Does not create a session, graphics
// device, swapchain, actions or frames. Run only when investigating a connected
// headset; all handles are released before this short-lived process exits.
#include "vr/openxr/OpenXrApi.hpp"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const HMODULE loader = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    auto get = loader ? reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        GetProcAddress(loader, "xrGetInstanceProcAddr")) : nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrResult result = XR_ERROR_RUNTIME_UNAVAILABLE;
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    XrGraphicsRequirementsD3D11KHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    std::string stage = "load-loader";
    bool available = false;
    const auto lookup = [&](const char* name) {
        PFN_xrVoidFunction function = nullptr;
        if (!get || XR_FAILED(get(instance, name, &function))) return PFN_xrVoidFunction{};
        return function;
    };
    if (get) {
        stage = "create-instance";
        const char* extension = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(info.applicationInfo.applicationName, "GakuVRify availability probe");
        info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        info.enabledExtensionCount = 1;
        info.enabledExtensionNames = &extension;
        const auto create = reinterpret_cast<PFN_xrCreateInstance>(lookup("xrCreateInstance"));
        result = create ? create(&info, &instance) : XR_ERROR_FUNCTION_UNSUPPORTED;
        if (XR_SUCCEEDED(result)) {
            const auto props = reinterpret_cast<PFN_xrGetInstanceProperties>(lookup("xrGetInstanceProperties"));
            if (props) props(instance, &properties);
            stage = "get-system";
            const auto getSystem = reinterpret_cast<PFN_xrGetSystem>(lookup("xrGetSystem"));
            XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
            systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            result = getSystem ? getSystem(instance, &systemInfo, &system) : XR_ERROR_FUNCTION_UNSUPPORTED;
            if (XR_SUCCEEDED(result)) {
                stage = "graphics-requirements";
                const auto requirements = reinterpret_cast<PFN_xrGetD3D11GraphicsRequirementsKHR>(
                    lookup("xrGetD3D11GraphicsRequirementsKHR"));
                result = requirements ? requirements(instance, system, &graphics) : XR_ERROR_FUNCTION_UNSUPPORTED;
                available = XR_SUCCEEDED(result);
                if (available) stage = "system-and-requirements-ready";
            }
        }
    }
    XrResult destroyed = XR_SUCCESS;
    if (instance != XR_NULL_HANDLE) {
        const auto destroy = reinterpret_cast<PFN_xrDestroyInstance>(lookup("xrDestroyInstance"));
        destroyed = destroy ? destroy(instance) : XR_ERROR_FUNCTION_UNSUPPORTED;
    }
    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    out << "{\"available\":" << (available ? "true" : "false")
        << ",\"stage\":" << std::quoted(stage)
        << ",\"xrResult\":" << result << ",\"destroyResult\":" << destroyed
        << ",\"runtimeName\":" << std::quoted(properties.runtimeName)
        << ",\"runtimeVersion\":" << properties.runtimeVersion
        << ",\"systemId\":" << system
        << ",\"adapterLuidHigh\":" << graphics.adapterLuid.HighPart
        << ",\"adapterLuidLow\":" << graphics.adapterLuid.LowPart
        << ",\"minimumFeatureLevel\":" << graphics.minFeatureLevel
        << ",\"sessionCreated\":false}\n";
    std::printf("OpenXR availability: %s stage=%s xrResult=%d runtime=%s\n",
        available ? "ready" : "unavailable", stage.c_str(), result, properties.runtimeName);
    if (loader) FreeLibrary(loader);
    return available && XR_SUCCEEDED(destroyed) ? 0 : 1;
}
