# 现状审计

> 本文是当前滚动审计，不再冻结某一次发布。历史快照保存在
> [`docs/audits/`](audits/)。

## 审计范围

- 代码基线：commit `01bd3d3`，叠加 2026-09-29 Windows 连接加固工作树。
- 本文更新时间：2026-09-29。
- 当前分支：`main`。
- 本地门禁来自最近一次 `tools/preflight.sh`；代码提交后未再修改后端或前端实现。
- 本审计不访问远端。CI 是否真正运行需要单独检查 GitHub Actions，不能由本地门禁推断。

历史 v0.0.8 快照：

- [2026-09-29 · v0.0.8](audits/2026-09-29-v0.0.8.md)

## 1. 当前门禁

| 检查 | 当前结果 |
| --- | --- |
| UI token 扫描 | 10 项自检通过，无违规 |
| Flutter 静态分析 | No issues found |
| Flutter 测试 | 5/5 通过 |
| 后端 offline/simulation | 33/33 通过 |
| macOS debug 构建 | 成功 |
| `tools/preflight.sh` | 全部通过 |

当前门禁覆盖任务文档编译、上传和回读模拟，但仍不覆盖真实飞控写任务。

## 2. 可复现统计

FFI 消费情况由以下命令生成，不再手工维护：

```bash
tools/audit_status.sh
```

脚本只读取本地仓库，不访问网络。它统计：

- `ffi_api*.h` 中实际声明为函数的 `mg_*` 导出。
- Flutter Dart 字符串字面量中实际调用的 `mg_*`。
- C++ 测试中实际调用的 `mg_*`。
- 按领域头拆分的前端使用、测试触达和无人区。

完整本地验收：

```bash
tools/preflight.sh
```

## 3. FFI 契约面

当前自动统计：

| 分类 | 数量 |
| --- | ---: |
| FFI 导出 | 281 |
| 前端实际使用 | 45 |
| 测试触达 | 170 |
| 既无前端调用、也无测试触达 | 105 |

| 领域头 | 导出 | 前端在用 | 仅测试 | 两者皆无 |
| --- | ---: | ---: | ---: | ---: |
| configuration | 43 | 5 | 22 | 19 |
| connection | 21 | 6 | 15 | 5 |
| core | 12 | 3 | 11 | 1 |
| environment | 5 | 0 | 3 | 2 |
| firmware | 7 | 3 | 5 | 1 |
| flight | 32 | 0 | 24 | 8 |
| geofence | 16 | 0 | 4 | 12 |
| logging | 22 | 0 | 2 | 20 |
| mission | 40 | 15 | 23 | 16 |
| params | 20 | 6 | 8 | 11 |
| payload | 17 | 0 | 13 | 4 |
| telemetry | 46 | 7 | 40 | 6 |

说明：

- 导出数包含新增的任务规划 FFI，因此不再等于 v0.0.8 快照中的 266。
- “无人区”不等于可删除代码。专用公开面仍需按 `API_CATALOG.md` 的稳定性规则处理。
- 完整 105 个接口清单可由 `tools/audit_status.sh` 重新生成。

## 4. 当前问题登记

### ARCH-001：core 逆向依赖 bridge

- 状态：`OPEN`
- 实测：`core/include/core/vehicle/vehicle.h` 直接 include `bridge/ffi/telemetry_cache.h`。
- 影响：违反 `core → third_party`、`bridge → core` 的单向依赖约束。
- 原因：`mgcore` 把 `bridge/ffi` 加到了 PUBLIC include 路径。
- 修复方式：把 `TelemetryCache` 下沉到 core，或为 Vehicle 定义 core 侧缓存接口，并收紧 CMake include 可见性。
- 回归保护：给 `test_bridge_architecture_guards.cc` 增加 core→bridge 方向断言。

### CONTRACT-001：前端参数写入入口未进入 Catalog

- 状态：`OPEN`
- 实测：`mg_config_read_param`、`mg_config_write_param`、`mg_config_write_params` 已声明和实现，但未出现在 `API_CATALOG.md`。
- 当前 Catalog 的 Preferred 参数写入入口仍是 `mg_params_set`。
- 影响：Flutter 实际使用路径与契约目录不一致，违反铁律三。
- 处理：统一到 `mg_params_set`，或把 `mg_config_*` 正式登记为首选入口并说明差异。

### UI-001：聚合诊断接口未接 Flutter

- 状态：`OPEN`
- 未接入口：
  - `mg_connection_get_diagnostics`
  - `mg_telemetry_get_dashboard_snapshot`
  - `mg_system_get_diagnostics`
