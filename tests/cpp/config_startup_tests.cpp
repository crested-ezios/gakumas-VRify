#include <Windows.h>
#include "vr/config/VrConfigStartup.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using Json = nlohmann::json;
using namespace GakumasLocal::Config;

namespace {
fs::path root;
unsigned assertions = 0;
unsigned cases = 0;
unsigned failures = 0;

void Check(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
void WriteBytes(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << bytes;
    output.flush();
    if (!output) throw std::runtime_error("fixture write failed");
}
std::string ReadBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
Json ReadJson(const fs::path& path) { return Json::parse(ReadBytes(path)); }
void WriteJson(const fs::path& path, const Json& document) { WriteBytes(path, document.dump(4) + "\n"); }
void ValidateTypes(const Json& document) {
    for (const auto* name : {"vrEnabled", "vrNativeOnly", "vrDiagnosticsEnabled"})
        if (document.contains(name)) (void)document.at(name).get<bool>();
    for (const auto* name : {"vrWorldScale", "vrPanelOffsetX"})
        if (document.contains(name)) (void)document.at(name).get<double>();
}
VrConfigStartupOptions Options(const char* name, bool managed = true, bool enabled = true) {
    const auto installation = root / name;
    VrConfigStartupOptions options;
    options.legacyDirectory = installation / "gakumas-vr";
    options.mixedConfig = installation / "gakumas-local" / "localizationConfig.json";
    if (managed) options.managedConfigRoot = installation / "gakumas-local" / "Configs" / "gaku.vrify";
    options.releaseDefaults = {{"schemaVersion", 1}, {"vrEnabled", enabled},
        {"vrNativeOnly", !enabled}, {"vrDiagnosticsEnabled", false}, {"vrWorldScale", 1.0}};
    options.knownKeys = {"vrEnabled", "vrNativeOnly", "vrDiagnosticsEnabled", "vrWorldScale", "vrPanelOffsetX"};
    options.validate = ValidateTypes;
    return options;
}
fs::path Legacy(const VrConfigStartupOptions& options) { return options.legacyDirectory / "config.json"; }
fs::path Managed(const VrConfigStartupOptions& options) { return options.managedConfigRoot / "config.json"; }
fs::path Marker(const VrConfigStartupOptions& options) { return options.legacyDirectory / "config-layout.json"; }
void MakePlanned(const VrConfigStartupOptions& options) {
    auto marker = ReadJson(Marker(options));
    marker["phase"] = "planned";
    WriteJson(Marker(options), marker);
}
void Run(const char* name, const std::function<void()>& test) {
    ++cases;
    try { test(); std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected unique absent synthetic run directory");
        root = fs::path(argv[1]);
        if (!root.is_absolute() || fs::exists(root))
            throw std::runtime_error("test root must be absolute and must not already exist");
        fs::create_directories(root);

        Run("fresh managed template and repeat", [] {
            auto options = Options("fresh-managed");
            auto first = PrepareVrConfig(options);
            Check(first.success && first.managedLayout, "fresh install did not select managed layout");
            Check(first.configPath == Managed(options), "fresh managed path differs");
            Check(first.document.at("vrEnabled") == true && first.document.at("vrNativeOnly") == false,
                "fresh install ignored explicit enabled release template");
            Check(first.outcome == "initialized", "fresh install misreported migration");
            Check(!fs::exists(Legacy(options)), "fresh managed install wrote legacy active config");
            const auto target = ReadBytes(first.configPath), marker = ReadBytes(Marker(options));
            options.releaseDefaults["vrEnabled"] = false;
            auto repeat = PrepareVrConfig(options);
            Check(repeat.success && repeat.document.at("vrEnabled") == true, "repeat replaced existing preferences with new defaults");
            Check(ReadBytes(first.configPath) == target && ReadBytes(Marker(options)) == marker,
                "repeat rewrote completed configuration or marker");
        });
        Run("fresh old host uses explicit disabled template", [] {
            auto options = Options("fresh-legacy", false, false);
            auto result = PrepareVrConfig(options);
            Check(result.success && !result.managedLayout && result.configPath == Legacy(options), "old host path differs");
            Check(result.document.at("vrEnabled") == false && result.document.at("vrNativeOnly") == true,
                "fresh install did not respect explicit disabled template");
            Check(ReadJson(Marker(options)).at("operation") == "initialized", "fresh old host was called a migration");
        });
        Run("legacy dedicated stays in place", [] {
            auto options = Options("legacy-dedicated");
            const std::string original = "{\"vrEnabled\":false,\"userSentinel\":17}\n";
            WriteBytes(Legacy(options), original);
            auto result = PrepareVrConfig(options);
            Check(result.success && !result.managedLayout && result.configPath == Legacy(options), "existing install was relocated");
            Check(ReadBytes(Legacy(options)) == original, "existing legacy source was changed");
            Check(!fs::exists(Managed(options)), "existing legacy install created managed target");
        });
        Run("new dedicated wins and is adopted without rewriting", [] {
            auto options = Options("new-over-old");
            const std::string target = "{\"schemaVersion\":1,\"vrEnabled\":false,\"newSentinel\":3}\n";
            WriteBytes(Managed(options), target);
            WriteBytes(Legacy(options), "{ broken lower-priority source");
            auto result = PrepareVrConfig(options);
            Check(result.success && result.managedLayout && result.configPath == Managed(options), "new valid target did not win");
            Check(ReadBytes(Managed(options)) == target, "adoption rewrote existing new target");
            Check(ReadJson(Marker(options)).at("operation") == "adopted", "adoption marker operation differs");
            WriteJson(Managed(options), {{"schemaVersion", 1}, {"vrEnabled", true}});
            auto repeat = PrepareVrConfig(options);
            Check(repeat.success && repeat.document.at("vrEnabled") == true, "completed marker prevented normal user edits");
        });
        Run("new target supersedes completed legacy layout", [] {
            auto options = Options("new-after-legacy", false, true);
            Check(PrepareVrConfig(options).success, "fixture legacy initialization failed");
            const auto old = ReadBytes(Legacy(options));
            options.managedConfigRoot = root / "new-after-legacy/gakumas-local/Configs/gaku.vrify";
            const std::string target = "{\"schemaVersion\":1,\"vrEnabled\":false,\"managedSentinel\":31}\n";
            WriteBytes(Managed(options), target);
            auto result = PrepareVrConfig(options);
            Check(result.success && result.managedLayout && result.configPath == Managed(options),
                "completed legacy marker masked new managed configuration");
            Check(ReadBytes(Managed(options)) == target && ReadBytes(Legacy(options)) == old,
                "new-target adoption changed either active configuration");
            const auto marker = ReadJson(Marker(options));
            Check(marker.at("layout") == "paths-v1" && marker.at("phase") == "completed" && marker.at("operation") == "adopted",
                "legacy marker was not advanced to adopted managed layout");
        });
        Run("mixed whitelist and new aliases win", [] {
            auto options = Options("mixed-whitelist");
            const Json source = {{"vrEnabled", false}, {"vrNativeOnly", false}, {"vrProbeOnly", true},
                {"vrDiagnosticsEnabled", false}, {"vrDebugEnabled", true}, {"vrWorldScale", 1.75},
                {"localifySentinel", "synthetic"}, {"vrUnknownFuture", 99}};
            WriteJson(options.mixedConfig, source);
            const auto original = ReadBytes(options.mixedConfig);
            auto result = PrepareVrConfig(options);
            Check(result.success && !result.managedLayout && result.configPath == Legacy(options), "mixed source did not stay on compatibility layout");
            Check(result.outcome == "migrated-mixed", "mixed import did not report actual migration");
            Check(result.document.at("vrNativeOnly") == false && result.document.at("vrDiagnosticsEnabled") == false,
                "old aliases overrode new names");
            Check(!result.document.contains("localifySentinel") && !result.document.contains("vrUnknownFuture") &&
                !result.document.contains("vrProbeOnly") && !result.document.contains("vrDebugEnabled"), "mixed import copied unknown or Localify keys");
            Check(result.document.at("vrWorldScale") == 1.75, "known numeric preference changed");
            Check(ReadBytes(options.mixedConfig) == original, "mixed source was changed");
            const auto imported = ReadBytes(result.configPath), marker = ReadBytes(Marker(options));
            WriteJson(options.mixedConfig, {{"vrEnabled", true}});
            auto repeat = PrepareVrConfig(options);
            Check(repeat.success && repeat.document.at("vrEnabled") == false, "completed migration was repeated after source change");
            Check(ReadBytes(result.configPath) == imported && ReadBytes(Marker(options)) == marker, "repeat changed imported target or receipt");
        });
        Run("mixed old aliases are converted", [] {
            auto options = Options("mixed-aliases");
            WriteJson(options.mixedConfig, {{"vrEnabled", true}, {"vrProbeOnly", false}, {"vrDebugEnabled", true}});
            auto result = PrepareVrConfig(options);
            Check(result.success && result.document.at("vrNativeOnly") == false &&
                result.document.at("vrDiagnosticsEnabled") == true, "old aliases were not converted");
        });
        Run("Localify-only source does not become VR migration", [] {
            auto options = Options("localify-only");
            WriteJson(options.mixedConfig, {{"localifySentinel", 12}, {"vrUnknownFuture", 7}});
            const auto original = ReadBytes(options.mixedConfig);
            auto result = PrepareVrConfig(options);
            Check(result.success && result.managedLayout && result.outcome == "initialized", "unrecognized source was mistaken for VR data");
            Check(ReadBytes(options.mixedConfig) == original, "Localify-only source was changed");
        });
        Run("unsupported dedicated schema is preserved", [] {
            auto options = Options("unknown-schema");
            WriteJson(Legacy(options), {{"schemaVersion", 2}, {"vrEnabled", false}});
            const auto original = ReadBytes(Legacy(options));
            auto result = PrepareVrConfig(options);
            Check(!result.success && ReadBytes(Legacy(options)) == original, "unknown schema was accepted or changed");
            Check(!fs::exists(Marker(options)) && !fs::exists(Managed(options)), "unknown schema wrote completion or fresh target");
        });
        Run("malformed dedicated JSON is preserved", [] {
            auto options = Options("corrupt-json");
            const std::string original = "{ broken json";
            WriteBytes(Legacy(options), original);
            auto result = PrepareVrConfig(options);
            Check(!result.success && ReadBytes(Legacy(options)) == original, "corrupt JSON was replaced");
            Check(!fs::exists(Marker(options)), "corrupt JSON wrote completion marker");
        });
        Run("callback rejects dedicated and mixed field types", [] {
            auto dedicated = Options("invalid-type-dedicated");
            WriteJson(Legacy(dedicated), {{"schemaVersion", 1}, {"vrEnabled", "false"}});
            const auto original = ReadBytes(Legacy(dedicated));
            Check(!PrepareVrConfig(dedicated).success && ReadBytes(Legacy(dedicated)) == original, "dedicated invalid boolean escaped validation");
            auto mixed = Options("invalid-type-mixed");
            WriteJson(mixed.mixedConfig, {{"vrWorldScale", "bad"}, {"localifySentinel", 1}});
            const auto mixedOriginal = ReadBytes(mixed.mixedConfig);
            Check(!PrepareVrConfig(mixed).success && ReadBytes(mixed.mixedConfig) == mixedOriginal,
                "mixed invalid number escaped validation or source changed");
            Check(!fs::exists(Legacy(mixed)) && !fs::exists(Marker(mixed)), "invalid mixed source was committed");
        });
        Run("invalid new target never falls back to valid old target", [] {
            auto options = Options("invalid-new-wins");
            WriteJson(Managed(options), {{"schemaVersion", 3}, {"vrEnabled", false}});
            WriteJson(Legacy(options), {{"vrEnabled", true}});
            const auto target = ReadBytes(Managed(options)), old = ReadBytes(Legacy(options));
            Check(!PrepareVrConfig(options).success, "invalid new target silently fell back");
            Check(ReadBytes(Managed(options)) == target && ReadBytes(Legacy(options)) == old, "invalid new target handling changed data");
        });
        Run("unsupported mixed schema is preserved", [] {
            auto options = Options("mixed-unknown-schema");
            WriteJson(options.mixedConfig, {{"schemaVersion", 9}, {"vrEnabled", true}});
            const auto original = ReadBytes(options.mixedConfig);
            Check(!PrepareVrConfig(options).success && ReadBytes(options.mixedConfig) == original, "future mixed schema was accepted or changed");
            Check(!fs::exists(Legacy(options)) && !fs::exists(Marker(options)), "future mixed schema created output");
        });
        Run("established managed layout refuses missing service", [] {
            auto options = Options("missing-service");
            Check(PrepareVrConfig(options).success, "fixture initialization failed");
            WriteJson(Legacy(options), {{"vrEnabled", false}, {"oldSentinel", 13}});
            const auto old = ReadBytes(Legacy(options)), target = ReadBytes(Managed(options));
            options.managedConfigRoot.clear();
            auto result = PrepareVrConfig(options);
            Check(!result.success && (result.outcome.find("paths service") != std::string::npos ||
                result.outcome.find("upgrade Loader") != std::string::npos),
                "missing service did not refuse legacy fallback");
            Check(ReadBytes(Legacy(options)) == old, "missing service changed legacy remnant");
            options.managedConfigRoot = root / "missing-service/gakumas-local/Configs/gaku.vrify";
            Check(ReadBytes(Managed(options)) == target, "missing service changed managed target");
        });
        Run("unmarked default managed target refuses old-host fallback", [] {
            auto options = Options("unmarked-managed-missing-service");
            const auto knownDefault = options.mixedConfig.parent_path() / "Configs/gaku.vrify/config.json";
            WriteJson(knownDefault, {{"schemaVersion", 1}, {"vrEnabled", false}, {"managedSentinel", 43}});
            WriteJson(Legacy(options), {{"vrEnabled", true}, {"oldSentinel", 47}});
            const auto target = ReadBytes(knownDefault), old = ReadBytes(Legacy(options));
            Check(!fs::exists(Marker(options)), "unmarked fixture unexpectedly has layout marker");
            options.managedConfigRoot.clear();
            auto result = PrepareVrConfig(options);
            Check(!result.success, "old host fell back despite known default managed target");
            Check(ReadBytes(knownDefault) == target && ReadBytes(Legacy(options)) == old,
                "old-host refusal changed managed target or legacy sentinel");
            Check(!fs::exists(Marker(options)), "old-host refusal adopted or initialized a layout");
        });
        Run("completed target disappearance does not initialize again", [] {
            auto options = Options("target-missing");
            Check(PrepareVrConfig(options).success, "fixture initialization failed");
            const auto marker = ReadBytes(Marker(options));
            fs::remove(Managed(options));
            WriteJson(Legacy(options), {{"vrEnabled", false}});
            auto result = PrepareVrConfig(options);
            Check(!result.success && !fs::exists(Managed(options)), "missing established target was recreated from defaults");
            Check(ReadBytes(Marker(options)) == marker, "target disappearance changed completion record");
        });
        Run("planned commit resumes without rewriting target", [] {
            auto options = Options("planned-committed");
            Check(PrepareVrConfig(options).success, "fixture initialization failed");
            const auto target = ReadBytes(Managed(options));
            MakePlanned(options);
            auto result = PrepareVrConfig(options);
            Check(result.success && result.outcome == "recovered initialization", "committed target was not recovered");
            Check(ReadBytes(Managed(options)) == target && ReadJson(Marker(options)).at("phase") == "completed",
                "recovery rewrote target or failed to complete receipt");
        });
        Run("planned changed target conflicts then retries", [] {
            auto options = Options("planned-conflict");
            Check(PrepareVrConfig(options).success, "fixture initialization failed");
            const auto original = ReadBytes(Managed(options));
            MakePlanned(options);
            const auto plannedMarker = ReadBytes(Marker(options));
            WriteJson(Managed(options), {{"schemaVersion", 1}, {"vrEnabled", false}});
            const auto changed = ReadBytes(Managed(options));
            auto result = PrepareVrConfig(options);
            Check(!result.success && ReadBytes(Managed(options)) == changed, "planned changed target was overwritten");
            Check(ReadJson(Marker(options)).at("phase") == "planned", "conflicted plan was marked complete");
            Check(ReadBytes(Marker(options)) == plannedMarker && !ReadJson(Marker(options)).contains("supersededPlans"),
                "existing changed target caused automatic replanning");
            WriteBytes(Managed(options), original);
            Check(PrepareVrConfig(options).success, "unchanged original target could not retry");
        });
        Run("planned absent target retries same inputs", [] {
            auto options = Options("planned-before-target");
            Check(PrepareVrConfig(options).success, "fixture initialization failed");
            const auto original = ReadBytes(Managed(options));
            MakePlanned(options);
            fs::remove(Managed(options));
            auto result = PrepareVrConfig(options);
            Check(result.success && ReadBytes(Managed(options)) == original, "uncommitted plan did not retry same inputs");
        });
        Run("planned changed mixed source replans before target commit", [] {
            auto options = Options("planned-source-change");
            WriteJson(options.mixedConfig, {{"vrEnabled", false}});
            Check(PrepareVrConfig(options).success, "fixture migration failed");
            MakePlanned(options);
            const auto previousPlan = ReadJson(Marker(options));
            fs::remove(Legacy(options));
            WriteJson(options.mixedConfig, {{"vrEnabled", true}});
            const auto changed = ReadBytes(options.mixedConfig);
            auto result = PrepareVrConfig(options);
            Check(result.success && result.document.at("vrEnabled") == true &&
                result.outcome.find("replanned") != std::string::npos, "valid changed source could not safely replan");
            Check(ReadBytes(options.mixedConfig) == changed, "replanning modified the changed original source");
            const auto marker = ReadJson(Marker(options));
            Check(marker.at("phase") == "completed" && ReadJson(Legacy(options)).at("vrEnabled") == true,
                "replanned source did not commit the latest VR settings");
            Check(marker.at("supersededPlans").size() == 1 && marker.at("supersededPlans").at(0) == previousPlan &&
                !marker.at("supersededPlans").at(0).contains("supersededPlans"), "superseded plan fingerprints were not preserved");
            Check(marker.at("sourceSha256") != previousPlan.at("sourceSha256") &&
                marker.at("targetSha256") != previousPlan.at("targetSha256"), "replanned source/target fingerprints were not updated");
        });
        Run("planned Localify-only source edit replans without changing VR data", [] {
            auto options = Options("planned-localify-source-change");
            WriteJson(options.mixedConfig, {{"vrEnabled", false}, {"localifySentinel", 1}});
            Check(PrepareVrConfig(options).success, "fixture migration failed");
            const auto originalTarget = ReadBytes(Legacy(options));
            MakePlanned(options);
            const auto previousPlan = ReadJson(Marker(options));
            fs::remove(Legacy(options));
            WriteJson(options.mixedConfig, {{"vrEnabled", false}, {"localifySentinel", 2}});
            const auto changed = ReadBytes(options.mixedConfig);
            auto result = PrepareVrConfig(options);
            Check(result.success && result.outcome.find("replanned") != std::string::npos,
                "unrelated Localify source edit blocked uncommitted migration");
            Check(ReadBytes(options.mixedConfig) == changed && ReadBytes(Legacy(options)) == originalTarget,
                "Localify-only replan modified source or changed VR output");
            Check(!result.document.contains("localifySentinel"), "Localify-only replan imported unrelated data");
            const auto marker = ReadJson(Marker(options));
            Check(marker.at("phase") == "completed" && marker.at("sourceSha256") != previousPlan.at("sourceSha256") &&
                marker.at("targetSha256") == previousPlan.at("targetSha256"), "Localify-only replan fingerprints differ");
            Check(marker.at("supersededPlans").size() == 1 && marker.at("supersededPlans").at(0) == previousPlan,
                "Localify-only replan did not retain previous plan history");
        });
        Run("initialization lock failure retries", [] {
            auto options = Options("lock-retry");
            fs::create_directories(options.legacyDirectory);
            const auto lockPath = options.legacyDirectory / "config-layout.lock";
            const HANDLE lock = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
            if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("fixture could not hold initialization lock");
            const auto blocked = PrepareVrConfig(options);
            CloseHandle(lock);
            Check(!blocked.success && blocked.outcome.find("lock unavailable") != std::string::npos, "contending initializer was not refused");
            Check(!fs::exists(Marker(options)) && !fs::exists(Managed(options)), "lock failure created a target or marker");
            Check(PrepareVrConfig(options).success, "lock release did not allow retry");
        });
        Run("parent file I/O error retries", [] {
            auto options = Options("parent-file");
            WriteBytes(options.legacyDirectory, "synthetic parent-file blocker");
            Check(!PrepareVrConfig(options).success, "file in directory position was accepted");
            Check(!fs::exists(Managed(options)), "parent-directory failure created managed configuration");
            fs::remove(options.legacyDirectory);
            Check(PrepareVrConfig(options).success, "parent-directory I/O failure was not retryable");
        });
        Run("no-replace writer preserves conflict and explicit replace works", [] {
            const auto target = root / "writer/config.json";
            WriteJson(target, {{"sentinel", 1}});
            const auto original = ReadBytes(target);
            std::string error;
            Check(!WriteVrConfigDocument(target, {{"sentinel", 2}}, false, error), "no-replace writer overwrote an existing target");
            Check(!error.empty() && ReadBytes(target) == original, "writer conflict was not reported or target changed");
            Check(WriteVrConfigDocument(target, {{"sentinel", 2}}, true, error) && ReadJson(target).at("sentinel") == 2,
                "explicit replacement failed");
            for (const auto& entry : fs::directory_iterator(target.parent_path()))
                Check(entry.path().filename().wstring().find(L".tmp-") == std::wstring::npos, "failed writer left unique temporary file");
        });

        std::cout << cases << " cases, " << assertions << " assertions, " << failures << " failures\n";
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Test harness error: " << error.what() << '\n';
        return 2;
    }
}
