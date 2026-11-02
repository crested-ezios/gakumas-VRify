[English](README.md) | **简体中文** | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md)

# gakumas-VRify

为《学园偶像大师》（学園アイドルマスター）加入 VR 支持的非官方 Mod，**适用于 Windows x64 DMM 版**，基于 [gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm)。

本分支包含当前 **GakuLoader 插件集成版 `GakuVRify.dll`**，已实现 Localify camera/render 与 Motion begin-camera 共享桥。源码默认构建 Plugin；原独立代理版保留为显式选项。

[源码构建](BUILDING.md) · [插件安装与接口](docs/GAKULOADER_PLUGIN.md) · [镜头兼容范围](docs/MOTION_CAMERA_COEXIST_20261027.md) · [源码归属](UPSTREAM.md)

## 使用前请阅读

- 游戏本身并非面向 VR 开发，可能出现人物缺失、位置错误和画面闪烁。
- **请注意眩晕和闪烁风险。如感到不适，请停止使用。**
- 使用本地游戏修改前，请查阅包括[用户协议（日文）](https://legal.bandainamcoent.co.jp/terms/nejp)在内的相关规则，并对自己的账号和使用后果负责。
- 本项目为粉丝自制，与上游项目及游戏相关方无隶属或背书关系。

## 使用插件集成版

使用配套的 GakuLoader 与插件包。自行编译请参阅[构建指南](BUILDING.md)；公开 Loader SDK 头文件已随仓库提供，编译插件不需要私有 Loader 实现。

安装前退出游戏。插件包将 `GakuVRify.dll` 放入 `gakumas-local/plugins/`，OpenXR loader 放在游戏可执行文件旁，VR 配置位于 `gakumas-vr/`。GakuLoader 单独安装。包清单、配置保留和恢复方式见[插件指南](docs/GAKULOADER_PLUGIN.md)。

通用安装器无法验证外部 Localify/Motion provider 是否配套，因此仍保守拒绝完整 Unity VR 混装。共享桥已经实现；该安装器不负责自动升级和验证整组组件。

[上游 Releases](https://github.com/KagaminTheMirror/gakumas-VRify/releases) 提供的是**独立代理版**，属于另一条安装路径，不能据此认定包含本分支的插件集成。不要在同一安装中混用独立代理与 GakuLoader 代理。

## 兼容性与已知限制

- 历史头显测试覆盖 Quest 3 + Virtual Desktop，不代表其它设备或全部场景已验收。
- 一组配套构建曾确认头显画面正常，并完成一次 Motion 动作播放，范围见[相机共享报告](docs/CAMERA_COEXIST_20261027.md)。
- **已知问题：**无 VR 时的 Motion 手动镜头移动、旋转已确认；启用 VR 后 Motion 镜头控制仍会失效。begin-camera 共享桥没有解决剩余的调度问题，详见[镜头共存记录](docs/MOTION_CAMERA_COEXIST_20261027.md)。
- 源码构建、合成检查、磁盘文件校验与游戏/头显验收分别记录。历史报告不自动认证新编译的 DLL；不支持热卸载。

## Quest 手柄操作指南

![Quest 手柄操作指南（简体中文）](docs/controller-guide/exports/gakumas-vrify-quest-guide-zh-CN.png)

## 开发与许可

公开检查使用合成夹具和 mock host。可选的生产宿主、Localify、Motion 源码联测需要另行提供对应源码，见[构建指南](BUILDING.md)。

项目采用 [GPL-3.0](LICENSE)。随附 Loader SDK 保留 [MIT 许可](sdk/gakuloader/LICENSE)；其它组件及控制器插图保留各自条款，见[第三方通知](THIRD_PARTY_NOTICES.md)。仓库不包含游戏资源或游戏安装文件。
