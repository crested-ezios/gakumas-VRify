# 发布来源与历史

本分支是为在现有公开 Fork 中发布而准备的完整托管插件集成。其公开父提交为 `feat/plugin-host` 上的 `562bf40c341758c205a18104f4d254247b5ea058`。完整功能源码检查点为 `4d9b684499aeb9040736852b02ab4f9a2d6d84fc`，对应树为 `bc61c976bb8a1e8ec73c75933adbf922328b4718`。

发布提交将完整检查点树应用于已有的公开父提交，并附加下文所述的公开 SDK、构建与测试改动。它是一个父提交已公开的常规新提交。原始的 6 个集成提交仅保留在本地源码归档/引用中，不作为发布历史的祖先节点。发布本分支不会发布原始历史日志版本、不会重写公开 Fork 历史、不会直接合并入 `main`，亦不会更改默认分支。

## 原始集成检查点

以下标识符用于描述功能源码的来源，而非必须推送到公开历史中的提交：

```text
5c9d6ab9849e25af1e3ebdba988b679c01bf77f5 feat: add GakuLoader-hosted VRify plugin and offline validation
b6bb39eb14c15a1cc911be924c74403236bf5643 test: verify VR-less startup with real host and explicit XR availability
de727ed3acda64b5bbafade43e84657a1f9c2c68 fix: join Localify camera render bridge before starting XR
5ff5b91d75d1ec1d2861d2253467c668c4b4844e Share camera begin boundaries with Motion without exclusive hook conflicts
d466084f237373dd6e57acf485c5d58a268062c9 Record paired camera coexistence deployment awaiting game acceptance
4d9b684499aeb9040736852b02ab4f9a2d6d84fc Record non-VR camera acceptance and deferred VR camera scheduling issue
```

## 准备工作改动

- 四语 README、构建说明以及托管插件使用指南准确描述当前插件目标、已安装的 SDK、公开检查项及运行时限制。
- 带日期的相机报告与证据 JSON 保留技术结果、产物哈希与验收边界。冗余的日志块、进程 ID、临时设备标识符、模块清单及本机绝对路径已替换为技术摘要。历史报告不代表新 DLL 通过验证。
- `sdk/gakuloader/GakuPlugin.h` 是精确的消费方 C ABI 头文件，附带完整的原始 MIT 声明与 [溯源说明](../sdk/gakuloader/PROVENANCE.md)。本源码树中未复制 Loader 注册表、Hook 引擎、UI 实现、Motion 实现或 Localify 提供方实现。现有的三个相机与服务头文件为消费方/服务声明，VRify camera-begin 提供方属于 VRify 自身实现。
- 构建系统选取并锁定已安装的 MSVC 工具集，生成实际的 Conan 编译器 profile，并允许使用兼容的旧依赖 ABI 以便复用缓存。`-Offline` 参数仅使用现有的工具、上游与依赖包。默认构建不依赖私有的同级源码目录。
- 公开的相机提供方测试仅使用自带 ABI 直接测试自有的提供方。现有的真实 DLL 生命周期与门面测试采用模拟宿主。跨组件的生产宿主与 Motion 测试需要维护者显式开启并单独提供实现源码树。
- 插件安装包保留 SDK 的原始 MIT 许可证（作为 `GakuLoader-SDK.txt`）。

锁定的 Localify 上游源码仍属于独立的构建依赖项，说明见 [UPSTREAM.md](../UPSTREAM.md) 与 `upstream.lock.json`；未将公开检出错误表述为内嵌的完整上游快照。

## 运行时范围

本次准备保留了完整检查点中的 Localify camera-render 消费方、VRify 诊断与服务，以及 Motion camera-begin 提供方。对于历史配套构建，无 VR 状态下的 Motion 相机手动移动与旋转已通过验收。在 VR 激活状态下，Motion 相机控制仍属于已知问题。编译与测试桩检查并不代表头显渲染或常规相机组合成功。限制条件见 [插件集成指南](GAKULOADER_PLUGIN.md) 及相关报告。

## 托管配置与双模式维护

托管配置刷新（`ce7cdba63f352bea230bf5beb3adcfc336720264`）基于源码检查点 `e6b17a21c94b30e2d94a925b5dcb45c742f19b26`，包含 `b50c97ad5c459b7372ac188e868e43186ef867fd` 与布局 v2 迁移，并保留了公开 SDK、编译器选取、离线构建与打包适配。

后续维护源码检查点为 `3d51912ba73307e078473603b70588ab8b631686`，移除了已废弃的内置 ModLoader/API 及分发点，同时保留了原始独立版代理、引导及 MinHook 后端。`VrBootstrap.cpp` 与 `HookManager.cpp` 与未托管前的 `c353d1a847635f69a2547c55c146614962b012fe` 基线保持一致。5 个独立版 cpprest 消费方采用局部 C++20 编译器选项；多次 XR 可用性重试仅在不可用原因变化时通知宿主。新增的独立版测试桩与重试断言属于测试桩检查，具有相同的运行时与头显限制。

本次准备保留了公开的构建与测试入口、已安装 SDK 默认值、组件声明及迁移协议，未导入内部交接文档、原始日志、依赖缓存或独立历史私有候选的审查元数据。`SOURCE_MANIFEST.json` 将此维护映射及其公开适配关联至先前的暂存检查点。

## Null 相机目标维护

相机目标选择修复仅映射了源码检查点 `b6fe8ac` 的托管消费方改动与回归测试，并将测试适配为自带的公开 SDK。先前暂存检查点的源码流水线、编译器选取、SDK 与依赖锁定保持完好。归属规则与有限的桌面验收证据见 [维护说明](CAMERA_NULL_PROVIDER.md)。本次选取未导入无关的源码历史、内部日志或原型功能。