- 当前 Flutter 自行拼接 `mg_connection_get_session_state`、`mg_telemetry_get_freshness` 和状态接口。
- 影响：重复聚合逻辑，且容易遗漏 freshness、last-error 和会话一致性。

### UI-002：长任务没有取消入口

- 状态：`OPEN`
- 未接入口：`mg_operations_get_snapshot`、`mg_operations_cancel`。
- 影响：参数同步和任务上传只能等待，不能从统一状态视图取消。

### TEST-001：5 个前端在用接口无测试

- 状态：`OPEN`
- 接口：
  - `mg_calibration_cancel`
  - `mg_params_get_by_name`
  - `mg_serial_get_uart_names`
  - `mg_stream_set_interval`
  - `mg_vehicle_request_firmware_version`
- 这些是真实 UI 路径已触达、自动化回归未覆盖的组合，应优先补契约测试。

### TEST-002：安全敏感飞行指令无测试

- 状态：`OPEN`
- 接口：`mg_command_set_mode`、`mg_command_land`、`mg_command_rtl`、`mg_command_guided_point`，以及起飞高度和 loiter 半径读写。
- 处理：先用模拟飞控验证命令编码和 ACK 语义，再进行人工真机验收。

### TEST-003：批量参数变更集无验证

- 状态：`OPEN`
- 接口：`mg_params_prepare_change_set`、`mg_params_commit_change_set`。
- 风险点：去重、未知参数、未变化参数、逐项失败和 `_ENABLE` 最后写入。
- 影响：这是写参数路径中最容易造成部分应用状态的接口组。

### DEPRECATION-001：兼容接口没有退役状态

- 状态：`OPEN`
- FFI 中 deprecated 标记仍为 0。
- 兼容别名、空壳 `mg_rc_calibrate_save` 和返回空列表的 `mg_log_list` 没有明确退役计划。
- 处理：建立 deprecated 标记和替代关系，不直接删除公开符号。

### CI-001：远端 CI 运行状态未在本地审计确认

- 状态：`UNVERIFIED`
- 原因：本审计不访问远端。
- 已知历史：v0.0.8 时 CI 因私有仓库计费被拦截，作业从未真正运行。
- 完成标准：在 GitHub Actions 页面确认 4 个 job 实际执行，而不是只看 workflow 是否触发。

## 5. 已解决或已变化的事项

以下是 v0.0.8 快照之后确认发生变化的项目：

| 项目 | 当前状态 |
| --- | --- |
| 后端门禁 | 31 → 33 个测试 |
| Flutter 测试 | 3 → 5 个 |
| UI token 迁移 | 已完成业务代码收敛，并加入自动检查 |
| 协作脚本 | `preflight.sh`、`install-hooks.sh`、`release.sh` 已实现 |
| pre-commit | 校验暂存快照，拒绝未暂存和相关未跟踪文件 |
| CMake/CTest | 最低版本 3.20，空测试会失败 |
| 任务规划 | 新增 MissionDocument、命令目录、校验、编译、估算、上传和回读 |
| Flutter 桥接 | 新增任务模型、后端接口和 `missionPlanning` 控制器 |
| Windows 串口 | 修复 ANSI/UTF-8 越界写、补齐 DCB/清理/部分写处理、加强读线程退出 |
| Windows 飞控发现 | USB 遥测电台 `SiK Radio` 现在也会标记为飞控候选 |
| Windows 打包 | Flutter Windows 构建显式继承生成器/架构，安装阶段校验 DLL 和数据文件 |
| Windows CI | 新增 `windows-shell` job，构建并上传 Windows debug 产物 |

## 6. 建议顺序

1. 修正 `ARCH-001` 的 core→bridge 逆向依赖。
2. 解决 `CONTRACT-001`，统一参数写入入口。
3. 补 `TEST-001` 的 5 个真实 UI 接口测试。
4. 接入 `UI-001` / `UI-002` 聚合诊断和长任务取消。
5. 补 `TEST-002` / `TEST-003` 安全敏感路径测试。
6. 启动 `DEPRECATION-001`，让兼容接口进入明确退役流程。
7. 远端 CI 恢复后，以一次真实运行关闭 `CI-001`。

## 7. 维护规则

1. 当前事实写在本文；历史结论写入 `docs/audits/`。
2. 统计数字必须能由 `tools/audit_status.sh` 复现。
3. 问题必须有稳定 ID，并使用 `OPEN` / `RESOLVED` / `STALE` / `ACCEPTED-RISK` 状态。
4. 已解决问题保留一行状态和验证提交，不删除历史审计。
5. 本地门禁与远端 CI 分开记录，不以本地绿色代替远端运行。
