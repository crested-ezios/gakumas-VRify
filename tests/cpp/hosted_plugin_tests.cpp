#include "../../src/host/GakuVrifyCameraBegin.h"
// Loads the production plugin in a plain native process. Every scenario runs
// in its own process so process-lifetime provider state cannot hide failures.
#include "GakuPlugin.h"
#include "GakuPaths.h"
#include "../../src/host/GakuVrifyDiagnostics.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Deliberately consume the published C layout rather than a provider helper.
// A mismatch in the real registered service is rejected by RegisterService.
struct VrifyService {
    uint32_t abiVersion;
    uint32_t structSize;
    uint32_t (*isVrActive)();
    uint32_t (*isLiveGazeActive)();
};
static_assert(sizeof(VrifyService) == 24);

struct MockHost {
    std::wstring gameRoot, localRoot, pluginsRoot;
    std::string missing;
    std::vector<std::string> queried, logs;
    VrifyService vrify{};
    size_t serviceRegistrations = 0;
    size_t acquireCalls = 0, batchCalls = 0, releaseCalls = 0;
    size_t actorRegistrations = 0, actorUnregistrations = 0;
    bool rejectRegistration = false;
    bool pathsAvailable = false;
};
GakuStatus GetPath(void* context, GakuPathKind kind, wchar_t* output, uint32_t capacity, uint32_t* required) {
    const auto& host = *static_cast<MockHost*>(context);
    if (!required || (kind != GAKU_PATH_CONFIGS && kind != GAKU_PATH_LOGS && kind != GAKU_PATH_PLUGIN_DATA)) return GAKU_E_INVALID_ARGUMENT;
    const auto path = std::filesystem::path(host.localRoot) /
        (kind == GAKU_PATH_CONFIGS ? L"Configs" : kind == GAKU_PATH_LOGS ? L"Logs" : L"PluginData") / L"gaku.vrify";
    const auto value = path.wstring();
    *required = static_cast<uint32_t>(value.size() + 1);
    if (capacity < *required) return GAKU_E_SIZE;
    if (!output) return GAKU_E_INVALID_ARGUMENT;
    std::copy(value.c_str(), value.c_str() + value.size() + 1, output);
    return GAKU_OK;
}
GakuStatus EnsurePath(void*, GakuPathKind) { return GAKU_OK; }
void WriteLog(void* context, GakuLogLevel, const char* message) {
    auto& host = *static_cast<MockHost*>(context);
    Check(message != nullptr, "logger received null message");
    host.logs.emplace_back(message);
}
void* NoGameAssembly() { return nullptr; }
GakuStatus Acquire(void* context, void*, void*, void**) {
    ++static_cast<MockHost*>(context)->acquireCalls;
    return GAKU_E_UNAVAILABLE;
}
GakuStatus AcquireBatch(void* context, uint32_t, const GakuHookRequest*) {
    ++static_cast<MockHost*>(context)->batchCalls;
    return GAKU_E_UNAVAILABLE;
}
GakuStatus Release(void* context, void*) {
    ++static_cast<MockHost*>(context)->releaseCalls;
    return GAKU_OK;
}
GakuStatus RegisterActor(void* context, const GakuActorEntryBinding* binding) {
    Check(binding != nullptr, "null actor binding");
    Check(binding->abiVersion == GAKU_SERVICE_VERSION &&
        binding->structSize == sizeof(GakuActorEntryBinding), "invalid actor binding header");
    Check(binding->callback != nullptr, "null actor callback");
    Check(binding->phase == GAKU_ACTOR_ENTRY_BEFORE ||
        binding->phase == GAKU_ACTOR_ENTRY_AFTER, "invalid actor phase");
    ++static_cast<MockHost*>(context)->actorRegistrations;
    return GAKU_OK;
}
GakuStatus UnregisterActor(void* context, uint32_t) {
    ++static_cast<MockHost*>(context)->actorUnregistrations;
    return GAKU_OK;
}
GakuStatus Enqueue(void*, GakuTaskFn, void*) {
    throw std::runtime_error("disabled plugin enqueued game work");
}
uint32_t IsMainThread(void*) { return 1; }

