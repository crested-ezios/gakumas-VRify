# Localify 相机与渲染共存

本文汇总历史配套构建及其受限的运行时验收结论，保留技术结论与产物身份，不包含本机路径、设备标识符或原始进程日志。本文不用于验证当前检出重新编译的新 DLL。

## 故障与修复

此前 VRify 构建在 XR 启动前中断，原因是 Localify 已占用 `Camera.get_main`、`CinemachineBrain.PushStateToUnityCamera` 与 `RenderPipeline.EndCameraRendering`。宿主租约返回 `GAKU_E_CONFLICT`；返回的运行时就绪回调并不代表 Unity 或 XR 初始化成功。保留的 Motion DLL 并非这三个目标的所有者。

Localify 的 `gakulocalify.camera-render` v1 桥接为每个目标保持单一 Detour。VRify 在订阅前先校验所有三个解析后的地址。服务缺失时保持独立路径；不兼容或不匹配的提供方会明确报错，而不会抢占租约。不会复制外部跳板（trampoline）。

- 主相机（Main camera）：执行游戏原始逻辑，随后进行 VR 观察或覆盖。
- Cinemachine：先执行 VR 预回调，游戏原始逻辑严格执行一次，再执行 VR 后回调。单一常驻快照覆盖这对回调，包括原始逻辑执行期间的重入或退订。
- 结束相机（End camera）：执行游戏原始逻辑，随后执行 VR 结束相机处理。

Localify 对应的自由相机分支会主动让位给处于活动状态的 VR 订阅者。这并不保证所有自由相机功能可用；历史测试配置禁用了自由相机与陀螺仪模拟。停机时在退订前先关闭 VR 准入，不保证回调完全排空或支持热卸载。函数与上下文在进程退出前保持常驻。

宿主 ABI、保留的 Motion DLL 以及现有的 `gakulocalify.camera-write` 服务均保持不变。Localify 提供方的源码版本为 `d8de333c3b899fe9884f5e51d2850dec1a982b92`。

## 历史验证

- 两个生产 Release DLL 均成功构建。
- Localify 源码级测试针对三个实际 Hook 体进行了校验，涵盖原始调用次数与顺序、非活动行为、异常、重新绑定/重入、Close 以及飞行中的快照。现有的 camera-write、二进制 ABI 与独立隔离检查均通过。
- 无 VR 集成测试通过全部 **13/13** 项用例。两项共存用例在测试桩 DLL 中使用实际 Localify 提供方类配合真实宿主租约：三个共享目标匹配，四个目标完成修补与恢复，Actor 参与者数量从 2 恢复至 0。错误的目标在后端启动前报错中止。
- 独立的已安装 OpenXR 探测器在 VirtualDesktopXR 下达到了系统与 D3D11 要求，返回 `XR_SUCCESS`。该探测器未创建会话或帧，其诊断源码位于 `tests/vrless/OpenXrAvailabilityProbe.cpp`；冒烟测试运行器不会调用它。

这些源码级跨组件测试需要单独提供实现源码，属于可选的维护者集成检查，不是公共模拟宿主测试或普通插件编译的前提条件；详见 [BUILDING.md](../BUILDING.md)。

## 配套部署与运行时观察

游戏在替换文件前已完全关闭，两份已安装 DLL 的哈希均匹配：

| 文件 | SHA256 |
| --- | --- |
| GakuLocalify.dll | `62d59aeed6641a2146f924ab20955925cae8163f9b43db3f132ebefc385a6075` |
| GakuVRify.dll | `95a48b8b1d175821a1ea155560be34ee8737ae2daa348340f7c3c65bdf20fef7` |

宿主代理、保留的 Motion 及相关配置均保持部署前记录的哈希。历史本机恢复清单与备份不作为安装包分发。

后续正常运行成功匹配所有三个共享目标，并报告相机与渲染 Hook 就绪。配合 Meta Quest 3 的 VirtualDesktopXR 达到了 `SessionRunning` 与 `PoseReady` 状态。双眼描述符分辨率为 2496×2592；投射提交持续至 10800 代。观测到的两次场景挂起分别在 54 毫秒与 23 毫秒后解除为 `stereo-ready`。这属于启动与场景切换时的正常过渡等待。

已确认头显画面正常，并使用 GakuMotion 完成了一次动作播放，兼容性正常。该结论仅涵盖该历史配套构建与保留的 Motion 二进制文件，不代表所有场景、资产、导入镜头或视线功能均已通过验收。验收运行期间未替换已加载的 DLL。公开的 [证据汇总](evidence/camera-coexist-20261027.json) 保留了相关输入哈希与限定观察结论。

## 当前限制

只有相互配套的提供方与消费方之间才具备该桥接；新版 VRify 无法自动修复旧版提供方。通用安装器未对外部 Localify/Motion 组件进行校验或升级，因此策略保持保守。

后续的 begin-camera 集成在无 VR 状态下恢复了 Motion 手动移动与旋转镜头。**VR 激活时 Motion 镜头控制仍属于已知问题**，记录于 [Motion 相机共存](MOTION_CAMERA_COEXIST.md)。先前的头显画面与动作播放验收结论不能扩大覆盖这一尚未解决的问题。
