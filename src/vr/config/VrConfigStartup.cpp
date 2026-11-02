#include "VrConfigStartup.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <atomic>
#include <fstream>
#include <stdexcept>
#include <sstream>
#pragma comment(lib, "bcrypt.lib")

namespace GakumasLocal::Config {
namespace {
using Json = nlohmann::json;
std::atomic<unsigned long> sequence{};
std::filesystem::path NativePath(const std::filesystem::path& path) {
    if (!path.is_absolute()) return path;
    auto normalized = path.lexically_normal();
    normalized.make_preferred();
    const auto value = normalized.wstring();
    if (value.starts_with(L"\\\\?\\")) return normalized;
    if (value.starts_with(L"\\\\")) return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    return std::filesystem::path(L"\\\\?\\" + value);
}
std::string Utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::string Digest(const std::string& bytes) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA256 provider unavailable");
    unsigned char hash[32]{};
    const auto status = BCryptHash(algorithm, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
        static_cast<ULONG>(bytes.size()), hash, sizeof(hash));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("SHA256 failed");
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (const auto value : hash) { result += hex[value >> 4]; result += hex[value & 15]; }
    return result;
}
void RejectReparse(const std::filesystem::path& path) {
    const auto attributes = GetFileAttributesW(NativePath(path).c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("configuration discovery refuses a reparse point");
}
bool Exists(const std::filesystem::path& path) {
    RejectReparse(path);
    return std::filesystem::exists(NativePath(path));
}
std::string Read(const std::filesystem::path& path) {
    RejectReparse(path);
    if (!std::filesystem::is_regular_file(NativePath(path))) throw std::runtime_error("configuration is not a regular file");
    const auto size = std::filesystem::file_size(NativePath(path));
    if (size > 4 * 1024 * 1024) throw std::runtime_error("configuration exceeds 4 MiB limit");
    std::ifstream input(NativePath(path), std::ios::binary);
    if (!input) throw std::runtime_error("cannot open configuration");
    std::string bytes(static_cast<size_t>(size), '\0');
    if (size && !input.read(bytes.data(), static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot read configuration");
    return bytes;
}
bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right) {
    const auto l = left.lexically_normal().wstring(), r = right.lexically_normal().wstring();
    return CompareStringOrdinal(l.c_str(), -1, r.c_str(), -1, TRUE) == CSTR_EQUAL;
}
struct Lock {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit Lock(const std::filesystem::path& path) {
        handle = CreateFileW(NativePath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("configuration initialization lock unavailable; retry after other process exits");
    }
    ~Lock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
void Write(const std::filesystem::path& path, const Json& document, bool replace) {
    std::string error;
    if (!WriteVrConfigDocument(path, document, replace, error)) throw std::runtime_error(error);
}
Json Validate(Json document, const VrConfigStartupOptions& options) {
    ValidateVrConfigSchema(document);
    if (options.validate) options.validate(document);
    return document;
}
Json Mixed(const Json& input, const VrConfigStartupOptions& options) {
    ValidateVrConfigSchema(input);
    Json output = Json::object();
    for (const auto& key : options.knownKeys)
        if (key != "schemaVersion" && input.contains(key)) output[key] = input.at(key);
    for (const auto& alias : {std::pair{"vrDebugEnabled", "vrDiagnosticsEnabled"},
            std::pair{"vrProbeOnly", "vrNativeOnly"}}) {
        if (!output.contains(alias.second) && input.contains(alias.first))
            output[alias.second] = input.at(alias.first);
    }
    if (!output.empty()) output["schemaVersion"] = 1;
    return output;
}
}

void ValidateVrConfigSchema(const Json& document) {
    if (!document.is_object()) throw std::runtime_error("configuration root must be an object");
    if (document.contains("schemaVersion")) {
        const auto& version = document.at("schemaVersion");
        if (!version.is_number_integer() || (version != 0 && version != 1))
            throw std::runtime_error("unsupported VR configuration schemaVersion");
    }
}

bool WriteVrConfigDocument(const std::filesystem::path& path, const Json& document,
    bool replaceExisting, std::string& error) noexcept {
    std::filesystem::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    bool ownsTemporary = false;
    try {
        RejectReparse(path.parent_path());
        RejectReparse(path);
        std::filesystem::create_directories(NativePath(path.parent_path()));
        temporary = path;
        temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(sequence.fetch_add(1));
        file = CreateFileW(NativePath(temporary).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create unique config temporary file");
        ownsTemporary = true;
        const auto bytes = document.dump(4) + "\n";
        DWORD written = 0;
        if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
            written != bytes.size() || !FlushFileBuffers(file))
            throw std::runtime_error("cannot write/flush config temporary file");
        CloseHandle(file); file = INVALID_HANDLE_VALUE;
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(NativePath(temporary).c_str(), NativePath(path).c_str(), flags))
            throw std::runtime_error("configuration commit failed (target conflict or I/O error)");
        temporary.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (ownsTemporary && !temporary.empty()) { std::error_code ignored; std::filesystem::remove(NativePath(temporary), ignored); }
        return false;
    }
}

VrConfigStartupResult PrepareVrConfig(const VrConfigStartupOptions& options) noexcept {
    VrConfigStartupResult result;
    try {
        if (!options.legacyDirectory.is_absolute() ||
            (!options.managedConfigRoot.empty() && !options.managedConfigRoot.is_absolute()))
            throw std::runtime_error("configuration roots must be absolute");
        // Before the first managed activation there may not be a marker yet.
        // An old host must not select stale legacy data when the default new
        // configuration already exists at the one known same-install location.
        if (options.managedConfigRoot.empty() && options.mixedConfig.is_absolute() &&
            Exists(options.mixedConfig.parent_path() / L"Configs" / L"gaku.vrify" / L"config.json"))
            throw std::runtime_error("new VR configuration exists; upgrade Loader for paths service (legacy fallback refused)");
        RejectReparse(options.legacyDirectory);
        RejectReparse(options.mixedConfig.parent_path());
        if (!options.managedConfigRoot.empty()) RejectReparse(options.managedConfigRoot);
        std::filesystem::create_directories(NativePath(options.legacyDirectory));
        Lock lock(options.legacyDirectory / L"config-layout.lock");
        const auto markerPath = options.legacyDirectory / L"config-layout.json";
        const auto legacyPath = options.legacyDirectory / L"config.json";
        const auto managedPath = options.managedConfigRoot.empty() ? std::filesystem::path{} :
            options.managedConfigRoot / L"config.json";
        Json marker;
        bool replaceMarker = false;
        std::string adoptedHash;
        if (Exists(markerPath)) {
            marker = Json::parse(Read(markerPath));
            if (!marker.is_object() || marker.at("schemaVersion") != 1 || marker.at("converterVersion") != 1 ||
                (marker.at("layout") != "legacy" && marker.at("layout") != "paths-v1") ||
                (marker.at("phase") != "planned" && marker.at("phase") != "completed"))
                throw std::runtime_error("unsupported configuration layout record");
            if (marker.at("layout") == "legacy" && !managedPath.empty() && Exists(managedPath)) {
                result.managedLayout = true;
                result.configPath = managedPath;
                const auto bytes = Read(managedPath);
                adoptedHash = Digest(bytes);
                result.document = Validate(Json::parse(bytes), options);
                marker = {{"operation", "adopted"}};
                replaceMarker = true;
            } else {
            result.managedLayout = marker.at("layout") == "paths-v1";
            if (result.managedLayout && managedPath.empty())
                throw std::runtime_error("managed VR configuration requires Loader paths service; legacy fallback refused");
            result.configPath = result.managedLayout ? managedPath : legacyPath;
            if (!SamePath(std::filesystem::u8path(marker.at("configPath").get<std::string>()), result.configPath))
                throw std::runtime_error("VR configuration root changed; explicit data-root migration required");
            if (Exists(result.configPath)) {
                const auto bytes = Read(result.configPath);
                result.document = Validate(Json::parse(bytes), options);
                if (marker.at("phase") == "planned") {
                    if (marker.at("targetSha256") != Digest(bytes))
                        throw std::runtime_error("configuration changed during interrupted initialization; target preserved");
                    marker["phase"] = "completed";
                    Write(markerPath, marker, true);
                    result.outcome = "recovered initialization";
                } else result.outcome = "loaded existing configuration";
                result.success = true;
                return result;
            }
            if (marker.at("phase") == "completed")
                throw std::runtime_error("initialized VR configuration is missing; legacy fallback refused");
            }
        } else {
            // Existing managed config wins, but no existing install is relocated.
            if (!managedPath.empty() && Exists(managedPath)) {
                result.managedLayout = true;
                result.configPath = managedPath;
                const auto bytes = Read(managedPath);
                adoptedHash = Digest(bytes);
                result.document = Validate(Json::parse(bytes), options);
                marker = {{"operation", "adopted"}};
            } else if (Exists(legacyPath)) {
                result.configPath = legacyPath;
                result.document = Validate(Json::parse(Read(legacyPath)), options);
                result.success = true;
                result.outcome = "loaded legacy dedicated configuration";
                return result;
            }
        }

        Json candidate;
        std::string operation;
        std::string sourceHash;
        bool replanned = false;
        if (!result.document.is_null()) {
            candidate = result.document;
            operation = "adopted";
        } else {
            if (Exists(options.mixedConfig)) {
                const auto source = Read(options.mixedConfig);
                candidate = Mixed(Json::parse(source), options);
                if (!candidate.empty()) { operation = "migrated-mixed"; sourceHash = Digest(source); }
            }
            if (operation.empty()) { candidate = options.releaseDefaults; operation = "initialized"; }
            candidate = Validate(std::move(candidate), options);
            if (result.configPath.empty()) {
                result.managedLayout = operation == "initialized" && !managedPath.empty();
                result.configPath = result.managedLayout ? managedPath : legacyPath;
            }
        }
        const auto targetHash = Digest(candidate.dump(4) + "\n");
        if (!marker.is_null() && marker.contains("phase")) {
            if (marker.at("operation") != operation || marker.at("sourceSha256") != sourceHash ||
                marker.at("targetSha256") != targetHash) {
                // No config has been committed. A valid changed mixed source
                // (including unrelated Localify edits) may safely supersede the
                // old plan; retain its fingerprints rather than requiring users
                // to delete an internal journal. Never replan an adopted file
                // or change source kind, and commit still uses no-replace.
                if (marker.at("operation") != operation ||
                    (operation != "migrated-mixed" && operation != "initialized") ||
                    Exists(result.configPath))
                    throw std::runtime_error("initialization source kind or target changed; original plan retained");
                auto previous = marker;
                previous.erase("supersededPlans");
                if (!marker.contains("supersededPlans")) marker["supersededPlans"] = Json::array();
                if (!marker.at("supersededPlans").is_array())
                    throw std::runtime_error("invalid initialization plan history");
                marker["supersededPlans"].push_back(std::move(previous));
                marker["sourceSha256"] = sourceHash;
                marker["targetSha256"] = targetHash;
                Write(markerPath, marker, true);
                replanned = true;
            }
        } else {
            marker = {{"schemaVersion", 1}, {"converterVersion", 1},
                {"layout", result.managedLayout ? "paths-v1" : "legacy"},
                {"phase", "planned"}, {"operation", operation},
                {"configPath", Utf8(result.configPath)}, {"sourceSha256", sourceHash},
                {"targetSha256", operation == "adopted" ? adoptedHash : targetHash}};
            Write(markerPath, marker, replaceMarker);
        }
        if (operation == "migrated-mixed" && Digest(Read(options.mixedConfig)) != sourceHash)
            throw std::runtime_error("legacy source changed before commit; original plan retained");
        if (operation != "adopted") Write(result.configPath, candidate, false);
        if (Digest(Read(result.configPath)) != marker.at("targetSha256").get<std::string>())
            throw std::runtime_error("configuration changed before initialization completed; target preserved");
        marker["phase"] = "completed";
        Write(markerPath, marker, true);
        result.document = std::move(candidate);
        result.outcome = operation + (replanned ? " (replanned before commit)" : "");
        result.success = true;
    } catch (const std::exception& exception) {
        result.document = Json();
        result.outcome = exception.what();
    }
    return result;
}
}

namespace GakumasLocal::Config {
namespace {
Json ParseSafe(const std::string& bytes) {
    auto document = Json::parse(bytes, nullptr, false);
    if (document.is_discarded()) throw std::runtime_error("malformed configuration JSON; original retained");
    return document;
}
void CheckManagedOptions(const VrConfigManagedOptions& options) {
    if (!options.startup.legacyDirectory.is_absolute() || !options.startup.mixedConfig.is_absolute())
        throw std::runtime_error("legacy discovery roots must be absolute");
    for (const auto& path : {options.startup.managedConfigRoot, options.pluginDataRoot, options.logRoot}) {
        if (!path.is_absolute()) throw std::runtime_error("managed configuration roots must be absolute");
        RejectReparse(path);
    }
}
void CheckManagedDocument(const Json& document, const VrConfigManagedOptions& options) {
    ValidateVrConfigSchema(document);
    for (auto it = options.startup.releaseDefaults.begin(); it != options.startup.releaseDefaults.end(); ++it) {
        if (it.key() == "schemaVersion" || !document.contains(it.key())) continue;
        const auto& actual = document.at(it.key());
        const auto& expected = it.value();
        if ((expected.is_boolean() && !actual.is_boolean()) ||
            (expected.is_number_integer() && !actual.is_number_integer()) ||
            (expected.is_number_float() && !actual.is_number()) ||
            (expected.is_string() && !actual.is_string()))
            throw std::runtime_error("invalid known VR configuration field type");
    }
    for (const auto& alias : {std::pair{"vrDebugEnabled", "vrDiagnosticsEnabled"},
            std::pair{"vrProbeOnly", "vrNativeOnly"}})
        if (!document.contains(alias.second) && document.contains(alias.first) &&
            !document.at(alias.first).is_boolean()) throw std::runtime_error("invalid legacy VR boolean");
    if (options.startup.validate) options.startup.validate(document);
}
std::filesystem::path ActivePath(const VrConfigManagedOptions& options) {
    return options.pluginDataRoot / L"active-layout.json";
}
std::filesystem::path JournalPath(const VrConfigManagedOptions& options) {
    return options.pluginDataRoot / L"migrations" / L"layout-v2.json";
}
std::filesystem::path TargetPath(const VrConfigManagedOptions& options) {
    return options.startup.managedConfigRoot / L"config.json";
}
void CheckPlanRoots(const Json& plan, const VrConfigManagedOptions& options) {
    if (!plan.is_object() || plan.at("schemaVersion") != 1 || plan.at("layoutVersion") != 2 ||
        plan.at("pluginId") != "gaku.vrify") throw std::runtime_error("unsupported VR migration plan");
    for (const auto& pair : {std::pair{"legacyDirectory", options.startup.legacyDirectory},
            std::pair{"mixedConfig", options.startup.mixedConfig},
            std::pair{"configRoot", options.startup.managedConfigRoot},
            std::pair{"pluginDataRoot", options.pluginDataRoot}, std::pair{"logRoot", options.logRoot}})
        if (!SamePath(std::filesystem::u8path(plan.at(pair.first).get<std::string>()), pair.second))
            throw std::runtime_error("migration plan root identity mismatch");
}
void CheckActive(const Json& active, const VrConfigManagedOptions& options) {
    if (!active.is_object() || active.at("schemaVersion") != 1 || active.at("layoutVersion") != 2 ||
        active.at("pluginId") != "gaku.vrify" ||
        !SamePath(std::filesystem::u8path(active.at("configPath").get<std::string>()), TargetPath(options)) ||
        !SamePath(std::filesystem::u8path(active.at("logRoot").get<std::string>()), options.logRoot))
        throw std::runtime_error("unsupported or mismatched active VR layout");
}
std::string FileIdentity(const std::filesystem::path& path) {
    RejectReparse(path);
    const HANDLE file = CreateFileW(NativePath(path).c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot read migration file identity");
    FILE_ID_INFO info{};
    const bool ok = GetFileInformationByHandleEx(file, FileIdInfo, &info, sizeof(info)) != 0;
    CloseHandle(file);
    if (!ok) throw std::runtime_error("filesystem does not provide durable migration file identities");
    constexpr char hex[] = "0123456789abcdef";
    std::string value = std::to_string(info.VolumeSerialNumber) + ":";
    for (const auto byte : info.FileId.Identifier) { value += hex[byte >> 4]; value += hex[byte & 15]; }
    return value;
}
std::filesystem::path StageBytes(const std::filesystem::path& target, const std::string& bytes) {
    RejectReparse(target.parent_path());
    std::filesystem::create_directories(NativePath(target.parent_path()));
    std::filesystem::path stage;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        stage = target;
        stage += L".migration-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(sequence.fetch_add(1));
        file = CreateFileW(NativePath(stage).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS) throw std::runtime_error("cannot create migration staging file");
    }
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("migration staging name unavailable");
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) throw std::runtime_error("migration staging write/flush failed; source retained");
    return stage;
}
void Checkpoint(const VrConfigManagedOptions& options, const char* name) {
    if (options.checkpoint) options.checkpoint(name);
}
struct MigrationInput {
    std::string action, sourceHash, targetBytes;
};
MigrationInput InputFor(const VrConfigManagedOptions& options, const std::string& action) {
    MigrationInput input;
    input.action = action;
    if (action == "adopt-new" || action == "already-managed") {
        input.targetBytes = Read(TargetPath(options));
        input.sourceHash = Digest(input.targetBytes);
    } else if (action == "copy-dedicated") {
        RejectReparse(options.startup.legacyDirectory);
        input.targetBytes = Read(options.startup.legacyDirectory / L"config.json");
        input.sourceHash = Digest(input.targetBytes);
    } else if (action == "convert-mixed") {
        RejectReparse(options.startup.mixedConfig.parent_path());
        const auto bytes = Read(options.startup.mixedConfig);
        input.sourceHash = Digest(bytes);
        auto candidate = Mixed(ParseSafe(bytes), options.startup);
        if (candidate.empty()) throw std::runtime_error("mixed source no longer contains known VR fields");
        input.targetBytes = candidate.dump(4) + "\n";
    } else if (action == "initialize") {
        input.targetBytes = options.startup.releaseDefaults.dump(4) + "\n";
        input.sourceHash = Digest(input.targetBytes);
    } else throw std::runtime_error("unsupported migration operation");
    CheckManagedDocument(ParseSafe(input.targetBytes), options);
    return input;
}
std::string OldReceiptHash(const VrConfigManagedOptions& options) {
    RejectReparse(options.startup.legacyDirectory);
    const auto path = options.startup.legacyDirectory / L"config-layout.json";
    return Exists(path) ? Digest(Read(path)) : std::string{};
}
std::string ReceiptHashFor(const VrConfigManagedOptions& options, const std::string& action) {
    if (action == "already-managed") return "not-inspected";
    if (action == "adopt-new") {
        try { return OldReceiptHash(options); }
        catch (...) { return "not-inspected"; } // Higher-priority new settings never depend on obsolete reads.
    }
    return OldReceiptHash(options);
}
void CheckLegacySelection(const VrConfigManagedOptions& options) {
    RejectReparse(options.startup.legacyDirectory);
    const auto path = options.startup.legacyDirectory / L"config-layout.json";
    if (!Exists(path)) return;
    const auto old = ParseSafe(Read(path));
    if (!old.is_object() || old.at("schemaVersion") != 1 || old.at("converterVersion") != 1 ||
        (old.at("layout") != "legacy" && old.at("layout") != "paths-v1") ||
        (old.at("phase") != "planned" && old.at("phase") != "completed"))
        throw std::runtime_error("unsupported legacy VR layout receipt; original retained");
    const auto selected = old.at("layout") == "paths-v1" ? TargetPath(options) :
        options.startup.legacyDirectory / L"config.json";
    if (!SamePath(std::filesystem::u8path(old.at("configPath").get<std::string>()), selected))
        throw std::runtime_error("legacy layout root changed; explicit root reconciliation required");
    if (old.at("layout") == "paths-v1" || old.at("phase") == "completed")
        if (!Exists(selected)) throw std::runtime_error("previously selected VR config is missing; fallback refused");
}
}

std::string VrConfigPlanHash(const Json& plan) { return Digest(plan.dump(4) + "\n"); }

Json PlanVrConfigMigration(const VrConfigManagedOptions& options) {
    CheckManagedOptions(options);
    std::string action;
    if (Exists(ActivePath(options))) {
        CheckActive(ParseSafe(Read(ActivePath(options))), options);
        action = "already-managed";
    } else if (Exists(JournalPath(options)) && ParseSafe(Read(JournalPath(options))).at("phase") == "completed") {
        const auto journal = ParseSafe(Read(JournalPath(options)));
        if (journal.at("schemaVersion") != 1 || journal.at("layoutVersion") != 2)
            throw std::runtime_error("unsupported completed migration state");
        CheckPlanRoots(journal.at("plan"), options);
        action = "already-managed"; // Completed never reads old sources, even if active pointer was removed.
    } else if (Exists(TargetPath(options))) action = "adopt-new";
    else {
        CheckLegacySelection(options);
        if (Exists(options.startup.legacyDirectory / L"config.json")) action = "copy-dedicated";
        else if (Exists(options.startup.mixedConfig) && !Mixed(ParseSafe(Read(options.startup.mixedConfig)), options.startup).empty())
            action = "convert-mixed";
        else action = "initialize";
    }
    // An interrupted owned copy is resumable under its approved original plan.
    if (action == "adopt-new" && Exists(JournalPath(options))) {
        const auto journal = ParseSafe(Read(JournalPath(options)));
        if (journal.at("schemaVersion") != 1 || journal.at("layoutVersion") != 2)
            throw std::runtime_error("unsupported migration journal");
        const auto& previous = journal.at("plan");
        CheckPlanRoots(previous, options);
        if (journal.contains("stagedFileId") && journal.at("stagedFileId") == FileIdentity(TargetPath(options)) &&
            previous.at("targetSha256") == Digest(Read(TargetPath(options)))) {
            try {
                const auto oldInput = InputFor(options, previous.at("action").get<std::string>());
                if (previous.at("sourceSha256") == oldInput.sourceHash &&
                    previous.at("legacyReceiptSha256") == ReceiptHashFor(options, oldInput.action)) return previous;
            } catch (...) { /* A fresh explicit adoption plan can preserve the valid new target. */ }
        }
    }
    const auto input = InputFor(options, action);
    return {{"schemaVersion", 1}, {"layoutVersion", 2}, {"pluginId", "gaku.vrify"}, {"action", action},
        {"legacyDirectory", Utf8(options.startup.legacyDirectory)}, {"mixedConfig", Utf8(options.startup.mixedConfig)},
        {"configRoot", Utf8(options.startup.managedConfigRoot)}, {"pluginDataRoot", Utf8(options.pluginDataRoot)},
        {"logRoot", Utf8(options.logRoot)}, {"sourceSha256", input.sourceHash},
        {"targetSha256", Digest(input.targetBytes)}, {"legacyReceiptSha256", ReceiptHashFor(options, action)}};
}

VrConfigStartupResult LoadManagedVrConfig(const VrConfigManagedOptions& options) noexcept {
    VrConfigStartupResult result;
    result.managedLayout = true;
    try {
        CheckManagedOptions(options);
        if (!Exists(ActivePath(options))) {
            result.outcome = Exists(JournalPath(options)) ? "managed migration pending; rerun approved migration tool" : "not-managed";
            return result;
        }
        const auto active = ParseSafe(Read(ActivePath(options)));
        CheckActive(active, options);
        result.configPath = TargetPath(options);
        result.document = ParseSafe(Read(result.configPath));
        CheckManagedDocument(result.document, options);
        result.success = true;
        result.outcome = "loaded standard managed configuration";
    } catch (const Json::exception&) { result.outcome = "invalid managed layout or configuration; fallback refused"; }
      catch (const std::exception& exception) { result.outcome = exception.what(); }
    return result;
}

VrConfigStartupResult ApplyVrConfigMigration(const VrConfigManagedOptions& options,
    const Json& approvedPlan, const std::string& expectedPlanSha256) noexcept {
    VrConfigStartupResult result;
    result.managedLayout = true;
    try {
        CheckManagedOptions(options);
        CheckPlanRoots(approvedPlan, options);
        if (VrConfigPlanHash(approvedPlan) != expectedPlanSha256) throw std::runtime_error("approved migration plan hash mismatch");
        const auto stateDirectory = JournalPath(options).parent_path();
        std::filesystem::create_directories(NativePath(stateDirectory));
        Lock lock(stateDirectory / L"layout-v2.lock");
        const auto active = LoadManagedVrConfig(options);
        if (active.success) {
            if (Exists(JournalPath(options))) try {
                auto journal = ParseSafe(Read(JournalPath(options)));
                const auto manifest = ParseSafe(Read(ActivePath(options)));
                if (journal.at("planSha256") == manifest.at("planSha256") && journal.at("phase") != "completed") {
                    journal["phase"] = "completed";
                    Write(JournalPath(options), journal, true);
                }
            } catch (...) { /* A valid committed active layout outranks obsolete journal history. */ }
            return active;
        }
        if (Exists(ActivePath(options))) throw std::runtime_error(active.outcome);
        if (Exists(JournalPath(options))) {
            const auto saved = ParseSafe(Read(JournalPath(options)));
            if (saved.at("schemaVersion") == 1 && saved.at("layoutVersion") == 2 && saved.at("phase") == "completed") {
                CheckPlanRoots(saved.at("plan"), options);
                const auto document = ParseSafe(Read(TargetPath(options)));
                CheckManagedDocument(document, options);
                const Json manifest{{"schemaVersion", 1}, {"layoutVersion", 2}, {"pluginId", "gaku.vrify"},
                    {"configPath", Utf8(TargetPath(options))}, {"logRoot", Utf8(options.logRoot)},
                    {"planSha256", saved.at("planSha256")},
                    {"configSha256AtCommit", saved.at("plan").at("targetSha256")}};
                Write(ActivePath(options), manifest, false);
                result = LoadManagedVrConfig(options);
                result.outcome = "recovered committed standard layout; legacy data not consulted";
                return result;
            }
        }
        const auto action = approvedPlan.at("action").get<std::string>();
        auto input = InputFor(options, action);
        if (approvedPlan.at("sourceSha256") != input.sourceHash ||
            approvedPlan.at("targetSha256") != Digest(input.targetBytes) ||
            approvedPlan.at("legacyReceiptSha256") != ReceiptHashFor(options, action))
            throw std::runtime_error("migration inputs changed; generate and inspect a new plan");
        Json journal;
        if (Exists(JournalPath(options))) {
            journal = ParseSafe(Read(JournalPath(options)));
            if (journal.at("schemaVersion") != 1 || journal.at("layoutVersion") != 2 ||
                (journal.at("phase") != "planned" && journal.at("phase") != "staged" &&
                 journal.at("phase") != "copied" && journal.at("phase") != "completed"))
                throw std::runtime_error("unsupported migration journal");
            CheckPlanRoots(journal.at("plan"), options);
            if (journal.at("planSha256") != expectedPlanSha256) {
                if (Exists(TargetPath(options)) && action != "adopt-new")
                    throw std::runtime_error("migration target appeared; explicit new-target adoption plan required");
                auto previous = journal;
                previous.erase("supersededPlans");
                auto history = journal.value("supersededPlans", Json::array());
                history.push_back(std::move(previous));
                journal = {{"schemaVersion", 1}, {"layoutVersion", 2}, {"phase", "planned"},
                    {"plan", approvedPlan}, {"planSha256", expectedPlanSha256}, {"supersededPlans", history}};
                Write(JournalPath(options), journal, true);
            }
        } else {
            // Refuse a destination that appeared after a plan selected an old source.
            if (action != "adopt-new" && action != "already-managed" && Exists(TargetPath(options)))
                throw std::runtime_error("migration target appeared after plan; target retained");
            journal = {{"schemaVersion", 1}, {"layoutVersion", 2}, {"phase", "planned"},
                {"plan", approvedPlan}, {"planSha256", expectedPlanSha256}};
            Write(JournalPath(options), journal, false);
        }
        Checkpoint(options, "after-planned");
        if (action != "adopt-new" && action != "already-managed") {
            if (Exists(TargetPath(options))) {
                if (!journal.contains("stagedFileId") ||
                    journal.at("stagedFileId") != FileIdentity(TargetPath(options)) ||
                    approvedPlan.at("targetSha256") != Digest(Read(TargetPath(options))))
                    throw std::runtime_error("migration destination belongs to another writer; target retained");
            } else {
                std::filesystem::path stage;
                if (journal.contains("stagedPath") && Exists(std::filesystem::u8path(journal.at("stagedPath").get<std::string>()))) {
                    stage = std::filesystem::u8path(journal.at("stagedPath").get<std::string>());
                    if (!SamePath(stage.parent_path(), options.startup.managedConfigRoot) ||
                        journal.at("stagedFileId") != FileIdentity(stage) ||
                        approvedPlan.at("targetSha256") != Digest(Read(stage)))
                        throw std::runtime_error("staged migration file identity/hash mismatch");
                } else {
                    stage = StageBytes(TargetPath(options), input.targetBytes);
                    journal["stagedPath"] = Utf8(stage);
                    journal["stagedFileId"] = FileIdentity(stage);
                    journal["phase"] = "staged";
                    Write(JournalPath(options), journal, true);
                }
                Checkpoint(options, "after-staged");
                if (InputFor(options, action).sourceHash != input.sourceHash)
                    throw std::runtime_error("configuration source changed before copy commit");
                Checkpoint(options, "before-copy-commit");
                if (!MoveFileExW(NativePath(stage).c_str(), NativePath(TargetPath(options)).c_str(), MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("no-replace configuration copy commit failed; target retained");
                Checkpoint(options, "after-copy-commit");
            }
            if (approvedPlan.at("targetSha256") != Digest(Read(TargetPath(options))))
                throw std::runtime_error("copied configuration checksum mismatch");
        } else if (approvedPlan.at("targetSha256") != Digest(Read(TargetPath(options))))
            throw std::runtime_error("new configuration changed after adoption plan; target retained");
        journal["phase"] = "copied";
        Write(JournalPath(options), journal, true);
        Checkpoint(options, "after-copied");
        const auto oldReceiptHash = ReceiptHashFor(options, action);
        if (approvedPlan.at("legacyReceiptSha256") != oldReceiptHash)
            throw std::runtime_error("legacy receipt changed before activation");
        if (!oldReceiptHash.empty() && oldReceiptHash != "not-inspected") {
            const auto backup = stateDirectory / (L"legacy-config-layout." + std::wstring(oldReceiptHash.begin(), oldReceiptHash.end()) + L".json");
            if (Exists(backup)) {
                if (Digest(Read(backup)) != oldReceiptHash) throw std::runtime_error("legacy receipt backup conflict");
            } else {
                const auto stage = StageBytes(backup, Read(options.startup.legacyDirectory / L"config-layout.json"));
                if (!MoveFileExW(NativePath(stage).c_str(), NativePath(backup).c_str(), MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("legacy receipt backup commit failed");
                if (Digest(Read(backup)) != oldReceiptHash) throw std::runtime_error("legacy receipt backup verification failed");
            }
            journal["legacyReceiptBackup"] = Utf8(backup);
            Write(JournalPath(options), journal, true);
        }
        Checkpoint(options, "after-receipt-backup");
        if (InputFor(options, action).sourceHash != input.sourceHash ||
            approvedPlan.at("targetSha256") != Digest(Read(TargetPath(options))))
            throw std::runtime_error("source or target changed before active-layout switch");
        Checkpoint(options, "before-activate");
        if (InputFor(options, action).sourceHash != input.sourceHash ||
            approvedPlan.at("targetSha256") != Digest(Read(TargetPath(options))))
            throw std::runtime_error("source or target changed at active-layout switch; target retained");
        const Json manifest{{"schemaVersion", 1}, {"layoutVersion", 2}, {"pluginId", "gaku.vrify"},
            {"configPath", Utf8(TargetPath(options))}, {"logRoot", Utf8(options.logRoot)},
            {"planSha256", expectedPlanSha256}, {"configSha256AtCommit", approvedPlan.at("targetSha256")}};
        Write(ActivePath(options), manifest, false);
        Checkpoint(options, "after-activate");
        journal["phase"] = "completed";
        Write(JournalPath(options), journal, true);
        Checkpoint(options, "after-completed");
        result = LoadManagedVrConfig(options);
        result.outcome = "standard layout activated; original config and receipts retained";
    } catch (const Json::exception&) { result.outcome = "invalid migration JSON or field type; original retained"; }
      catch (const std::exception& exception) { result.outcome = exception.what(); }
    return result;
}
}