template<class T>
GakuStatus CopyTable(const T& table, uint32_t version, uint32_t size, void* output) {
    if (version != GAKU_SERVICE_VERSION) return GAKU_E_VERSION;
    if (!output) return GAKU_E_INVALID_ARGUMENT;
    if (size < sizeof(T)) return GAKU_E_SIZE;
    std::memcpy(output, &table, sizeof(T));
    return GAKU_OK;
}
GakuStatus QueryService(void* context, const char* name, uint32_t version,
    uint32_t size, void* output) {
    auto& host = *static_cast<MockHost*>(context);
    Check(name != nullptr, "null service query");
    host.queried.emplace_back(name);
    if (host.missing == name) return GAKU_E_UNAVAILABLE;
    if (std::strcmp(name, GAKU_SERVICE_PATHS) == 0 && host.pathsAvailable) {
        const GakuPathsV1 service{GAKU_PATHS_VERSION, sizeof(service), &host, GetPath, EnsurePath};
        return CopyTable(service, version, size, output);
    }
    if (std::strcmp(name, GAKU_SERVICE_LOG) == 0) {
        const GakuLogService service{GAKU_SERVICE_VERSION, sizeof(service), &host, WriteLog};
        return CopyTable(service, version, size, output);
    }
    if (std::strcmp(name, GAKU_SERVICE_RUNTIME) == 0) {
        const GakuRuntimeService service{GAKU_SERVICE_VERSION, sizeof(service),
            host.gameRoot.c_str(), host.localRoot.c_str(), host.pluginsRoot.c_str(), NoGameAssembly};
        return CopyTable(service, version, size, output);
    }
    if (std::strcmp(name, GAKU_SERVICE_HOOKS) == 0) {
        const GakuHookService service{GAKU_SERVICE_VERSION, sizeof(service), &host,
            Acquire, Release, AcquireBatch};
        return CopyTable(service, version, size, output);
    }
    if (std::strcmp(name, GAKU_SERVICE_ACTOR_ENTRY) == 0) {
        const GakuActorEntryService service{GAKU_SERVICE_VERSION, sizeof(service), &host,
            RegisterActor, UnregisterActor};
        return CopyTable(service, version, size, output);
    }
    if (std::strcmp(name, GAKU_SERVICE_MAIN_THREAD) == 0) {
        const GakuMainThreadService service{GAKU_SERVICE_VERSION, sizeof(service), &host,
            Enqueue, IsMainThread};
        return CopyTable(service, version, size, output);
    }
    return GAKU_E_UNAVAILABLE;
}
GakuStatus RegisterService(void* context, const char* name, uint32_t size, const void* table) {
    auto& host = *static_cast<MockHost*>(context);
    if (name && std::strcmp(name, GAKU_VRIFY_CAMERA_BEGIN_SERVICE) == 0) {
        Check(table && size == sizeof(GakuVrifyCameraBeginService), "invalid camera-begin table");
        const auto& bridge = *static_cast<const GakuVrifyCameraBeginService*>(table);
        Check(bridge.abiVersion == 1 && bridge.structSize == sizeof(bridge) && bridge.context &&
            bridge.getTarget && bridge.subscribe && bridge.unsubscribe, "invalid camera-begin functions");
        Check(!bridge.getTarget(bridge.context, 0) && !bridge.getTarget(bridge.context, 1),
            "camera targets published before runtime initialization");
        return GAKU_OK;
    }
    if (name && std::strcmp(name, GAKU_VRIFY_DIAGNOSTICS_SERVICE) == 0) {
        Check(table && size == sizeof(GakuVrifyDiagnosticsService), "invalid diagnostics table");
        const auto& diagnostics = *static_cast<const GakuVrifyDiagnosticsService*>(table);
        Check(diagnostics.abiVersion == 1 && diagnostics.getSnapshot, "invalid diagnostics service");
        GakuVrifySnapshot snapshot{};
        Check(diagnostics.getSnapshot(sizeof(snapshot), &snapshot) == GAKU_OK,
            "diagnostics snapshot failed");
        return GAKU_OK;
    }
    Check(name && std::strcmp(name, "gaku.vrify.service") == 0,
        "plugin registered unexpected service name");
    Check(table && size == sizeof(VrifyService), "unexpected VR service size");
    VrifyService service{};
    std::memcpy(&service, table, sizeof(service));
    Check(service.abiVersion == 1 && service.structSize == sizeof(service),
        "invalid VR service version/size");
    Check(service.isVrActive && service.isLiveGazeActive, "missing VR status callbacks");
    ++host.serviceRegistrations;
    if (host.rejectRegistration) return GAKU_E_CONFLICT;
    Check(host.serviceRegistrations == 1, "duplicate VR service registration");
    host.vrify = service;
    return GAKU_OK;
}

