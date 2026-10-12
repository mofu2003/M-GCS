# 第二阶段完成报告：FFI 适配器租约迁移（KD-51）

**日期**：2026-10-12；**状态**：已完成

## 完成内容

第二阶段把 `bridge/ffi` 中原有的 `get_active_mavlink()` 借用访问迁移到
`shared_active_mavlink()` 返回的 `std::shared_ptr<MavlinkAdapter>`，并删除旧访问器。
命令等待 helper 直接持有适配器租约直到发送、ACK 等待或取消结束；FFI 不再调用
`get_active_mavlink()`，也不再通过 `Vehicle::mavlink()` 取得适配器裸指针。

为保证同一次 FFI 操作不会混用活动车辆切换前后的会话，新增 `VehicleSessionScope`，
由 `CHECK_CORE`、`CHECK_CORE_RET`、`CHECK_MAVLINK` 和 `CHECK_CONNECTED` 固定一个会话视图。
视图同时持有 `Vehicle`、`ConnectionManager` 和适配器租约，并捕获目标 system/component
ID 与连接状态。活动车辆以 `MultiVehicleManager::active_vehicle_shared()` 在注册表锁内
解析并获取所有权；显式 `VehicleTargetScope` 安装目标快照，嵌套调用复用该快照，退出时
恢复外层会话。异步命令 worker 捕获提交时的完整快照，不在执行时重新解析活动车辆或适配器。

## 验证

- `cmake --build build -j2`：通过。
- 阶段二定向常规测试：10/10 通过，包括 `test_ffi_contract`、`test_ffi_comprehensive`、
  `test_bridge_vehicle_session`、任务/参数/命令/校准和连接测试。
- `test_bridge_vehicle_session`：187 项断言通过；新增覆盖活动车辆切换、目标 ID 与连接状态
  固定、显式目标嵌套恢复，以及车辆从注册表移除后 vehicle/adapter 租约仍有效。
- ASan 和 TSan 下分别运行阶段二关键测试 6/6 通过：`test_ffi_comprehensive`、
  `test_bridge_vehicle_session`、`test_sim_params`、`test_sim_mission`、
  `test_sim_commands`、`test_transport_links`。
- FFI 合约检查通过：280 个声明与 136 个 Dart 绑定一致；API 变更集检查确认未修改公开
  `ffi_api*.h`；假断言检查通过。
- 静态扫描：`bridge/ffi` 中没有 `get_active_mavlink()`、`get_mavlink()` 或从
  `mavlink_lease` 解出的裸指针调用。
- `git diff --check` 对本阶段修改文件通过。

## 2026-10-12 Core Getter 收尾

随后补齐 core 内 `ConnectionManager::get_mavlink()` 与 `Vehicle::mavlink()` 的删除，
相关调用迁移到 `shared_mavlink()` 租约。此次变更后的 `cmake --build build -j2` 通过；
`test_bridge_vehicle_session`（187 项）、`test_ffi_comprehensive`（143 项）、
`test_sim_calibration`、`test_mavlink_router`（99 项）、`test_transport_links`（53 项）、
`test_in_memory`（23 项）均通过。随后针对本次 getter 收尾重编并运行 ASan/TSan 关键测试
各 6/6，通过且无 sanitizer 报告。

另有一项独立的生命周期风险仍待验证：`Vehicle::wire_telemetry_callbacks_for()` 捕获裸
`ConnectionManager* source`；本次按约定不重构该回调，已登记为 KD-57。

## 验收限制

- ASan 全套执行 40 项时有 39 项通过；唯一失败是 `test_mavlink_protocol` 并发发送组
  “concurrent senders produced frames”（213/214）。该测试在本阶段未修改的 MAVLink 发送
  实现上已有间歇失败记录；本阶段六项关键测试在 ASan 和 TSan 下均通过。全套 sanitizer
  仍保留该既有间歇性测试限制。
- `tools/preflight.sh --backend-only` 被现有文档格式问题阻断：`CHANGELOG.md`、
  `docs/constraints/parity.md`、`docs/constraints/upstream.md`、
  `docs/plans/mgcs-development-plan-2026-10-10.md` 中已有尾随空白或 EOF 空行。本阶段未
  清理这些无关改动；本阶段修改文件的 `git diff --check` 通过。

KD-51 已完成，可从 `known-deviations.md` 的待处理项中关闭。
