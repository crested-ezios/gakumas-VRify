# 标准配置迁移（布局 v2）

本规范取代第一阶段过渡锚点，作为当前的托管布局规范。这是一项**单向**迁移（上游/旧版向托管布局迁移），不会反向同步回独立版上游，且绝不删除旧配置、回执或日志。

## 目标与激活

路径采用绑定到所有者的 Loader paths v1 根目录（所有者为 `gaku.vrify`）：

```text
gakumas-local/
  Configs/gaku.vrify/config.json
  PluginData/gaku.vrify/
    active-layout.json
    migrations/layout-v2.json
    migrations/legacy-config-layout.<source-SHA256>.json
  Logs/gaku.vrify/<session>.log
```

`active-layout.json` 是唯一的活动布局开关。仅复制配置文件不会激活迁移。该文件仅在完成配置复制与校验、源/目标指纹核对以及既有旧版回执备份后写入。新的活动配置与 Logs 根目录必须与当前 Loader 根目录匹配。OpenXR 依赖项的查找仍使用程序可执行文件所在目录。

有效的全新配置拥有最高优先级。否则，专属的 `gakumas-vr/config.json` 优先于 `gakumas-local/localizationConfig.json` 中的已知 VR 字段。专属配置副本保留其原始字节与未知字段；混合配置导入仅复制已知的 VR 白名单字段，并规范化别名（优先采用新名称）。不支持的 schema、格式错误或类型错误的已知字段会直接阻止转换，不会替换为默认值。

既有旧版安装在执行显式的离线应用步骤前保持原样。全新安装和已存在的新配置将直接确立标准状态，不会在旧目录中创建兼容锚点。经过批准且处于挂起状态的迁移可在插件激活期间恢复执行。已完成的标准布局始终选取新配置；绝不将旧残留配置作为回退，即使新目标文件丢失或损坏亦然。当已确立标准状态时，缺少 paths 服务的旧宿主将直接报错中止。

旧的 `gakumas-vr/config-layout.json` 逐字节保留。若可读，还会以基于内容寻址的备份形式复制到 PluginData 下。已有效的全新配置不依赖于读取过时的旧数据；无法访问的旧回执记录为 `not-inspected` 并保持不变。旧日志文件保留在原位；仅后续的托管诊断信息输出至 Logs。本数据迁移不修改 `gakumas-vr` 中的程序示例、版本或许可证文件。

## 只读规划、备份与显式应用

原生迁移工具编译产物位于 `build/tools/Release/vr-config-migration.exe`，打包产物位于 `gakumas-local/tools/gaku.vrify/vr-config-migration.exe`。该工具不加载游戏、插件或 XR 运行时，也不调用 Python。当前支持默认便携根目录（`<GameRoot>/gakumas-local`），与现有的 Loader 布局一致；不会猜测外部自定义路径或扫描其他安装。

在将 `$GameRoot` 设为已确认的安装路径，并将 `$PlanPath` 设为**安装目录外部**未使用的 JSON 文件路径后执行：

```powershell
$tool = './build/tools/Release/vr-config-migration.exe'
& $tool --plan $GameRoot $PlanPath
$planHash = (Get-FileHash -LiteralPath $PlanPath -Algorithm SHA256).Hash
# 在此步骤前检查规划并备份原始配置与回执。
& $tool --apply $PlanPath --expect-sha256 $planHash
```

规划过程只读取已知的固定文件，仅输出指定的规划文件，其中包含路径、格式与哈希，而不包含具体配置值。应用过程要求规划哈希匹配且游戏已停止运行。源数据发生变更需要重新规划；绝不覆盖已存在或竞争的目标文件。审查确认后，新的显式接管规划可直接选定已有效的目标。

## 重启与故障恢复

PluginData 日志记录 `planned`、`staged`、`copied`、`completed` 四个阶段以及获批的规划哈希。临时文件暂存在目标卷上，完成刷新并校验哈希。日志在执行非覆盖重命名之前记录暂存文件的卷 ID 与文件 ID。中断恢复后仅认领相同的文件 ID 与预期哈希；内容相同但不相关的文件予以保留，不会被静默认领。

相同的获批应用命令可恢复执行未发生变更的规划。在激活之前，更新且经核验的规划可取代尚未提交的规划；旧元数据保存在 `supersededPlans` 中。源文件与回执原件全程保持不变。若活动开关在最终日志完成前已提交，标准配置已具权威性，重新应用即可补全剩余日志。已完成的日志可直接根据有效的新目标重建丢失的活动指针，而无需查询旧数据。已提交的目标若丢失或损坏将报错中止，需从用户备份中恢复新配置。

失败或中断的暂存可能会在目标目录中留下自身拥有的临时文件。除非能证实归属，否则这些文件予以保留；该工具绝不删除未知的临时文件或竞争目标以强行推进。

安装器的 `Restore.ps1` 用于恢复**程序安装包文件**（包括工具与示例），并保留活动配置、PluginData 状态以及所有旧原件。该操作不是反向数据迁移。在新布局使用之后，请勿将保留的旧数据视作已同步的配置。

## 为独立版选取配置

原始独立版 VRify 代理读取的是 `gakumas-vr/config.json`，不会读取 `Configs/gaku.vrify/config.json` 或 Loader 的活动布局记录。若要在切换回独立版时保留托管激活后修改的配置，请先退出游戏，备份这两份文件，在替换代理前显式将选定的托管 VR 配置复制到旧路径。请保留托管配置与 PluginData 记录；独立版保存配置不会更新它们。这种手动文件选取不需要反向迁移引擎或 Loader 服务，完整流程见 [从插件返回独立版](../BUILDING.md#returning-from-plugin-to-standalone)。

## 验证边界

测试桩用例在持久化阶段注入了中断、源/目标竞争、相同哈希竞争文件、目标缺失、schema/类型错误、锁与 I/O 失败以及长 Unicode/含空格路径。生产工具与 DLL 也在独立的模拟进程中运行。没有测试会启动真实游戏或迁移真实用户文件。在应用核验的规划前，请务必备份目标安装目录。模拟检查不代表游戏启动、头显渲染或控制器行为已通过验证。
