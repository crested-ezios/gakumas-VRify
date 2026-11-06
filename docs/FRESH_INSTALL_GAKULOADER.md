# Fresh installation with GakuLoader / 使用 GakuLoader 全新安装

## English

Use the matched complete GakuLoader + GakuVRify package intended for the new root. Close the game before installation. The standalone VRify proxy and GakuLoader both occupy the game's root `version.dll`; use only the proxy required by the chosen installation.

```text
<game directory>/
  gakumas.exe
  version.dll                         # GakuLoader proxy
  openxr_loader.dll                   # matching OpenXR loader
  GakuLoader/
    plugins/GakuVRify.dll             # hosted VRify plugin
    Configs/gaku.vrify/config.json    # created for a fresh configuration
    PluginData/gaku.vrify/            # managed layout state
    Logs/gaku.vrify/                  # diagnostics when enabled
```

The directories under `Configs`, `PluginData` and `Logs` are runtime data, not files that must all exist before the first launch. Enable and configure the intended OpenXR runtime, launch the game, and check the VRify state and logs. A successful install or plugin load alone does not establish headset rendering, camera coexistence, or visual acceptance.

The repository's generic `package-plugin.ps1` and `install-plugin.ps1` still produce/install an older `gakumas-local` layout. The native VR config migration tool also targets that old root. **Do not use these as the fresh installer for the new root.** The generic installer also refuses full Unity VR with Localify/Motion when it cannot verify matching providers. Use a coordinated complete package rather than combining unrelated binaries.

An existing `gakumas-vr/config.json` may continue to be selected in place; keep the old settings and logs backed up before changing layout. Dated configuration and migration documents describe the older root and must not be read as a new-root migration procedure. Switching to standalone is a separate workflow; it uses its own proxy and legacy configuration.

## 简体中文

使用面向新根目录、组件相互配套的 GakuLoader + GakuVRify 完整安装包，安装前完全退出游戏。独立版 VRify 和 GakuLoader 都占用游戏根目录的 `version.dll`；按所选方案使用对应代理。

上方目录树中，插件位于 `GakuLoader/plugins/`，OpenXR loader 与游戏程序同级。全新配置位于 `GakuLoader/Configs/gaku.vrify/`，布局状态位于 `GakuLoader/PluginData/gaku.vrify/`，启用时的诊断日志位于 `GakuLoader/Logs/gaku.vrify/`。这些运行时目录不要求在首次启动前全部存在。设置好目标 OpenXR 运行时后启动游戏，并分别确认插件状态、头显画面与实际镜头表现。

本仓库通用的 `package-plugin.ps1`、`install-plugin.ps1` 仍生成或安装旧 `gakumas-local` 布局，原生 VR 配置迁移工具也以旧根为目标，**不能直接用于新根全新安装**。通用安装器无法确认配套 provider 时也会拒绝完整 Unity VR 与 Localify/Motion 混装；请使用配套完整包，不要混用来源不明的二进制。

已有 `gakumas-vr/config.json` 可能继续原位生效。变更布局前保留旧配置和日志备份；带日期的配置与迁移文档描述旧根，不等于新根迁移步骤。独立版是另一种安装方式，使用自己的代理和旧版配置。
