# 托管版 VRify 首次使用配置

本文描述第一阶段兼容布局。当前托管布局与现有安装的显式迁移规范见 [标准配置迁移（布局 v2）](CONFIG_MIGRATION_V2.md)。既有安装不会自动迁移路径。

## 配置选择与兼容性

描述符 ID 保持为 `gaku.vrify`。可选的 `gakuloader.paths` v1 服务为该所有者返回绝对路径格式（UTF-16）的 Configs 与 Logs 根目录。既有的 ABI v1 结构保持不变；当前编译需要配套的 `GakuPaths.h` SDK 头文件，同时仍支持在旧宿主上以旧版布局运行。

配置选择发生在 VR 启动及生成默认配置之前：

1. 优先使用 Loader 服务返回的有效 `Configs/gaku.vrify/config.json`。
2. 既有 `gakumas-vr/config.json` 继续原位读取和保存。
3. 若仅同一安装目录下的 `gakumas-local/localizationConfig.json` 包含已知 VR 字段，则将这些字段单次导入旧版专属路径。原混合文件逐字节保留，绝不复制或修改 Localify 资源与非 VR 设置。
4. 全新安装从编译期嵌入的 Release 版 `defaults.json` 初始化，可用时使用新的 Configs 根目录，否则使用旧版路径。这刻意保留了 Release 模板的启用与立体策略，避免误采用 C++ 默认的安全/禁用状态。

新字段名 `vrDiagnosticsEnabled` 与 `vrNativeOnly` 优先于旧别名 `vrDebugEnabled` 与 `vrProbeOnly`。专属配置是完整的数据源，缺失键不会从陈旧的混合文件中修补。从混合配置中仅导入已知的 VR 白名单字段。对于已知的旧格式，允许缺失 schemaVersion 或使用版本 0/1；格式错误的对象、类型错误的已知字段及未来的未知 schema 均会报错中止，不会替换默认值或回退至更旧数据。

启动包中包含的是 **config.example.json**，而非活动的配置文件。解压与 `install-plugin.ps1` 均不会创建或覆盖活动配置。示例文件仅供参考，修改示例不会更改嵌入的默认值。`-Probe` 参数仅修改示例文件。首次初始化后，可在选定的活动配置中启用 `vrDiagnosticsEnabled` 以输出详细日志。

## 初始化记录与重试

`gakumas-vr/config-layout.json` 是固定的兼容性检测点，用于记录布局方式（`legacy`/`paths-v1`）、操作类型、转换器版本、阶段、源文件与目标文件的 SHA-256 以及目标路径标识。它不是用于读取任意位置的指针。一旦选定了 paths 布局，如果宿主缺失 paths 服务、根目录变更或已初始化的目标文件缺失，将直接禁用 VR，而不会回退读取残留的旧版配置。

在标记文件生成之前，若同一安装目录下已存在已知的 `gakumas-local/Configs/gaku.vrify/config.json`，旧宿主也会拒绝回退至旧版配置。该旧目录标记是刻意保留的过渡期兼容锚点；第一阶段尚未将所有文件完全合并至新分类中。

初始化过程采用独占进程锁以及规划/完成日志（journal）。初始化在提交前进行校验，在同目录下创建唯一的临时文件，刷新写入并在目标不存在时提交，绝不覆盖已有目标文件。原始混合数据予以保留。未完成的提交通过规划的目标哈希识别：内容匹配时补全日志；目标内容发生变化时保留为冲突。发生 I/O 错误时保留规划并在下次启动时重试。若目标仍不存在，且同一支持的初始化操作收到了有效但发生变化的输入，下次启动会安全地重新规划，并将旧规划指纹记录在 `supersededPlans` 中。因此，不相关的 Localify 修改不会永久阻塞未提交的 VR 导入。源类型改变或已接管的目标文件丢失均保留为冲突；不会重复执行同步。

常规菜单保存会返回真实的成功或失败结果，使用唯一的临时文件并仅替换选定的配置文件。初始化失败后拒绝保存。默认初始化状态报告为 `initialized`，真实的混合配置导入报告为 `migrated-mixed`；在完成持久化提交前均不会报告成功。

新的托管布局可将详细诊断日志输出至 `Logs/gaku.vrify`。既有布局保留旧日志目录。`applicationDirectory` 继续用于定位程序目录下的 OpenXR DLL；`logDirectory` 则与之分离。日志保留策略与既有数据迁移属于后续阶段。

## 本地验证

运行原生测试桩启动测试，构建插件，随后执行生产 DLL 生命周期与测试桩安装测试：

```powershell
./tests/test-config-startup.ps1
./build.ps1 -Mode Plugin -Configuration Release
./tests/test-hosted-plugin.ps1
./package-plugin.ps1 -Configuration Release
./tests/test-plugin-install.ps1
```

测试使用生成的精简配置与模拟安装目录树，绝不会启动游戏或迁移真实用户文件。实际的冷启动、头显渲染与输入以及游戏退出仍属于独立的验收工作。本阶段不提供回退至独立版上游的反向迁移，亦不包含部署操作。
