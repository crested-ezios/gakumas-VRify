# 使用 GakuLoader 全新安装

请使用面向新根目录、组件相互配套的 GakuLoader + GakuVRify 完整安装包。安装前请完全退出游戏。独立版 VRify 和 GakuLoader 均占用游戏根目录的 `version.dll`，请根据所选方案仅使用对应代理。

```text
<游戏根目录>/
  gakumas.exe
  version.dll                         # GakuLoader 代理
  openxr_loader.dll                   # 配套的 OpenXR loader
  GakuLoader/
    plugins/GakuVRify.dll             # 托管版 VRify 插件
    Configs/gaku.vrify/config.json    # 全新安装时自动创建的配置
    PluginData/gaku.vrify/            # 托管布局状态数据
    Logs/gaku.vrify/                  # 启用时的诊断日志
```

`Configs`、`PluginData` 与 `Logs` 目录存放的是运行时数据，无需在首次启动前手动创建。配置并启用目标 OpenXR 运行时后启动游戏，随后检查 VRify 状态与日志。仅安装成功或插件成功加载并不代表头显渲染、相机共存或画面表现已通过验收。

仓库中通用的 `package-plugin.ps1` 与 `install-plugin.ps1` 仍生成或安装旧版 `gakumas-local` 布局。原生 VR 配置迁移工具也针对该旧根目录。**请勿将它们用作新根目录的全新安装工具。** 通用安装器在无法核验配套提供方时，也会拒绝包含 Localify/Motion 的完整 Unity VR 组合。请使用经过协调的完整包，不要混用来源不相关的二进制文件。

既有的 `gakumas-vr/config.json` 可能会原位保持生效；变更布局前请先备份旧配置与日志。带日期的旧配置与迁移文档描述的是旧根目录流程，不可当作新根目录的迁移步骤。切换至独立版是另一套工作流，使用其自身的代理与旧版配置。
