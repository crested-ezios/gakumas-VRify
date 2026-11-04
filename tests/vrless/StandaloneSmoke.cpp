// Synthetic executable (gakumas.exe), never a game host.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
namespace {
using StopRuntime = void (*)() noexcept;
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    Check(static_cast<bool>(input), "cannot read synthetic fixture file");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string Logs(const fs::path& root) {
    std::string result;
    const auto directory = root / L"gakumas-vr" / L"logs";
    if (fs::exists(directory)) {
        for (const auto& entry : fs::directory_iterator(directory))
            if (entry.is_regular_file() && entry.path().extension() == L".log")
                result += Read(entry.path());
    }
    return result;
}
void CheckImports(HMODULE module) {
    const auto base = reinterpret_cast<const unsigned char*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    Check(dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE,
        "loaded standalone image has invalid PE headers");
    const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    Check(rva != 0, "standalone image has no import table");
    auto entry = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
    for (; entry->Name; ++entry) {
        std::string name(reinterpret_cast<const char*>(base + entry->Name));
        for (auto& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        Check(name.find("gakuloader") == std::string::npos,
            "standalone image imports a GakuLoader DLL");
    }
}
void CheckProxy(HMODULE module) {
    constexpr const char* expected[] = {
        "GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
        "GetFileVersionInfoExW", "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA",
        "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW", "GetFileVersionInfoW",
        "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW",
        "VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW"
    };
    for (const auto name : expected)
        Check(GetProcAddress(module, name) != nullptr, "version proxy export missing");
    Check(!GetProcAddress(module, "GakuPlugin_Query"), "standalone exports the hosted plugin entry");
    Check(!GetProcAddress(module, "GakumasVr_GetImGuiContext"), "standalone exports the retired GUI entry");
    using LanguageName = DWORD (WINAPI*)(DWORD, LPWSTR, DWORD);
    const auto languageName = reinterpret_cast<LanguageName>(GetProcAddress(module, "VerLanguageNameW"));
    wchar_t text[256]{};
    Check(languageName(0x0409, text, 256) != 0, "version proxy did not forward to system version.dll");
}
}

int wmain() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    StopRuntime stop = nullptr;
    try {
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        Check(length && length < executable.size(), "cannot locate synthetic executable");
        executable.resize(length);
        const auto root = fs::path(executable).parent_path();
        Check(fs::path(executable).filename() == L"gakumas.exe" &&
            root.parent_path().parent_path().filename() == L"standalone-smoke" &&
            Read(root / L".standalone-smoke-fixture") == "SYNTHETIC-ONLY\n",
            "runner is not in its standalone-smoke fixture");
        const auto originalCwd = fs::current_path();
        Check(originalCwd != root, "smoke requires a working directory different from the executable root");
        Check(!fs::exists(root / L"openxr_loader.dll") && !GetModuleHandleW(L"openxr_loader.dll"),
            "missing-loader scenario has an OpenXR loader");
        Check(!fs::exists(root / L"GameAssembly.dll") && !GetModuleHandleW(L"GameAssembly.dll"),
            "native-only smoke must not load a game assembly");
        const std::array<fs::path, 3> configs = {root / L"gakumas-local/config.json",
            root / L"gakumas-local/localizationConfig.json", root / L"gakumas-vr/config.json"};
        std::array<std::string, 3> originals;
        for (size_t i = 0; i < configs.size(); ++i) originals[i] = Read(configs[i]);

        // The real DllMain launches the original standalone bootstrap. Hold the
        // DLL for process lifetime; unloading it is outside this test's contract.
        const auto module = LoadLibraryExW((root / L"version.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        Check(module != nullptr, "cannot load the production standalone version.dll");
        stop = reinterpret_cast<StopRuntime>(GetProcAddress(module, "GakumasVrStopRuntimeForTest"));
        Check(stop != nullptr, "standalone runtime stop seam missing");
        Check(LoadLibraryW(L"d3d11.dll") != nullptr, "cannot load system D3D11");
        const auto deadline = GetTickCount64() + 12000;
        std::string log;
        do {
            log = Logs(root);
            if (log.find("VR unavailable: loader-missing") != std::string::npos) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        } while (GetTickCount64() < deadline);
        Check(log.find("VR unavailable: loader-missing") != std::string::npos,
            "real standalone bootstrap did not reach the missing-loader state");
        stop();
        stop = nullptr;
        Check(Logs(root).find("worker stopped") != std::string::npos,
            "standalone worker did not stop before process exit");
        CheckProxy(module);
        CheckImports(module);
        Check(!GetModuleHandleW(L"GakuLoader.dll") && !GetModuleHandleW(L"GakuLocalify.dll") &&
            !GetModuleHandleW(L"GakuVRify.dll"), "unexpected hosted module loaded");
        for (size_t i = 0; i < configs.size(); ++i)
            Check(Read(configs[i]) == originals[i], "standalone changed original configuration bytes");
        Check(fs::current_path() == originalCwd &&
            !fs::exists(originalCwd / L"gakumas-local") && !fs::exists(originalCwd / L"gakumas-vr"),
            "standalone changed CWD or rooted data in the unrelated working directory");
        std::puts("STANDALONE_PASS bootstrap=real native-only=1 xr=loader-missing worker=stopped proxy=forwarded config=unchanged cwd=unchanged");
        return 0;
    } catch (const std::exception& error) {
        if (stop) stop();
        std::fprintf(stderr, "STANDALONE_FAIL %s\n", error.what());
        return 1;
    }
}