GakuPluginDescriptor CheckAbi(FnGakuPlugin_Query query) {
    GakuPluginDescriptor sentinel{};
    std::memset(&sentinel, 0xA5, sizeof(sentinel));
    auto output = sentinel;
    Check(query(999, sizeof(output), &output) == GAKU_E_VERSION,
        "unsupported ABI was accepted");
    Check(std::memcmp(&output, &sentinel, sizeof(output)) == 0,
        "ABI rejection modified output");
    Check(query(GAKU_ABI_VERSION, sizeof(output) - 1, &output) == GAKU_E_SIZE,
        "short descriptor was accepted");
    Check(std::memcmp(&output, &sentinel, sizeof(output)) == 0,
        "short descriptor rejection modified output");
    Check(query(GAKU_ABI_VERSION, sizeof(output), nullptr) == GAKU_E_INVALID_ARGUMENT,
        "null descriptor output was accepted");
    Check(query(GAKU_ABI_VERSION, sizeof(output), &output) == GAKU_OK,
        "production plugin query failed");
    Check(output.abiVersion == GAKU_ABI_VERSION && output.structSize == sizeof(output),
        "invalid descriptor header");
    Check(output.id && output.id[0] && output.name && output.name[0] && output.version,
        "missing plugin identity");
    Check(output.activate && output.onRuntimeReady && output.shutdown,
        "missing lifecycle callback");
    return output;
}

