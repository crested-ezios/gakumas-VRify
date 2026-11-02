#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

namespace GakumasLocal::Config {
struct VrConfigStartupOptions {
    std::filesystem::path legacyDirectory;
    std::filesystem::path mixedConfig;
    // Empty only when the optional Loader paths service is unavailable.
    std::filesystem::path managedConfigRoot;
    nlohmann::json releaseDefaults;
    std::vector<std::string> knownKeys;
    void (*validate)(const nlohmann::json&) = nullptr;
};
struct VrConfigStartupResult {
    bool success = false;
    bool managedLayout = false;
    std::filesystem::path configPath;
    nlohmann::json document;
    std::string outcome;
};
// Single-direction initialization only. Existing configurations are never
// replaced; original mixed data is never modified. Journal lives at the fixed
// compatibility discovery point, so old hosts cannot select stale legacy data.
VrConfigStartupResult PrepareVrConfig(const VrConfigStartupOptions& options) noexcept;
bool WriteVrConfigDocument(const std::filesystem::path& path,
    const nlohmann::json& document, bool replaceExisting, std::string& error) noexcept;
void ValidateVrConfigSchema(const nlohmann::json& document);

struct VrConfigManagedOptions {
    VrConfigStartupOptions startup;
    std::filesystem::path pluginDataRoot;
    std::filesystem::path logRoot;
    // Internal synthetic fault checkpoints; never exposed across the plugin ABI.
    void (*checkpoint)(const char*) = nullptr;
};
// Planning reads known sources only and never creates installation directories.
nlohmann::json PlanVrConfigMigration(const VrConfigManagedOptions& options);
std::string VrConfigPlanHash(const nlohmann::json& plan);
VrConfigStartupResult ApplyVrConfigMigration(const VrConfigManagedOptions& options,
    const nlohmann::json& plan, const std::string& expectedPlanSha256) noexcept;
// outcome=="not-managed" means no v2 state exists. All other failures prohibit
// fallback, including a damaged/missing selected target or a pending migration.
VrConfigStartupResult LoadManagedVrConfig(const VrConfigManagedOptions& options) noexcept;
}
