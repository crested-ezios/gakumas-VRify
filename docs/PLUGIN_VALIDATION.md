# 托管插件验证

本文记录 `5c9d6ab` 初始构建产物。后续启用的无 VR 启动工作与当前产物证据见 [无 VR 冒烟测试](VRLESS_SMOKE.md)。下文记录的哈希值属于历史数据。

该开发实现能够成功构建并通​​过以下离线检查，尚未部署到正常游戏环境中或在头显内验收。用法及剩余的相机/渲染共存工作见 [GAKULOADER_PLUGIN.md](GAKULOADER_PLUGIN.md)。

## 构建产物

- Release x64：`build/bin/plugin/x64/Release/GakuVRify.dll`，大小 4,777,984 字节。
- SHA256：`9e2865aff345b47b8673daf43aa32eb63d6ebefea6c257d939dff0a50c6a2d0d`。
- 源码输入指纹：`785d198bd5d310c1d32dfa0114175e5756bb7dcdd7d3884ce4849b3210a5b6d2`。
- GakuLoader SDK 指纹：`76987362c4885ba73d35e11a4c99486809bc9aa0550059f74aceca1c12d7c801`。
- 锁定的上游提交：`318107b6af69740d55f3674ad29644f0b21284e7`。
- 编译器：通过 Visual Studio 2026 安装的 MSVC x64 工具链，Release CMake 目标。第三方依赖来自锁定的 Conan 缓存。
- PE 导出项：仅包含 `GakuPlugin_Query`；无 version 代理或测试宿主导出项。未引入 MinHook 实现或导入。生成的构建目标排除了代理、旧版引导/平台/HookManager、嵌套 ModLoader、旧桌面 GUI 及更新器。
- `build/plugin-package/` 包含插件本身、锁定的 OpenXR loader 1.1.61、独立配置、版本标记、许可证及经过完整校验的文件哈希清单。该目录下不包含 `version.dll` 或 Localify 配置。

## 通过的检查项

| 检查项 | 证据与限制 |
|---|---|
| 生产 DLL 契约与生命周期 | `tests/test-hosted-plugin.ps1`：9 个独立的本地进程；ABI/输出保持、4 项必需服务失败、服务冲突、禁用状态生命周期、无运行时启用、格式错误配置。未启动游戏、XR 或窗口。 |
| 生产 Hook 门面 | 同一运行器直接编译 `HostedHooks.cpp`：全批次转发、精确宿主状态、创建/启用失败时保留原状态、首次失败锁存、仅释放当前所有者的去重租约。 |
| 安装器与恢复机制 | `tests/test-plugin-install.ps1`：将实际安装包部署至测试桩目录，保留宿主及 Localify/VR 配置，还原旧 DLL，清理新安装文件，在修改任何文件前拒绝覆盖后续的用户编辑，拒绝不支持的完整 Unity 组合。 |
| 源码归属检查 | `tests/test-source-layout.ps1`、上游边界与归属检查；10 项 `test-source-pipeline.ps1` 用例（含模式/SDK 漂移检查）。预期的补丁拒绝测试桩包含在该套件中。 |
| 共享相机入口回归 | `tests/test-shared-entry-contracts.ps1`：提取的暂存 Hook 体、原始计数与顺序、嵌套渲染、异常及提前返回路径。非实机 Unity 环境。 |
| 既有 VR 数学与输入 | `test-head-pose-core.ps1`、`test-free-camera-turn.ps1`、`test-pointer-smoother.ps1` 使用已安装的 MSVC 工具链通过测试。 |
| 独立版 Hook 管理器 | `test-hook-manager.ps1` 仍通过了部分创建/排队/应用失败以及跳板生命周期的回归测试。未重新构建完整的独立版代理。 |

本地证据记录于 `build/plugin-build.log`、`build/hosted-plugin-tests/`、`build/plugin-install-tests.log`、`build/pipeline-tests.log` 以及各回归日志中。Debug 模式、实机 Unity/IL2CPP 运行时行为、头显立体/6DoF、控制器输入、场景切换、XR 丢失以及优雅退出游戏仍未经实机测试。与 GakuLocalify/Motion 的完整 Unity 共存、最终姿态视线时序、Motion 状态服务消费方及宿主桌面参数面板属于后续工作。

## 保留的本地状态

初始被忽略的上游缓存中有 4 个 ImGui 文件存在改动。其完整检出保留在 `.upstream/localify-preserved-20261026-015630/`；另行在锁定提交上创建了干净检出用于暂存，未丢弃任何修改。基于既有锁定要求修复了迁移后的 Conan/CMake Windows 启动器。未修改 `legacy` 目录树与同级的 GakuLoader/GakuMotion 仓库。
