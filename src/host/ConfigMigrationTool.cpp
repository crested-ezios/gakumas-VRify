// Offline configuration utility. It never loads the game, VR runtime or plugin.
#include "vr/config/VrConfigStartup.hpp"
#include "VrConfigDefaults.hpp"
#include <Windows.h>
#include <TlHelp32.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace GakumasLocal::Config;
namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace {
std::string ReadFile(const fs::path& path) {
    if (!fs::is_regular_file(path) || fs::file_size(path) > 4 * 1024 * 1024)
        throw std::runtime_error("plan file must be a regular file smaller than 4 MiB");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read migration plan");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
VrConfigManagedOptions Options(const fs::path& gameRoot, const fs::path& localRoot) {
    VrConfigManagedOptions options;
    options.startup.legacyDirectory = gameRoot / L"gakumas-vr";
    options.startup.mixedConfig = localRoot / L"localizationConfig.json";
    options.startup.managedConfigRoot = localRoot / L"Configs" / L"gaku.vrify";
    options.pluginDataRoot = localRoot / L"PluginData" / L"gaku.vrify";
    options.logRoot = localRoot / L"Logs" / L"gaku.vrify";
    options.startup.releaseDefaults = Json::parse(kVrReleaseDefaults);
    for (auto it = options.startup.releaseDefaults.begin(); it != options.startup.releaseDefaults.end(); ++it)
        options.startup.knownKeys.push_back(it.key());
    return options;
}
void RequireStopped(const fs::path& gameRoot) {
    const auto expected = (gameRoot / L"gakumas.exe").lexically_normal().wstring();
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot verify the game is stopped");
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool running = false, uncertain = false;
    if (!Process32FirstW(snapshot, &process)) {
        CloseHandle(snapshot);
        throw std::runtime_error("cannot enumerate processes to verify the game is stopped");
    }
    do {
        if (_wcsicmp(process.szExeFile, L"gakumas.exe") != 0) continue;
        const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.th32ProcessID);
        if (!handle) { uncertain = true; continue; }
        std::wstring name(32768, L'\0');
        DWORD size = static_cast<DWORD>(name.size());
        if (!QueryFullProcessImageNameW(handle, 0, name.data(), &size)) uncertain = true;
        else {
            name.resize(size);
            if (_wcsicmp(name.c_str(), expected.c_str()) == 0) running = true;
        }
        CloseHandle(handle);
    } while (Process32NextW(snapshot, &process));
    CloseHandle(snapshot);
    if (running || uncertain) throw std::runtime_error("close the game before applying configuration migration");
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 4 && std::wstring_view(argv[1]) == L"--plan") {
            const auto gameRoot = fs::absolute(argv[2]).lexically_normal();
            const auto output = fs::absolute(argv[3]).lexically_normal();
            const auto relative = output.lexically_relative(gameRoot);
            if (!relative.empty() && *relative.begin() != L"..")
                throw std::runtime_error("write the inspection plan outside the game installation");
            if (!fs::is_regular_file(gameRoot / L"gakumas.exe"))
                throw std::runtime_error("selected installation does not contain gakumas.exe");
            auto options = Options(gameRoot, gameRoot / L"gakumas-local");
            const auto plan = PlanVrConfigMigration(options);
            std::string error;
            if (!WriteVrConfigDocument(output, plan, false, error)) throw std::runtime_error(error);
            std::cout << "PLAN action=" << plan.at("action").get<std::string>()
                << " sha256=" << VrConfigPlanHash(plan)
                << " (installation unchanged; back up original data before --apply)\n";
            return 0;
        }
        if (argc == 5 && std::wstring_view(argv[1]) == L"--apply" &&
            std::wstring_view(argv[3]) == L"--expect-sha256") {
            const auto bytes = ReadFile(fs::absolute(argv[2]));
            auto plan = Json::parse(bytes, nullptr, false);
            if (plan.is_discarded()) throw std::runtime_error("invalid migration plan JSON");
            std::string expected;
            for (const auto character : std::wstring_view(argv[4])) {
                if (character > 127) throw std::runtime_error("plan hash must be ASCII SHA-256");
                expected += static_cast<char>(character >= L'A' && character <= L'F' ? character + 32 : character);
            }
            const auto gameRoot = fs::u8path(plan.at("legacyDirectory").get<std::string>()).parent_path();
            if (!fs::is_regular_file(gameRoot / L"gakumas.exe"))
                throw std::runtime_error("selected installation does not contain gakumas.exe");
            RequireStopped(gameRoot);
            const auto options = Options(gameRoot, gameRoot / L"gakumas-local");
            const auto result = ApplyVrConfigMigration(options, plan, expected);
            if (!result.success) throw std::runtime_error(result.outcome);
            std::cout << "APPLIED " << result.outcome << "\n";
            return 0;
        }
        std::cerr << "Usage: vr-config-migration --plan <GameRoot> <new-plan.json>\n"
            "       vr-config-migration --apply <plan.json> --expect-sha256 <printed-plan-sha256>\n";
        return 2;
    } catch (const Json::exception&) { std::cerr << "ERROR invalid plan/configuration structure; original retained\n"; }
      catch (const std::exception& error) { std::cerr << "ERROR " << error.what() << '\n'; }
    return 1;
}
