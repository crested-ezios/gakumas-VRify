// Metadata-only IL2CPP fixture for the production hosted-plugin smoke test.
// This is not Unity, an IL2CPP emulator, or a substitute for game acceptance.
// Unknown assemblies/classes/methods return null; optional VR paths must skip.
#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#define FIXTURE_EXPORT extern "C" __declspec(dllexport)
namespace {
std::atomic<std::uint64_t> domainQueries{}, attachCalls{}, assemblyQueries{},
    classQueries{}, methodQueries{}, managedInvocations{}, targetCalls{};
std::atomic<std::uint32_t> frameCounter{};
volatile LONG stubCalls[8]{};

// Method pointers are distinct native functions with real bodies so an actual
// host hook backend can lease them. None is called by metadata initialization.
__declspec(noinline) void* CameraMain(void*) {
    InterlockedIncrement(&stubCalls[0]); ++targetCalls; return nullptr;
}
__declspec(noinline) void PushCameraState(void*, void*, void*) {
    InterlockedIncrement(&stubCalls[1]); ++targetCalls;
}
__declspec(noinline) void EndCamera(void*, void*, void*) {
    InterlockedIncrement(&stubCalls[2]); ++targetCalls;
}
__declspec(noinline) void RenderLoop(void*, std::intptr_t, void*, void*) {
    InterlockedIncrement(&stubCalls[3]); ++targetCalls;
}
__declspec(noinline) void ActorLateUpdate(void*, void*) {
    InterlockedIncrement(&stubCalls[4]); ++targetCalls;
}
__declspec(noinline) int FrameCount() {
    InterlockedIncrement(&stubCalls[5]); return static_cast<int>(++frameCounter);
}
__declspec(noinline) float DeltaTime() {
    InterlockedIncrement(&stubCalls[6]); return 1.0F / 60.0F;
}

struct Type { const char* name; };
struct Class;
struct Method {
    // Compatible MethodInfo prefix: both the host and UnityResolve read the
    // function pointer at offset zero, then use the exports for metadata.
    void* methodPointer;
    void* invoker = nullptr;
    const char* name;
    Class* klass = nullptr;
    Type* result;
    const void* parameters = nullptr;
    void* definition = nullptr;
    void* genericContainer = nullptr;
    std::uint32_t token = 0;
    std::uint16_t flags;
    std::uint16_t iflags = 0;
    std::uint16_t slot = 0;
    std::uint8_t count;
    std::uint8_t bits = 0;
    Type* arguments[3]{};
};
struct Image;
struct Class {
    // Il2CppClass head used by the plugin's object/class utilities.
    Image* image;
    void* gcDescription = nullptr;
    const char* name;
    const char* namespaze;
    std::vector<Method*> methods;
};
struct Image {
    const char* name;
    std::vector<Class*> classes;
};

struct Fixture {
    int domain = 1;
    int thread = 2;
    Type voidType{"System.Void"}, cameraType{"UnityEngine.Camera"},
        cameraState{"Cinemachine.CameraState&"},
        context{"UnityEngine.Rendering.ScriptableRenderContext"},
        pipelineAsset{"UnityEngine.Rendering.RenderPipelineAsset"},
        intptrType{"System.IntPtr"}, objectType{"UnityEngine.Object"},
        intType{"System.Int32"}, floatType{"System.Single"};
    Image core{"UnityEngine.CoreModule.dll"}, cinema{"Cinemachine.dll"},
        campus{"campus-submodule.Runtime.dll"};
    Class camera{&core, nullptr, "Camera", "UnityEngine"},
        pipeline{&core, nullptr, "RenderPipeline", "UnityEngine.Rendering"},
        pipelineManager{&core, nullptr, "RenderPipelineManager", "UnityEngine.Rendering"},
        time{&core, nullptr, "Time", "UnityEngine"},
        brain{&cinema, nullptr, "CinemachineBrain", "Cinemachine"},
        actor{&campus, nullptr, "CampusActorController", "Campus.Common"};
    Method main{reinterpret_cast<void*>(&CameraMain), nullptr, "get_main", &camera,
        &cameraType, nullptr, nullptr, nullptr, 0, 0x10, 0, 0, 0},
        push{reinterpret_cast<void*>(&PushCameraState), nullptr, "PushStateToUnityCamera", &brain,
        &voidType, nullptr, nullptr, nullptr, 0, 0, 0, 0, 1, 0, {&cameraState}},
        end{reinterpret_cast<void*>(&EndCamera), nullptr, "EndCameraRendering", &pipeline,
        &voidType, nullptr, nullptr, nullptr, 0, 0x10, 0, 0, 2, 0, {&context, &cameraType}},
        loop{reinterpret_cast<void*>(&RenderLoop), nullptr, "DoRenderLoop_Internal", &pipelineManager,
        &voidType, nullptr, nullptr, nullptr, 0, 0x10, 0, 0, 3, 0, {&pipelineAsset, &intptrType, &objectType}},
        late{reinterpret_cast<void*>(&ActorLateUpdate), nullptr, "LateUpdate", &actor,
        &voidType, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0},
        frame{reinterpret_cast<void*>(&FrameCount), nullptr, "get_frameCount", &time,
        &intType, nullptr, nullptr, nullptr, 0, 0x10, 0, 0, 0},
        delta{reinterpret_cast<void*>(&DeltaTime), nullptr, "get_deltaTime", &time,
        &floatType, nullptr, nullptr, nullptr, 0, 0x10, 0, 0, 0};
    void* assemblies[3]{&core, &cinema, &campus};
    Fixture() {
        core.classes = {&camera, &pipeline, &pipelineManager, &time};
        cinema.classes = {&brain}; campus.classes = {&actor};
        camera.methods = {&main}; brain.methods = {&push};
        wchar_t omitCritical[2]{};
        if (GetEnvironmentVariableW(L"GAKU_VRLESS_OMIT_CRITICAL", omitCritical, 2) && omitCritical[0] == L'1')
            camera.methods.clear();
        pipeline.methods = {&end}; pipelineManager.methods = {&loop};
        actor.methods = {&late}; time.methods = {&frame, &delta};
    }
};
Fixture& Data() { static Fixture data; return data; }
bool Equal(const char* left, const char* right) {
    return left && right && std::strcmp(left, right) == 0;
}
}

