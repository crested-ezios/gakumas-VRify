#include <string>
#include "VrifyConfig.hpp"
#include "VrConfigStartup.hpp"
#include "VrConfigDefaults.hpp"
#include "host/localify/ConfigIntegration.hpp"
#include "../camera/FollowSmoothing.hpp"
#include "../LiveGaze.hpp"
#include "../PointerVisual.hpp"
#include "nlohmann/json.hpp"
#include "GakumasLocalify/Log.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace GakumasLocal::Config {

    bool vrDiagnosticsEnabled = false;
    bool vrRuntimeStartupEnabled = false;
    bool vrDiagnosticsStartupEnabled = false;
    bool vrLocalizeText = false;
    int vrMenuLanguage = kDefaultVrMenuLanguage;
    bool vrEnabled = false;
    bool vrNativeOnly = true;
    bool vrHeadPoseEnabled = false;
    float vrWorldScale = 1.0F;
    bool vrStereoEnabled = false;
    float vrStereoRenderScale = 1.0F;
    bool vrStereoLandscapeOnly = false;
    float vrEyeOutlineWidth = kDefaultVrEyeOutlineWidth;
    bool vrBloomFollowSourceCamera = false;
    int vrEyeAaMode = kDefaultVrEyeAaMode;
    int vrEyeTaaQuality = kDefaultVrEyeTaaQuality;
    float vrEyeTaaFrameInfluence = kDefaultVrEyeTaaFrameInfluence;
    float vrEyeTaaJitterScale = kDefaultVrEyeTaaJitterScale;
    float vrEyeTaaMipBias = kDefaultVrEyeTaaMipBias;
    float vrEyeTaaVarianceClamp = kDefaultVrEyeTaaVarianceClamp;
    float vrEyeTaaSharpen = kDefaultVrEyeTaaSharpen;
    int vrEyeSmaaQuality = 2;
    bool vrActorShadowSourceAnchor = false;
    bool vrActorToonSourceAnchor = false;
    int vrToonFollowRef = kDefaultVrToonFollowRef;
    bool vrVolumeSourceAnchor = true;
    bool vrDisableSourceCamera = false;
    bool vrSourceOffResetPerFrame = false;
    bool vrEyeJumpFloodTemporalOff = false;
    bool vrVlMotionBlurOff = true;
    bool vrEyeAsMainCamera = false;
    bool vrEyeObjectMotionVectorsOff = false;
    bool vrEyeDeferredStencilOff = false;
    float vrEyeShadeBandBias = 0.0F;
    bool vrSourceCameraTiny = false;
    bool vrGripPanelTransparent = false;
    bool vrHideUiTextureOverlay = true;
    bool vrHandGlowSticks = false;
    bool vrPointerSmoothEnabled = true;
    float vrPointerSmoothMinCutoff = kDefaultVrPointerSmoothMinCutoff;
    float vrPointerSmoothBeta = kDefaultVrPointerSmoothBeta;
    float vrPointerSizeScale = 1.0F;
    int vrCameraYButtonBone = 0;
    int vrFpDirectionFollow = 3;
    int vrFollowSmoothingPreset = 0;
    bool vrLiveGaze = false;
    int vrLiveGazePreset = kDefaultVrLiveGazePreset;
    int vrLiveGazeScope = kDefaultVrLiveGazeScope;
    float vrFollowHorizontalMs = 0.0F;
    float vrFollowVerticalMs = 100.0F;
    int vrCameraTurnMode = 0;
    float vrCameraTurnSpeed = 120.0F;
    bool vrPanelCustomized = false;
    bool vrPanelPinned = false;
    float vrPanelOffsetX = 0.0F;
    float vrPanelOffsetY = -0.2F;
    float vrPanelOffsetZ = -2.0F;
    float vrPanelWidth = 1.2F;

    void ResetVrEyeAaToBaseline() {
        vrEyeAaMode = kDefaultVrEyeAaMode;
        vrEyeTaaQuality = kDefaultVrEyeTaaQuality;
        vrEyeTaaFrameInfluence = kDefaultVrEyeTaaFrameInfluence;
        vrEyeTaaJitterScale = kDefaultVrEyeTaaJitterScale;
        vrEyeTaaMipBias = kDefaultVrEyeTaaMipBias;
        vrEyeTaaVarianceClamp = kDefaultVrEyeTaaVarianceClamp;
        vrEyeTaaSharpen = kDefaultVrEyeTaaSharpen;
        vrEyeSmaaQuality = 2;
    }

    void ClampVrEyeAaSettings() {
        const auto clampInt = [](int value, int lo, int hi) {
            return value < lo ? lo : (value > hi ? hi : value);
        };
        const auto clampFloat = [](float value, float lo, float hi, float fallback) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, lo, hi);
        };
        // Stored IDs are stable: 0 inherit, 1 TAA, 2 SMAA, 3 none,
        // 4 SMAA T2x, 5 TSCMAA.
        vrEyeAaMode = clampInt(vrEyeAaMode, 0, 5);
        vrEyeTaaQuality = clampInt(vrEyeTaaQuality, 0, 4);
        vrEyeSmaaQuality = clampInt(vrEyeSmaaQuality, 0, 2);
        vrStereoRenderScale =
            clampFloat(vrStereoRenderScale, 0.25F, 1.5F, 1.0F);
        vrEyeOutlineWidth = clampFloat(
            vrEyeOutlineWidth, 0.0F, 1.0F, kDefaultVrEyeOutlineWidth);
        vrEyeTaaFrameInfluence = clampFloat(
            vrEyeTaaFrameInfluence,
            0.0F,
            1.0F,
            kDefaultVrEyeTaaFrameInfluence);
        vrEyeTaaJitterScale = clampFloat(
            vrEyeTaaJitterScale, 0.0F, 2.0F, kDefaultVrEyeTaaJitterScale);
        vrEyeTaaMipBias = clampFloat(
            vrEyeTaaMipBias, -2.0F, 2.0F, kDefaultVrEyeTaaMipBias);
        vrEyeTaaVarianceClamp = clampFloat(
            vrEyeTaaVarianceClamp,
            0.0F,
            4.0F,
            kDefaultVrEyeTaaVarianceClamp);
        vrEyeTaaSharpen = clampFloat(
            vrEyeTaaSharpen, 0.0F, 1.0F, kDefaultVrEyeTaaSharpen);
        vrEyeShadeBandBias =
            clampFloat(vrEyeShadeBandBias, -0.5F, 0.5F, 0.0F);
    }

    void ResetVrPointerSettings() {
        vrPointerSmoothEnabled = true;
        vrPointerSmoothMinCutoff = kDefaultVrPointerSmoothMinCutoff;
        vrPointerSmoothBeta = kDefaultVrPointerSmoothBeta;
    }

    void ClampVrPointerSettings() {
        const auto clampFloat = [](float value, float lo, float hi, float fallback) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, lo, hi);
        };
        vrPointerSmoothMinCutoff = clampFloat(
            vrPointerSmoothMinCutoff,
            0.15F,
            3.0F,
            kDefaultVrPointerSmoothMinCutoff);
        vrPointerSmoothBeta = clampFloat(
            vrPointerSmoothBeta, 0.0F, 12.0F, kDefaultVrPointerSmoothBeta);
        const auto clampInt = [](int value, int lo, int hi) {
            return value < lo ? lo : (value > hi ? hi : value);
        };
        vrCameraYButtonBone = clampInt(vrCameraYButtonBone, 0, 1);
        vrFpDirectionFollow = clampInt(vrFpDirectionFollow, 0, 3);
        vrFollowSmoothingPreset = vrFollowSmoothingPreset == 5 ? 5 : 0;
        gakumas::vr::SetLiveGazeRequested(vrLiveGaze);
        if (vrLiveGazePreset < 0 || vrLiveGazePreset > 2) vrLiveGazePreset = kDefaultVrLiveGazePreset;
        gakumas::vr::SetLiveGazePreset(vrLiveGazePreset);
        if (vrLiveGazeScope != 0 && vrLiveGazeScope != 1) vrLiveGazeScope = kDefaultVrLiveGazeScope;
        gakumas::vr::SetLiveGazeScope(vrLiveGazeScope);
        vrFollowHorizontalMs = clampFloat(vrFollowHorizontalMs, 0.0F, 500.0F, 0.0F);
        vrFollowVerticalMs = clampFloat(vrFollowVerticalMs, 0.0F, 500.0F, 100.0F);
        vrCameraTurnMode = clampInt(vrCameraTurnMode, 0, 1);
        vrCameraTurnSpeed = clampFloat(vrCameraTurnSpeed, 30.0F, 360.0F, 120.0F);
        gakumas::vr::camera::PublishFollowSmoothing(
            vrFollowSmoothingPreset, vrFollowHorizontalMs, vrFollowVerticalMs);
        vrMenuLanguage = clampInt(
            vrMenuLanguage, kVrMenuLanguageSystem, kVrMenuLanguageEn);
    }

    namespace {
        nlohmann::json g_vrConfigDocument = nlohmann::json::object();

        void ResetVrConfigFailClosed() {
            vrFollowSmoothingPreset = 0;
            vrLiveGaze = false;
            vrLiveGazePreset = kDefaultVrLiveGazePreset;
            vrLiveGazeScope = kDefaultVrLiveGazeScope;
            gakumas::vr::SetLiveGazePreset(vrLiveGazePreset);
            gakumas::vr::SetLiveGazeScope(vrLiveGazeScope);
            gakumas::vr::SetLiveGazeRequested(false);
            vrFollowHorizontalMs = 0.0F;
            vrFollowVerticalMs = 100.0F;
            gakumas::vr::camera::PublishFollowSmoothing(0, 0.0F, 100.0F);
            vrDiagnosticsEnabled = false;
            vrRuntimeStartupEnabled = false;
            vrDiagnosticsStartupEnabled = false;
            vrEnabled = false;
            vrLocalizeText = false;
            vrMenuLanguage = kDefaultVrMenuLanguage;
            vrNativeOnly = true;
            vrHeadPoseEnabled = false;
            vrWorldScale = 1.0F;
            vrStereoEnabled = false;
            vrStereoRenderScale = 1.0F;
            vrStereoLandscapeOnly = false;
            vrEyeOutlineWidth = kDefaultVrEyeOutlineWidth;
            vrBloomFollowSourceCamera = false;
            ResetVrEyeAaToBaseline();
            vrActorShadowSourceAnchor = false;
            vrActorToonSourceAnchor = false;
            vrToonFollowRef = kDefaultVrToonFollowRef;
            vrVolumeSourceAnchor = true;
            vrDisableSourceCamera = false;
            vrSourceOffResetPerFrame = false;
            vrEyeJumpFloodTemporalOff = false;
            vrVlMotionBlurOff = true;
            vrEyeAsMainCamera = false;
            vrEyeObjectMotionVectorsOff = false;
            vrEyeDeferredStencilOff = false;
            vrEyeShadeBandBias = 0.0F;
            vrSourceCameraTiny = false;
            vrGripPanelTransparent = false;
            vrHideUiTextureOverlay = true;
            vrHandGlowSticks = false;
            vrPointerSizeScale = 1.0F;
            ResetVrPointerSettings();
            vrPanelCustomized = false;
            vrPanelPinned = false;
            vrPanelOffsetX = 0.0F;
            vrPanelOffsetY = -0.2F;
            vrPanelOffsetZ = -2.0F;
            vrPanelWidth = 1.2F;
        }

        void ApplyVrConfig(const nlohmann::json& config) {
#define GetVrConfigItem(name) if (config.contains(#name)) name = config.at(#name).get<decltype(name)>()
            if (config.contains("vrDiagnosticsEnabled")) {
                vrDiagnosticsEnabled = config.at("vrDiagnosticsEnabled").get<bool>();
            } else if (config.contains("vrDebugEnabled")) {
                vrDiagnosticsEnabled = config.at("vrDebugEnabled").get<bool>();
            }
            GetVrConfigItem(vrEnabled);
            GetVrConfigItem(vrLocalizeText);
            GetVrConfigItem(vrMenuLanguage);
            if (config.contains("vrNativeOnly")) {
                vrNativeOnly = config.at("vrNativeOnly").get<bool>();
            } else if (config.contains("vrProbeOnly")) {
                vrNativeOnly = config.at("vrProbeOnly").get<bool>();
            }
            GetVrConfigItem(vrHeadPoseEnabled);
            GetVrConfigItem(vrWorldScale);
            GetVrConfigItem(vrStereoEnabled);
            GetVrConfigItem(vrStereoRenderScale);
            GetVrConfigItem(vrStereoLandscapeOnly);
            GetVrConfigItem(vrEyeOutlineWidth);
            GetVrConfigItem(vrBloomFollowSourceCamera);
            GetVrConfigItem(vrEyeAaMode);
            GetVrConfigItem(vrEyeTaaQuality);
            GetVrConfigItem(vrEyeTaaFrameInfluence);
            GetVrConfigItem(vrEyeTaaJitterScale);
            GetVrConfigItem(vrEyeTaaMipBias);
            GetVrConfigItem(vrEyeTaaVarianceClamp);
            GetVrConfigItem(vrEyeTaaSharpen);
            GetVrConfigItem(vrEyeSmaaQuality);
            GetVrConfigItem(vrActorShadowSourceAnchor);
            GetVrConfigItem(vrActorToonSourceAnchor);
            GetVrConfigItem(vrToonFollowRef);
            GetVrConfigItem(vrVolumeSourceAnchor);
            GetVrConfigItem(vrDisableSourceCamera);
            GetVrConfigItem(vrSourceOffResetPerFrame);
            GetVrConfigItem(vrEyeJumpFloodTemporalOff);
            GetVrConfigItem(vrVlMotionBlurOff);
            GetVrConfigItem(vrEyeAsMainCamera);
            GetVrConfigItem(vrEyeObjectMotionVectorsOff);
            GetVrConfigItem(vrEyeDeferredStencilOff);
            GetVrConfigItem(vrEyeShadeBandBias);
            GetVrConfigItem(vrSourceCameraTiny);
            GetVrConfigItem(vrGripPanelTransparent);
            GetVrConfigItem(vrHideUiTextureOverlay);
            GetVrConfigItem(vrHandGlowSticks);
            GetVrConfigItem(vrPointerSizeScale);
            GetVrConfigItem(vrCameraYButtonBone);
            GetVrConfigItem(vrFpDirectionFollow);
            GetVrConfigItem(vrFollowSmoothingPreset);
            GetVrConfigItem(vrLiveGaze);
            GetVrConfigItem(vrLiveGazePreset);
            GetVrConfigItem(vrLiveGazeScope);
            GetVrConfigItem(vrFollowHorizontalMs);
            GetVrConfigItem(vrFollowVerticalMs);
            GetVrConfigItem(vrCameraTurnMode);
            GetVrConfigItem(vrCameraTurnSpeed);
            GetVrConfigItem(vrPanelCustomized);
            GetVrConfigItem(vrPanelPinned);
            GetVrConfigItem(vrPanelOffsetX);
            GetVrConfigItem(vrPanelOffsetY);
            GetVrConfigItem(vrPanelOffsetZ);
            GetVrConfigItem(vrPanelWidth);
#undef GetVrConfigItem
            // File-only: clamp once at load. The menu calls the other clamp
            // routines every paint; it must not write this VR-worker input.
            vrPointerSizeScale = gakumas::vr::pointer::ClampSizeScale(vrPointerSizeScale);
            if (vrToonFollowRef < 0 || vrToonFollowRef > 2) {
                vrToonFollowRef = kDefaultVrToonFollowRef;
            }
            ClampVrEyeAaSettings();
            ClampVrPointerSettings();
            ResetVrPointerSettings();
        }

        nlohmann::json BuildVrConfig() {
            auto config = g_vrConfigDocument.is_object()
                ? g_vrConfigDocument
                : nlohmann::json::object();
            config.erase("vrDebugEnabled");
            config.erase("vrProbeOnly");
            config.erase("vrPointerAnalyticAa");
            config.erase("vrPointerFixedProbe");
            config["schemaVersion"] = 1;
#define SetVrConfigItem(name) config[#name] = name
            SetVrConfigItem(vrDiagnosticsEnabled);
            SetVrConfigItem(vrEnabled);
            SetVrConfigItem(vrLocalizeText);
            SetVrConfigItem(vrMenuLanguage);
            SetVrConfigItem(vrNativeOnly);
            SetVrConfigItem(vrHeadPoseEnabled);
            SetVrConfigItem(vrWorldScale);
            SetVrConfigItem(vrStereoEnabled);
            SetVrConfigItem(vrStereoRenderScale);
            SetVrConfigItem(vrStereoLandscapeOnly);
            ClampVrEyeAaSettings();
            SetVrConfigItem(vrEyeOutlineWidth);
            SetVrConfigItem(vrBloomFollowSourceCamera);
            SetVrConfigItem(vrEyeAaMode);
            SetVrConfigItem(vrEyeTaaQuality);
            SetVrConfigItem(vrEyeTaaFrameInfluence);
            SetVrConfigItem(vrEyeTaaJitterScale);
            SetVrConfigItem(vrEyeTaaMipBias);
            SetVrConfigItem(vrEyeTaaVarianceClamp);
            SetVrConfigItem(vrEyeTaaSharpen);
            SetVrConfigItem(vrEyeSmaaQuality);
            SetVrConfigItem(vrActorShadowSourceAnchor);
            SetVrConfigItem(vrActorToonSourceAnchor);
            SetVrConfigItem(vrToonFollowRef);
            SetVrConfigItem(vrVolumeSourceAnchor);
            SetVrConfigItem(vrDisableSourceCamera);
            SetVrConfigItem(vrSourceOffResetPerFrame);
            SetVrConfigItem(vrEyeJumpFloodTemporalOff);
            SetVrConfigItem(vrVlMotionBlurOff);
            SetVrConfigItem(vrEyeAsMainCamera);
            SetVrConfigItem(vrEyeObjectMotionVectorsOff);
            SetVrConfigItem(vrEyeDeferredStencilOff);
            SetVrConfigItem(vrEyeShadeBandBias);
            SetVrConfigItem(vrSourceCameraTiny);
            SetVrConfigItem(vrGripPanelTransparent);
            SetVrConfigItem(vrHideUiTextureOverlay);
            SetVrConfigItem(vrHandGlowSticks);
            ClampVrPointerSettings();
            SetVrConfigItem(vrPointerSmoothEnabled);
            SetVrConfigItem(vrPointerSmoothMinCutoff);
            SetVrConfigItem(vrPointerSmoothBeta);
            SetVrConfigItem(vrPointerSizeScale);
            SetVrConfigItem(vrCameraYButtonBone);
            SetVrConfigItem(vrFpDirectionFollow);
            SetVrConfigItem(vrFollowSmoothingPreset);
            SetVrConfigItem(vrLiveGaze);
            SetVrConfigItem(vrLiveGazePreset);
            SetVrConfigItem(vrLiveGazeScope);
            SetVrConfigItem(vrFollowHorizontalMs);
            SetVrConfigItem(vrFollowVerticalMs);
            SetVrConfigItem(vrCameraTurnMode);
            SetVrConfigItem(vrCameraTurnSpeed);
            SetVrConfigItem(vrPanelCustomized);
            SetVrConfigItem(vrPanelPinned);
            SetVrConfigItem(vrPanelOffsetX);
            SetVrConfigItem(vrPanelOffsetY);
            SetVrConfigItem(vrPanelOffsetZ);
            SetVrConfigItem(vrPanelWidth);
#undef SetVrConfigItem
            return config;
        }
    }






    namespace Integration {
        thread_local ConfigLoadPurpose loadPurpose = ConfigLoadPurpose::LocalifyReload;
        void BeginLoad() { isConfigInit = false; }
        void CheckDocument(const nlohmann::json& config) {
            if (!config.is_object()) throw std::runtime_error("Localify config root must be an object");
        }
        void Parsed(const nlohmann::json& config) {
            if (loadPurpose == ConfigLoadPurpose::StartupMigration) ApplyVrConfig(config);
        }
        void LoadFailed() {
            dbgMode = false;
            enabled = false;
            if (loadPurpose == ConfigLoadPurpose::StartupMigration) ResetVrConfigFailClosed();
        }
        nlohmann::json SaveBase(const std::string& path) {
            auto config = nlohmann::json::object();
            if (std::filesystem::exists(path)) {
                std::ifstream in(path);
                in >> config;
                CheckDocument(config);
            }
            for (auto it = config.begin(); it != config.end(); ) {
                if (it.key().rfind("vr", 0) == 0) it = config.erase(it);
                else ++it;
            }
            return config;
        }
    }

    void LoadConfig(const std::string& configStr, ConfigLoadPurpose purpose) {
        struct LoadScope {
            ConfigLoadPurpose saved = Integration::loadPurpose;
            ~LoadScope() { Integration::loadPurpose = saved; }
        } scope;
        Integration::loadPurpose = purpose;
        LoadConfig(configStr);
    }

    namespace {
        bool configWritable = false;
        void ValidateCandidate(const nlohmann::json& config) {
            ValidateVrConfigSchema(config);
            ResetVrConfigFailClosed();
            const auto known = BuildVrConfig();
            for (auto it = known.begin(); it != known.end(); ++it) {
                if (!config.contains(it.key()) || it.key() == "schemaVersion") continue;
                const auto& value = config.at(it.key());
                const auto& expected = it.value();
                if ((expected.is_boolean() && !value.is_boolean()) ||
                    (expected.is_number_integer() && !value.is_number_integer()) ||
                    (expected.is_number_float() && !value.is_number()) ||
                    (expected.is_string() && !value.is_string()))
                    throw std::runtime_error("invalid VR configuration field type: " + it.key());
            }
            for (const auto& alias : {std::pair{"vrDebugEnabled", "vrDiagnosticsEnabled"},
                    std::pair{"vrProbeOnly", "vrNativeOnly"}})
                if (!config.contains(alias.second) && config.contains(alias.first) &&
                    !config.at(alias.first).is_boolean())
                    throw std::runtime_error("invalid legacy VR boolean field");
            ApplyVrConfig(config);
        }
    }

    bool SaveVrConfig(const std::filesystem::path& configPath) {
        if (!configWritable) {
            Log::Error("VR configuration was not initialized successfully; save refused");
            return false;
        }
        try {
            const auto config = BuildVrConfig();
            std::string error;
            if (!WriteVrConfigDocument(configPath, config, true, error)) {
                Log::ErrorFmt("SaveVrConfig failed: %s", error.c_str());
                return false;
            }
            g_vrConfigDocument = config;
            Log::Info("SaveVrConfig success");
            return true;
        }
        catch (const std::exception& e) {
            Log::ErrorFmt("SaveVrConfig error: %s", e.what());
            return false;
        }
    }

    bool InitializeHostedVrConfig(const std::filesystem::path& legacyDirectory,
        const std::filesystem::path& mixedConfig,
        const std::filesystem::path& managedConfigRoot,
        std::filesystem::path& selectedPath, bool& managedLayout,
        const std::filesystem::path& pluginDataRoot,
        const std::filesystem::path& logRoot) {
        configWritable = false;
        g_vrConfigDocument = nlohmann::json::object();
        ResetVrConfigFailClosed();
        VrConfigStartupOptions options;
        options.legacyDirectory = legacyDirectory;
        options.mixedConfig = mixedConfig;
        options.managedConfigRoot = managedConfigRoot;
        options.releaseDefaults = nlohmann::json::parse(kVrReleaseDefaults);
        for (auto it = options.releaseDefaults.begin(); it != options.releaseDefaults.end(); ++it)
            options.knownKeys.push_back(it.key());
        // Include known serialized fields even when the shipping template omits them.
        const auto known = BuildVrConfig();
        for (auto it = known.begin(); it != known.end(); ++it)
            options.knownKeys.push_back(it.key());
        options.validate = ValidateCandidate;
        VrConfigStartupResult result;
        try {
            if (!pluginDataRoot.empty()) {
                VrConfigManagedOptions managed;
                managed.startup = options;
                managed.pluginDataRoot = pluginDataRoot;
                managed.logRoot = logRoot;
                result = LoadManagedVrConfig(managed);
                const auto journalPath = pluginDataRoot / L"migrations" / L"layout-v2.json";
                if (!result.success && std::filesystem::exists(journalPath)) {
                    std::ifstream input(journalPath, std::ios::binary);
                    const auto journal = nlohmann::json::parse(input, nullptr, false);
                    if (journal.is_discarded()) throw std::runtime_error("invalid managed migration journal; fallback refused");
                    // An explicit tool apply persisted this plan before copying.
                    // Restart may finish it; it never invents a new old-data move.
                    result = ApplyVrConfigMigration(managed, journal.at("plan"),
                        journal.at("planSha256").get<std::string>());
                } else if (!result.success && result.outcome == "not-managed") {
                    const auto plan = PlanVrConfigMigration(managed);
                    const auto action = plan.at("action").get<std::string>();
                    if (action == "initialize" || action == "adopt-new")
                        result = ApplyVrConfigMigration(managed, plan, VrConfigPlanHash(plan));
                    else result = PrepareVrConfig(options); // Existing legacy remains until explicit apply.
                }
            } else {
                const auto standardRoot = mixedConfig.parent_path() / L"PluginData" / L"gaku.vrify";
                if (std::filesystem::exists(standardRoot / L"active-layout.json") ||
                    std::filesystem::exists(standardRoot / L"migrations" / L"layout-v2.json"))
                    throw std::runtime_error("standard VR layout requires Loader paths service; legacy fallback refused");
                result = PrepareVrConfig(options);
            }
        } catch (const std::exception&) {
            result.success = false;
            result.outcome = "configuration layout or migration state is invalid; original retained, fallback refused";
        }
        if (!result.success) {
            ResetVrConfigFailClosed();
            Log::ErrorFmt("VR configuration initialization failed (VR disabled; retry next start): %s",
                result.outcome.c_str());
            return false;
        }
        ResetVrConfigFailClosed();
        ApplyVrConfig(result.document);
        g_vrConfigDocument = result.document;
        selectedPath = result.configPath;
        managedLayout = result.managedLayout;
        configWritable = true;
        Log::InfoFmt("VR configuration: %s; original data retained", result.outcome.c_str());
        return true;
    }

    bool LoadVrConfig(
        const std::filesystem::path& configPath,
        const std::filesystem::path& legacyConfigPath) {
        configWritable = false;
        try {
            if (!std::filesystem::exists(configPath)) {
                bool managed = false;
                std::filesystem::path selected;
                return InitializeHostedVrConfig(configPath.parent_path(), legacyConfigPath,
                    {}, selected, managed);
            }
            std::ifstream in(configPath, std::ios::binary);
            if (!in) {
                throw std::runtime_error("cannot open dedicated VR config");
            }
            nlohmann::json config;
            in >> config;
            ValidateCandidate(config);
            // A present dedicated file is authoritative. Start from safe VR
            // defaults so omitted keys never bleed through from the legacy
            // localizationConfig.json loaded immediately before this call.
            ResetVrConfigFailClosed();
            ApplyVrConfig(config);
            g_vrConfigDocument = std::move(config);
            configWritable = true;
            Log::Info("LoadVrConfig success");
            return true;
        }
        catch (const std::exception& e) {
            ResetVrConfigFailClosed();
            g_vrConfigDocument = nlohmann::json::object();
            Log::ErrorFmt("LoadVrConfig error (VR disabled): %s", e.what());
            return false;
        }
    }
}
