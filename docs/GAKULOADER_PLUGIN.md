# GakuVRify 插件集成

本项目源码可构建由 GakuLoader 托管的 `GakuVRify.dll`，支持与 Localify 的相机/渲染共享以及 Motion 的 begin-camera 提供方。历史测试报告仅描述其对应构建与观察结论，不代表当前检出新编译的 DLL 自动通过验证。

## 构建与安装

工具链与检出专属的 SDK 默认配置见 [BUILDING.md](../BUILDING.md)。两个仓库均支持以下 PowerShell 7 命令：

```powershell
./build.ps1 -Mode Plugin -Configuration Release
./tests/test-hosted-plugin.ps1 -Configuration Release
```

输出路径为 `build/bin/plugin/x64/Release/GakuVRify.dll`。发布版检出自带公开的 Loader SDK；开发版检出默认使用同级的 GakuLoader SDK。可通过 `-LoaderSdkDirectory` 参数指定兼容的 SDK 路径。公共模拟宿主测试不需要私有的 Loader 或 Motion 实现。

全新 `GakuLoader/` 安装请使用配套的完整安装包，并参考 [全新安装指南](FRESH_INSTALL_GAKULOADER.md)：VRify DLL 位于 `GakuLoader/plugins/`，OpenXR loader 位于 `gakumas.exe` 同级，插件的托管配置、状态与日志分别位于 `GakuLoader/Configs/gaku.vrify/`、`GakuLoader/PluginData/gaku.vrify/` 与 `GakuLoader/Logs/gaku.vrify/`。

通用的 `package-plugin.ps1` 仍生成 `gakumas-local` 安装包，`install-plugin.ps1` 也会安装为旧布局。原生迁移工具同样针对旧版 `gakumas-local` 根目录。**请勿将它们作为新根目录的全新安装器。** 通用安装器在无法核验配套提供方时，也会保守地拒绝与 Localify/Motion 混装完整 Unity VR。必须使用配套的完整包，不能仅凭文件名推断兼容性。

既有的专属配置可保留在 `gakumas-vr/config.json` 中继续生效；变更布局前请先备份。[首次使用兼容记录](CONFIG_LAYOUT.md) 与 [布局 v2 迁移记录](CONFIG_MIGRATION_V2.md) 描述的是原旧根目录的工作流，不可视作新根目录的迁移命令。还原程序文件并不等同于反向数据迁移。

独立版使用独立的代理与旧版配置，切勿将其 `version.dll` 与 GakuLoader 代理混用。切换版本时，请先关闭游戏，备份两套配置，并显式指定所需的独立版配置；详见 [BUILDING.md](../BUILDING.md#standalone-proxy-workflow)。

## 运行时与共享接口

- `GakuPlugin_Query` 协商 Loader ABI v1，不启动线程或 XR。激活阶段查询日志、运行时、Hook、Actor 入口及可选的 paths 服务。优先读取已提交的标准配置，未显式迁移前支持兼容的旧配置。已知混合 VR 字段可单次导入，不涉及 Localify 资源逻辑。全新初始化采用嵌入的 Release 默认值。配置无效、不受支持或已提交数据缺失时，将直接禁用 VR，不会覆盖原文件或静默回退。
- 运行时就绪依托宿主挂载的 IL2CPP 工作线程。插件模式不包含二级代理引导、Localify 桌面 GUI / 资源更新器以及嵌套插件扫描器。
- 可执行 Hook 请求通过宿主绑定所有者的 `acquireBatch` 发起。插件未链接 MinHook 实现。初始批次租约失败将保留原始状态，阻止 XR 启动，并仅释放当前所有者的租约。
- 宿主持有 `CampusActorController::LateUpdate`；VRify 在采样前注册回调，并在自然视线回调之后执行。这不代表与 Motion 后续的 Actor 回调建立了最终姿态/视线合成。
- 停机时关闭 Unity 准入，注销 Actor 回调，停止 XR 并释放所持有的租约。常驻上下文与跳板可能在准入关闭后继续驻留。不支持热卸载/重启，亦不保证回调完全排空。

| 接口 | 用途与边界 |
| --- | --- |
| `gaku.vrify.service` v1 | 只读活动 VR/视线查询；需要活动会话，停机后调用返回 false。不暴露未初始化的头部姿态。 |
| `gaku.vrify.diagnostics` v1 | 校验大小的生命周期快照，涵盖 Unity/Actor/图形就绪状态、后端尝试次数、原始 XR 结果及显式不可用原因。 |
| `gakulocalify.camera-render` v1 | 主相机、Cinemachine 与结束相机目标的可选外部提供方。VRify 在订阅前核验确切解析的目标，不共享外部跳板。 |
| `gaku.vrify.camera-begin` v1 | VRify 为 Motion 发布的两个 begin-camera 边界。原始逻辑执行一次；VR 逻辑先于其执行，Motion 通知在其后执行。排除 VR 双眼相机。 |

提供方缺失时保留独立路径。不兼容的 ABI 或目标将明确报错，而非抢占其他所有者的独占 Hook。相机渲染仲裁需要正在运行的 XR 会话与就绪的 Unity 环境；在不可用或停止状态下不会抑制 Localify 的原生相机行为。

来自锁定 Localify 上游的辅助编译单元仍参与编译，用于元数据、相机计算、配置与菜单字符串；托管模式下禁用了翻译与资源处理逻辑。这并不等同于完全剔除源码与依赖项。桌面参数面板以及最终姿态/视线协同属于独立工作。

## 验证与已知限制

公共默认验证使用源码流水线测试桩、公共相机提供方契约以及基于模拟宿主的生产 DLL，不编译私有核心实现。具体命令见 [BUILDING.md](../BUILDING.md)。

生产宿主与 Localify 提供方冒烟测试需要 `-RunMaintainerIntegration` 参数与单独提供的源码树。Motion 实现比对同样属于显式的维护者检查项。已安装运行时与设备可用性探测器属于单独的手动诊断源码，冒烟运行器和默认公共检查均不调用。这些都不是常规公共构建的前提条件。

历史文档保留各自对应的输入版本记录：

- [初始 DLL 生命周期与门面检查](PLUGIN_VALIDATION.md)
- [启用无 VR 启动](VRLESS_SMOKE.md)：测试桩元数据/后端控制及锁定的官方 loader，未进行成功的 XR 渲染。
- [Localify 相机与渲染共存](CAMERA_COEXIST.md)：共享目标加配套构建的头显画面和一次 Motion 播放。
- [Motion 相机共存](MOTION_CAMERA_COEXIST.md)：无 VR 手动移动与旋转已确认；**VR 激活时 Motion 镜头控制仍为已知问题**。

这些检查不代表所有场景、设备、导入相机轨、控制器行为、XR 丢失、最终姿态/视线时序或所有停机路径均已通过验证。编译、安装包验证、进程加载与视觉验收需分别对待。