BOOL CALLBACK CountWindows(HWND window, LPARAM raw) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId()) ++*reinterpret_cast<size_t*>(raw);
    return TRUE;
}
size_t WindowCount() {
    size_t count = 0;
    EnumWindows(CountWindows, reinterpret_cast<LPARAM>(&count));
    return count;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        Check(argc == 4, "usage: hosted-plugin-tests <dll> <scenario> <test-root>");
        const std::filesystem::path plugin = std::filesystem::absolute(argv[1]);
        const std::wstring scenario = argv[2];
        const auto testRoot = std::filesystem::absolute(argv[3]);
        Check(GetModuleHandleW(L"GameAssembly.dll") == nullptr,
            "test process unexpectedly contains a game assembly");
        const size_t initialWindows = WindowCount();
        // Do not FreeLibrary: the ABI explicitly retains provider tables and
        // detour state until process exit, including unsuccessful activation.
        const auto module = LoadLibraryExW(plugin.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            std::fprintf(stderr, "LoadLibraryExW failed: %lu\n", GetLastError());
            throw std::runtime_error("cannot load production plugin");
        }
        const auto query = reinterpret_cast<FnGakuPlugin_Query>(GetProcAddress(module, "GakuPlugin_Query"));
        Check(query != nullptr, "production plugin lacks GakuPlugin_Query");
        Check(GetProcAddress(module, "GmlPlugin_Query") == nullptr &&
            GetProcAddress(module, "GakumasPlugin_Query") == nullptr &&
            GetProcAddress(module, "GetFileVersionInfoW") == nullptr,
            "plugin still exposes a legacy host/proxy entry");
        const auto descriptor = CheckAbi(query);
        if (scenario == L"abi") {
            void* rejectedState = nullptr;
            Check(descriptor.activate(nullptr, &rejectedState) == GAKU_E_INVALID_ARGUMENT,
                "null host was accepted");
            GakuHostApi invalidHost{GAKU_ABI_VERSION, sizeof(GakuHostApi), nullptr,
                QueryService, RegisterService};
            Check(descriptor.activate(&invalidHost, nullptr) == GAKU_E_INVALID_ARGUMENT,
                "null state output was accepted");
            invalidHost.abiVersion = 999;
            Check(descriptor.activate(&invalidHost, &rejectedState) == GAKU_E_VERSION,
                "incompatible host ABI was accepted");
            invalidHost.abiVersion = GAKU_ABI_VERSION;
            --invalidHost.structSize;
            Check(descriptor.activate(&invalidHost, &rejectedState) == GAKU_E_SIZE,
                "short host API was accepted");
            invalidHost.structSize = sizeof(invalidHost);
            invalidHost.queryService = nullptr;
            Check(descriptor.activate(&invalidHost, &rejectedState) == GAKU_E_INVALID_ARGUMENT,
                "missing query capability was accepted");
            invalidHost.queryService = QueryService;
            invalidHost.registerService = nullptr;
            Check(descriptor.activate(&invalidHost, &rejectedState) == GAKU_E_INVALID_ARGUMENT,
                "missing provider-registration capability was accepted");
            Check(WindowCount() == initialWindows, "DLL loading/query created a window");
            std::puts("PASS: production DLL load and ABI negotiation, rejection leaves output untouched");
            return 0;
        }
        std::filesystem::create_directories(testRoot / "gakumas-vr");
        if (scenario != L"fresh-paths" && scenario != L"mixed-legacy" && scenario != L"mixed-invalid-type") {
            std::ofstream config(testRoot / "gakumas-vr" / "config.json");
            Check(config.good(), "cannot write test VR configuration");
            if (scenario == L"invalid-vr-config") config << "{invalid JSON";
            else if (scenario == L"future-schema") config << "{\"schemaVersion\":2,\"vrEnabled\":true}";
            else if (scenario == L"enabled-without-runtime" || scenario == L"new-priority" || scenario == L"new-without-service" ||
                scenario == L"managed-no-service" || scenario == L"standard-prepared" || scenario == L"standard-no-service") config << "{\"vrEnabled\":true}";
            else config << "{\"vrEnabled\":false}";
        }
        MockHost mock;
        mock.gameRoot = testRoot.wstring();
        mock.localRoot = (testRoot / "gakumas-local").wstring();
        mock.pluginsRoot = (testRoot / "gakumas-local" / "plugins").wstring();
        mock.pathsAvailable = scenario == L"fresh-paths" || scenario == L"new-priority" || scenario == L"mixed-legacy" || scenario == L"standard-prepared";
        if (scenario == L"mixed-legacy" || scenario == L"mixed-invalid-type") {
            std::filesystem::create_directories(mock.localRoot);
            std::ofstream source(testRoot / "gakumas-local" / "localizationConfig.json");
            if (scenario == L"mixed-invalid-type") source << "{\"vrEnabled\":false,\"vrMenuLanguage\":\"ja\"}";
            else source << "{\"vrEnabled\":false,\"vrDebugEnabled\":true,\"vrDiagnosticsEnabled\":false,"
                    "\"vrProbeOnly\":false,\"vrNativeOnly\":true,\"vrMenuLanguage\":2,\"downloadUrl\":\"sentinel\"}";
        }
        if (scenario == L"new-priority" || scenario == L"new-without-service") {
            const auto managed = testRoot / "gakumas-local" / "Configs" / "gaku.vrify";
            std::filesystem::create_directories(managed);
            std::ofstream(managed / "config.json") << "{\"schemaVersion\":1,\"vrEnabled\":false}";
        }
        if (scenario == L"managed-no-service") {
            std::ofstream(testRoot / "gakumas-vr" / "config-layout.json") <<
                "{\"schemaVersion\":1,\"layout\":\"paths-v1\",\"phase\":\"completed\"}";
        }
        if (scenario.starts_with(L"missing:")) {
            const auto missing = scenario.substr(8);
            for (const auto character : missing) {
                Check(character <= 127, "non-ASCII mock service name");
                mock.missing.push_back(static_cast<char>(character));
            }
        }
        mock.rejectRegistration = scenario == L"service-conflict";
        const GakuHostApi host{GAKU_ABI_VERSION, sizeof(host), &mock, QueryService, RegisterService};
        void* state = nullptr;
        const auto status = descriptor.activate(&host, &state);
        if (!mock.missing.empty() || mock.rejectRegistration) {
            Check(status != GAKU_OK, "activation succeeded without required capability");
            Check(mock.acquireCalls == 0 && mock.batchCalls == 0,
                "failed activation attempted game/graphics hooks");
            Check(WindowCount() == initialWindows, "failed activation created a window");
            std::puts("PASS: required-service/registration failure disables only this plugin");
            return 0;
        }
        Check(scenario == L"disabled" || scenario == L"invalid-vr-config" ||
            scenario == L"enabled-without-runtime" || scenario == L"fresh-paths" ||
            scenario == L"mixed-legacy" || scenario == L"new-priority" ||
            scenario == L"future-schema" || scenario == L"managed-no-service" ||
            scenario == L"mixed-invalid-type" || scenario == L"new-without-service" ||
            scenario == L"standard-prepared" || scenario == L"standard-no-service", "unknown test scenario");
        Check(status == GAKU_OK, "safe production plugin failed activation");
        Check(mock.serviceRegistrations == 1 && mock.vrify.isVrActive && mock.vrify.isLiveGazeActive,
            "status service was not registered");
        Check(mock.vrify.isVrActive() == 0 && mock.vrify.isLiveGazeActive() == 0,
            "plugin advertised active VR/gaze without a session");
        if (scenario == L"standard-prepared" || scenario == L"standard-no-service")
            Check(std::any_of(mock.logs.begin(), mock.logs.end(), [](const std::string& message) {
                return message.find("activated with VR disabled") != std::string::npos;
            }), "restart used enabled legacy residue instead of standard settings or fail-closed policy");
        descriptor.onRuntimeReady(state);
        if (descriptor.onTick) descriptor.onTick(state, 1.0F / 60.0F);
        // No fake actor pointer is passed: this harness never fabricates managed
        // objects or a game module, including the disabled path.
        Check(mock.acquireCalls == 0 && mock.batchCalls == 0,
            "disabled/invalid/no-runtime plugin attempted a graphics/Unity detour");
        Check(mock.vrify.isVrActive() == 0 && mock.vrify.isLiveGazeActive() == 0,
            "safe runtime-ready path advertised active VR/gaze");
        descriptor.shutdown(state);
        Check(mock.vrify.isVrActive() == 0 && mock.vrify.isLiveGazeActive() == 0,
            "service reports active state after shutdown");
        Check(!std::filesystem::exists(testRoot / "gakumas-local" / "config.json") &&
            (scenario == L"mixed-legacy" || scenario == L"mixed-invalid-type" ||
                !std::filesystem::exists(testRoot / "gakumas-local" / "localizationConfig.json")),
            "hosted plugin wrote Localify business configuration");
        if (scenario == L"fresh-paths") {
            const auto freshConfig = testRoot / "gakumas-local" / "Configs" / "gaku.vrify" / "config.json";
            Check(std::filesystem::exists(freshConfig),
                "fresh paths-service config was not initialized");
            std::ifstream input(freshConfig);
            const std::string text((std::istreambuf_iterator<char>(input)), {});
            Check(text.find("\"vrEnabled\": true") != std::string::npos &&
                text.find("\"vrNativeOnly\": false") != std::string::npos,
                "fresh config used safe memory defaults instead of the embedded release template");
            Check(!std::filesystem::exists(testRoot / "gakumas-vr" / "config.json"),
                "fresh paths-service config created a legacy activity file");
            Check(!std::filesystem::exists(testRoot / "gakumas-vr" / "config-layout.json") &&
                std::filesystem::exists(testRoot / "gakumas-local" / "PluginData" / "gaku.vrify" / "active-layout.json"),
                "fresh standard state was not consolidated under PluginData");
        }
        if (scenario == L"mixed-invalid-type")
            Check(!std::filesystem::exists(testRoot / "gakumas-vr" / "config.json"),
                "invalid known mixed field type created an active configuration");
        if (scenario == L"mixed-legacy") {
            std::ifstream input(testRoot / "gakumas-vr" / "config.json");
            const std::string text((std::istreambuf_iterator<char>(input)), {});
            Check(text.find("\"vrMenuLanguage\": 2") != std::string::npos &&
                text.find("downloadUrl") == std::string::npos &&
                text.find("vrDebugEnabled") == std::string::npos &&
                text.find("\"vrDiagnosticsEnabled\": false") != std::string::npos,
                "mixed VR settings were not filtered/normalized with new names preferred");
        }
        Check(WindowCount() == initialWindows, "safe lifecycle created a UI window");
        Check(GetModuleHandleW(L"GameAssembly.dll") == nullptr, "safe lifecycle loaded a game module");
        if (scenario == L"invalid-vr-config")
            std::puts("PASS: production malformed VR config fails closed through runtime-ready/shutdown; no game/hooks/UI");
        else if (scenario == L"enabled-without-runtime")
            std::puts("PASS: production enabled config without a runtime acquires no leases and reports inactive VR/gaze");
        else
            std::puts("PASS: production disabled-config activate/runtime-ready/tick/shutdown and live status service; no game/hooks/UI");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
