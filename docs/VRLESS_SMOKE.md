# 无 VR 冒烟测试

生产版 GakuVRify DLL 现已通过在未连接外部 Localify 模块与头显情况下的启用启动路径冒烟测试。本测试用于检验初始化边界与实际宿主 Hook 租约，不代表真实 Unity 场景或 VR 渲染已通过验证。

## 设计

运行器根据同级 GakuLoader 的生产 `PluginHost`、`ActorEntry`、`Hotkeys`、跟踪代码及内嵌 MinHook 编译出原生可执行程序，未定义任何宿主测试缝隙宏，仅将桌面 UI 与对话框外壳替换为存根。宿主加载生产版 `GakuVRify.dll`，发布实际绑定所有者的服务，调用激活、运行时就绪与停机接口，并安装与恢复真实的 Detour。

每个测试用例均在新的子进程与隔离目录中运行。现场不存在 Localify DLL、翻译/配置数据、Vuplex 模块或旧版代理。激活发生在加载任何 GameAssembly 之前。系统记录模块清单、宿主与 VR 日志、生命周期快照以及后端/元数据计数器。通过子进程超时机制检测崩溃或挂起，不修改已安装的游戏文件。

测试包含三层正向用例与两层反向对照：

| 测试层 | 实际执行内容与预期边界 |
|---|---|
| 纯原生（Native-only） | 完全不包含 GameAssembly。执行真实宿主激活与 D3D11 租约，随后呈现缺失 loader / 缺失运行时 / 缺失头显状态。 |
| 托管 Unity 启动 | 激活后加载最小化的 GameAssembly 元数据测试桩。生产初始化逻辑枚举该元数据，安装 4 个关键相机/渲染 Detour 与 2 个 Actor 参与者，随后达到相同的三个 VR 边界。 |
| 官方 Khronos loader | 原生与 Unity 路径均加载锁定的官方 1.1.61 loader。仅在子进程中将 `XR_RUNTIME_JSON` 指向刻意不存在的文件，产生真实的 `XR_ERROR_RUNTIME_UNAVAILABLE`。 |
| 无效 loader 对照 | 损坏的程序本地 OpenXR DLL 必须报告 `Faulted`，且不可用原因为 `none`，不得误判为缺失 VR 硬件。 |
| 缺失元数据对照 | 隐藏测试桩的关键 Camera.get_main 元数据必须报告 `initialization-failed`，后端尝试次数为 0，且不可用原因为 `none`。已安装的租约必须全部释放。 |

精简的 OpenXR 测试后端仅提供扩展与实例元数据分发。一种模式从 xrCreateInstance 返回 `XR_ERROR_RUNTIME_UNAVAILABLE`；另一种从 xrGetSystem 返回 `XR_ERROR_FORM_FACTOR_UNAVAILABLE`。该后端从不提供系统、会话、图形绑定、控制器动作、姿态或帧。非预期的会话与姿态调用会报错并计入统计。真实 loader 用例确保了运行时不可用的结论在该虚拟后端之外同样成立。

## 生产可观测行为

- 纯原生初始化不再需要 GameAssembly 句柄。完整 Unity 模式仍需要有效的游戏元数据，该要求独立于 Localify。
- `gaku.vrify.diagnostics` v1 暴露只读且校验大小的生命周期快照，报告 Unity/Actor 就绪状态、图形 Hook 就绪状态、后端尝试次数、原始 XR 结果及显式不可用原因。现有的 `gaku.vrify.service` v1 表保持不变。
- 缺少 loader、运行时或头显时表现为 `Unavailable`，而非 VR 激活成功或 Localify 初始化错误。无效的后端或 Unity 初始化仍属于独立的故障（Fault）。生产插件中未编译任何虚假后端代码。
- 即使关闭了详细的 VR 文件日志且无控制台，不可用与故障转换仍会写入原生宿主日志。缺少原生 loader 的用例显式测试了该配置。
- 缺少运行时或头显时保留现有的重试策略。冒烟运行器等待显式的不可用快照与宿主日志消息，随后请求常规的宿主关闭，并验证取消操作、参与者注销以及 4 个原始 Hook 目标的恢复情况。重试不被视作挂起。

## 复现结果

执行结果：**10/10 项通过**，所有子进程均以代码 0 退出，无超时发生。宿主关闭最长耗时为 **563 毫秒**（断言阈值为 4,000 毫秒）。全部 4 项正向 Unity 用例均安装并恢复了 4 个关键目标，并移除了两个 Actor 参与者。托管/对象调用计数器与虚拟会话/姿态调用计数器保持为 0。每个不可用用例均将原因写入了宿主日志（包括禁用 VR 详细日志的用例）。

测试的生产 DLL SHA256 为：
`f4b8d9b77984a74a780af335575a7d469580cf5698054680d7da006985ff1e14`。

| 预期中止点 | 结果 |
|---|---|
| 缺少程序本地 loader | `Unavailable / loader-missing`；未伪造 XR API 结果 |
| 缺少运行时（虚拟与官方 loader） | `Unavailable / runtime-missing`，XR 结果 `-51` |
| 实例创建后缺少头显 | `Unavailable / headset-missing`，XR 结果 `-35` |
| 无效 loader | `Faulted`，不可用原因 `none` |
| 缺少关键游戏元数据 | `initialization-failed`，0 次后端尝试，不可用原因 `none` |

可选的维护者复现测试需要单独提供 GakuLoader 宿主/MinHook 以及 Localify 提供方的实现源码。这些内容未打包在此处，公共构建与模拟宿主检查亦不需要：

```powershell
./build.ps1 -Mode Plugin -Configuration Release
./package-plugin.ps1
./tests/test-vrless-smoke.ps1 -RunMaintainerIntegration -GakuLoaderRoot '<宿主源码路径>' -LocalifyRoot '<提供方源码路径>'
```

打包命令会获取并校验锁定的官方 loader。冒烟测试运行器同样接受 `-PluginPath`、`-GakuLoaderRoot` 与 `-RealOpenXrLoader` 参数；它会校验官方 loader 的哈希，绝不将测试 DLL 安装到真实游戏目录中。测试结果位于 `build/vrless-smoke/run-*/report.json` 下，包含各用例单独的 JSON、标准输出/错误、宿主日志以及（启用时的）VR 日志，并记录了源码与产物哈希。已检入的执行汇总见 [`evidence/vrless-smoke-20261026.json`](evidence/vrless-smoke-20261026.json)。

## 证据边界

元数据测试桩仅包含游戏/Unity/Cinemachine 的映像与方法外形，未导出任何 Localify 引导或服务符号。测试确认元数据枚举已发生、启动未调用托管/对象方法，且 4 个关键入口点确实由宿主修补并恢复。测试桩中有意未包含的可选游戏元数据不在验收保证范围内。