FIXTURE_EXPORT void* il2cpp_domain_get() { ++domainQueries; return &Data().domain; }
FIXTURE_EXPORT void* il2cpp_thread_attach(void*) { ++attachCalls; return &Data().thread; }
FIXTURE_EXPORT void il2cpp_thread_detach(void*) {}
FIXTURE_EXPORT void** il2cpp_domain_get_assemblies(void*, std::size_t* count) {
    ++assemblyQueries; if (count) *count = 3; return Data().assemblies;
}
FIXTURE_EXPORT void* il2cpp_domain_assembly_open(void*, const char* name) {
    ++assemblyQueries;
    for (auto* entry : Data().assemblies) {
        auto* image = static_cast<Image*>(entry);
        if (Equal(image->name, name)) return image;
    }
    return nullptr;
}
FIXTURE_EXPORT void* il2cpp_assembly_get_image(void* assembly) { return assembly; }
FIXTURE_EXPORT const char* il2cpp_image_get_filename(void* image) {
    return image ? static_cast<Image*>(image)->name : "";
}
FIXTURE_EXPORT const char* il2cpp_image_get_name(void* image) {
    return il2cpp_image_get_filename(image);
}
FIXTURE_EXPORT std::size_t il2cpp_image_get_class_count(void* image) {
    return image ? static_cast<Image*>(image)->classes.size() : 0;
}
FIXTURE_EXPORT void* il2cpp_image_get_class(void* image, std::size_t index) {
    ++classQueries;
    if (!image) return nullptr;
    auto& entries = static_cast<Image*>(image)->classes;
    return index < entries.size() ? entries[index] : nullptr;
}
FIXTURE_EXPORT void* il2cpp_class_from_name(void* image, const char* namespaze, const char* name) {
    ++classQueries;
    if (image) for (auto* klass : static_cast<Image*>(image)->classes)
        if (Equal(klass->namespaze, namespaze) && Equal(klass->name, name)) return klass;
    return nullptr;
}
FIXTURE_EXPORT const char* il2cpp_class_get_name(void* klass) {
    return klass ? static_cast<Class*>(klass)->name : "";
}
FIXTURE_EXPORT const char* il2cpp_class_get_namespace(void* klass) {
    return klass ? static_cast<Class*>(klass)->namespaze : "";
}
FIXTURE_EXPORT void* il2cpp_class_get_parent(void*) { return nullptr; }
FIXTURE_EXPORT void* il2cpp_class_get_interfaces(void*, void**) { return nullptr; }
FIXTURE_EXPORT void* il2cpp_class_get_fields(void*, void**) { return nullptr; }
FIXTURE_EXPORT void* il2cpp_class_get_field_from_name(void*, const char*) { return nullptr; }
FIXTURE_EXPORT void* il2cpp_class_get_nested_types(void*, void**) { return nullptr; }
FIXTURE_EXPORT bool il2cpp_class_is_valuetype(void*) { return false; }
FIXTURE_EXPORT void* il2cpp_class_get_methods(void* klass, void** iterator) {
    ++methodQueries;
    if (!klass || !iterator) return nullptr;
    auto index = reinterpret_cast<std::uintptr_t>(*iterator);
    auto& methods = static_cast<Class*>(klass)->methods;
    if (index >= methods.size()) return nullptr;
    *iterator = reinterpret_cast<void*>(index + 1);
    return methods[index];
}
FIXTURE_EXPORT void* il2cpp_class_get_method_from_name(void* klass, const char* name, int arity) {
    ++methodQueries;
    if (klass) for (auto* method : static_cast<Class*>(klass)->methods)
        if (Equal(method->name, name) && (arity < 0 || method->count == arity)) return method;
    return nullptr;
}
FIXTURE_EXPORT const char* il2cpp_method_get_name(void* method) {
    return method ? static_cast<Method*>(method)->name : "";
}
FIXTURE_EXPORT void* il2cpp_method_get_return_type(void* method) {
    return method ? static_cast<Method*>(method)->result : nullptr;
}
FIXTURE_EXPORT std::uint32_t il2cpp_method_get_flags(void* method, std::uint32_t* implementation) {
    if (implementation) *implementation = 0;
    return method ? static_cast<Method*>(method)->flags : 0;
}
FIXTURE_EXPORT std::uint32_t il2cpp_method_get_param_count(void* method) {
    return method ? static_cast<Method*>(method)->count : 0;
}
FIXTURE_EXPORT const char* il2cpp_method_get_param_name(void*, std::uint32_t) { return "argument"; }
FIXTURE_EXPORT void* il2cpp_method_get_param(void* method, std::uint32_t index) {
    auto* entry = static_cast<Method*>(method);
    return entry && index < entry->count ? entry->arguments[index] : nullptr;
}
FIXTURE_EXPORT const char* il2cpp_type_get_name(void* type) {
    return type ? static_cast<Type*>(type)->name : "";
}
FIXTURE_EXPORT void* il2cpp_resolve_icall(const char*) { return nullptr; }
FIXTURE_EXPORT void* il2cpp_runtime_invoke(void*, void*, void**, void** exception) {
    ++managedInvocations; if (exception) *exception = nullptr; return nullptr;
}
FIXTURE_EXPORT void* il2cpp_object_unbox(void*) { return nullptr; }

FIXTURE_EXPORT std::uint64_t FakeGameAssembly_GetStat(const char* name) {
    if (Equal(name, "domain")) return domainQueries.load();
    if (Equal(name, "attach")) return attachCalls.load();
    if (Equal(name, "assembly")) return assemblyQueries.load();
    if (Equal(name, "class")) return classQueries.load();
    if (Equal(name, "method")) return methodQueries.load();
    if (Equal(name, "managedInvoke")) return managedInvocations.load();
    if (Equal(name, "targetCalls")) return targetCalls.load();
    return 0;
}
FIXTURE_EXPORT void* FakeGameAssembly_GetTarget(const char* name) {
    auto& f = Data();
    const Method* entries[]{&f.main, &f.push, &f.end, &f.loop, &f.late, &f.frame, &f.delta};
    for (const auto* method : entries) if (Equal(method->name, name)) return method->methodPointer;
    return nullptr;
}

// Numeric aliases keep the external smoke driver independent of C++ structs.
FIXTURE_EXPORT std::uint64_t GakuVrifyTestMetadataCounter(std::uint32_t id) {
    const char* names[]{"domain", "attach", "assembly", "class", "method", "managedInvoke", "targetCalls"};
    return id < sizeof(names) / sizeof(names[0]) ? FakeGameAssembly_GetStat(names[id]) : 0;
}
FIXTURE_EXPORT void* GakuVrifyTestMetadataHookTarget(std::uint32_t index) {
    const char* names[]{"get_main", "PushStateToUnityCamera", "EndCameraRendering", "DoRenderLoop_Internal"};
    return index < sizeof(names) / sizeof(names[0]) ? FakeGameAssembly_GetTarget(names[index]) : nullptr;
}
