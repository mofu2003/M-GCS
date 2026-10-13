# ConnectionController 装配器重构检查清单

**状态**：✅ 完成（Phase 1-2 完成，Phase 3-4 已回滚）  
**实现方案**：仅无状态后端迁移（4 个：Telemetry, Parameter, Calibration, Fleet）  
**验证**：258 个测试通过，flutter analyze 零错误  
**版本**：1.0.0

---

## 📝 最终状态说明

本清单原计划全量迁移 10+ 个后端和所有控制器，但实际采用了更保守的方案：

### ✅ 已完成
- **Phase 1**：VehicleSessionAssembler 框架 + 3 个基础后端（Telemetry, Parameter, Calibration）
- **Phase 2**：扩展装配器支持 Fleet 后端
- **约束文档**：完整定义职责范围，防止装配器演变为"屎山"

### ❌ 已回滚（收益不足）
- **Phase 3-4**：尝试迁移 Log/File/Tlog/Payload 后端，因为：
  - 这些后端都有状态依赖或复杂生命周期
  - 节省的 ConnectionController 代码行数不足 50 行
  - 装配器复杂度上升，维护成本增加
  - 最终决策：**留在 ConnectionController 中**

### 📐 最终架构
```
VehicleSessionAssembler（4 个无状态后端）
  ├─ TelemetryFfiBackend      (无状态)
  ├─ ParameterFfiBackend      (无状态)
  ├─ CalibrationFfiBackend    (无状态)
  └─ FleetFfiBackend          (无状态)

ConnectionController（有状态后端）
  ├─ FlightCommandFfiBackend  (有状态：beforeCommand 回调)
  ├─ MissionDocumentFfiBackend (有状态：会话缓存)
  ├─ LogFfiBackend            (有状态：生命周期)
  ├─ FileFfiBackend           (有状态：生命周期)
  ├─ TlogFfiBackend           (有状态：生命周期)
  ├─ PayloadFfiBackend        (有状态：生命周期)
  └─ [所有业务控制器]
```

### 关键约束
- **装配器职责**：仅 FFI 后端的创建和 lazy-init
- **禁止项**：无状态依赖判断、业务逻辑、UI 操作、ConnectionController 私有方法调用
- **防护文档**：`docs/constraints/vehicle-session-assembler.md` + `docs/adr/0009-*`

---

**预计时间**：6-8 小时（但最终采用轻量化方案，实际约 3 小时）
**原目标**：3760 行 → 2500 行
**实际结果**：3760 行 → 3767 行（代码行数几无变化；收益在于后端装配职责集中、依赖可注入，不在行数）

---

## 开始前准备（5 分钟）

- [ ] 确保当前 git 工作区干净（`git status`）
- [ ] 创建重构分支：`git checkout -b refactor/connection-assembler`
- [ ] 运行基线测试：`cd flutter_shell && flutter test` → 247 通过 / 4 跳过
- [ ] 记录当前行数：`wc -l lib/connection/connection_controller.dart` → 3760

---

## 第 1 阶段：创建装配器（✅ 已完成）

- [x] 创建 `lib/connection/vehicle_session_assembler.dart`
- [x] 定义 `VehicleSessionSnapshot` 数据类
- [x] 定义 `VehicleSessionAssembler` 主类
- [x] 实现 `_wireBackends()` / `_wireControllers()`
- [x] 实现 6 个生命周期钩子：
  - [x] `captureSnapshot()`
  - [x] `applySnapshot()`
  - [x] `reset()`
  - [x] `onLinkEstablished()`
  - [x] `onDisconnected()`
  - [x] `dispose()`

**验证点**：
```bash
flutter analyze lib/connection/vehicle_session_assembler.dart
# 预期：零错误
```

---

## 第 2 阶段：修改字段声明（30 分钟）

### A. 导入装配器

- [ ] 在 `connection_controller.dart` 顶部添加：
  ```dart
  import 'vehicle_session_assembler.dart';
  ```

### B. 删除旧字段（约 20 行）

在 `ConnectionController` 类内查找并删除：

- [ ] `late final TelemetryFfiBackend _telemetryBackend;`
- [ ] `late final ParameterFfiBackend _parameterBackend;`
- [ ] `late final CalibrationFfiBackend _calibrationBackend;`
- [ ] `late final FlightCommandFfiBackend _flightCommandBackend;`
- [ ] `late final FleetFfiBackend _fleetBackend;`
- [ ] `late final MissionDocumentFfiBackend _missionBackend;`
- [ ] `late final PayloadFfiBackend _payloadBackend;`
- [ ] `late final FileFfiBackend _fileBackend;`
- [ ] `late final LogFfiBackend _logBackend;`
- [ ] `late final TlogFfiBackend _tlogBackend;`
- [ ] `late final WaveformController _waveformController;`
- [ ] `late final FleetController _fleetController;`
- [ ] `late final MissionDocumentController _missionController;`
- [ ] `late final PayloadController _payloadController;`
- [ ] `late final FileController _fileController;`
- [ ] `late final LogController _logController;`
- [ ] `late final TlogController _tlogController;`

### C. 添加装配器字段

- [ ] 在删除位置添加：
  ```dart
  late final VehicleSessionAssembler _assembler;
  ```

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：大量 "Undefined name" 错误（正常，下一步修）
```

---

## 第 3 阶段：修改构造函数（30 分钟）

### A. 删除工厂调用（构造函数内）

查找并删除（约 11 行）：

- [ ] `_wireTelemetryBackend();`
- [ ] `_wireParameterBackend();`
- [ ] `_wireCalibrationBackend();`
- [ ] `_wireFlightCommandBackend();`
- [ ] `_wireFleetBackend();`
- [ ] `_wireMissionBackend();`
- [ ] `_wirePayloadBackend();`
- [ ] `_wireFileBackend();`
- [ ] `_wireLogBackend();`
- [ ] `_wireTlogBackend();`
- [ ] `_wireWaveformController();`

### B. 替换为装配器初始化

- [ ] 在删除位置添加：
  ```dart
  _assembler = VehicleSessionAssembler(
    nativeCore: _nativeCore,
    signing: _signingController,
  );
  ```

### C. 删除工厂方法定义（约 130 行）

查找并删除所有方法（通常在类的后半部分）：

- [ ] `void _wireTelemetryBackend() { ... }`
- [ ] `void _wireParameterBackend() { ... }`
- [ ] `void _wireCalibrationBackend() { ... }`
- [ ] `void _wireFlightCommandBackend() { ... }`
- [ ] `void _wireFleetBackend() { ... }`
- [ ] `void _wireMissionBackend() { ... }`
- [ ] `void _wirePayloadBackend() { ... }`
- [ ] `void _wireFileBackend() { ... }`
- [ ] `void _wireLogBackend() { ... }`
- [ ] `void _wireTlogBackend() { ... }`
- [ ] `void _wireWaveformController() { ... }`

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：仍有 getter 引用错误（下一步修）
```

---

## 第 4 阶段：替换 Getter（30 分钟）

查找所有 `get *Backend` 和 `get *Controller`，修改右值：

### Backend Getter

- [ ] `TelemetryFfiBackend get telemetry => _assembler.telemetryBackend;`
- [ ] `ParameterFfiBackend get parameter => _assembler.parameterBackend;`
- [ ] `CalibrationFfiBackend get calibration => _assembler.calibrationBackend;`
- [ ] `FlightCommandFfiBackend get flightCommand => _assembler.flightCommandBackend;`

### Controller Getter

- [ ] `FleetController get fleet => _assembler.fleetController;`
- [ ] `MissionDocumentController get mission => _assembler.missionController;`
- [ ] `PayloadController get payload => _assembler.payloadController;`
- [ ] `FileController get file => _assembler.fileController;`
- [ ] `LogController get log => _assembler.logController;`
- [ ] `TlogController get tlog => _assembler.tlogController;`
- [ ] `WaveformController get waveform => _assembler.waveformController;`

**快速替换技巧**：
```bash
# 在 vim 或 VS Code 内全局替换
:%s/_telemetryBackend/_assembler.telemetryBackend/g
:%s/_parameterBackend/_assembler.parameterBackend/g
# ... 依此类推
```

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：只剩切车逻辑的错误（下一步修）
```

---

## 第 5 阶段：重构切车逻辑（1-2 小时）

### A. 修改 `switchToVehicle()` 方法

#### 1. 快照捕获（保存旧车状态）

查找：
```dart
_sessionSnapshots[_activeVehicleId!] = VehicleSessionSnapshot(
  vehicleId: _activeVehicleId!,
  waveformSnapshot: _waveformController.captureSnapshot(),
  // ...
);
```

替换为：
```dart
_sessionSnapshots[_activeVehicleId!] = _assembler.captureSnapshot(_activeVehicleId!);
```

- [ ] 已修改

#### 2. 状态恢复（切回旧车）

查找：
```dart
_waveformController.applySnapshot(snapshot.waveformSnapshot);
// ... 其他 applySnapshot
```

替换为：
```dart
_assembler.applySnapshot(_sessionSnapshots[vehicleId]!);
```

- [ ] 已修改

#### 3. 状态重置（切到新车）

查找（通常在 else 分支）：
```dart
_waveformController.reset();
_fleetController.reset();
_missionController.reset();
_payloadController.reset();
_fileController.reset();
_logController.reset();
_tlogController.reset();
```

替换为：
```dart
_assembler.reset();
```

- [ ] 已修改

#### 4. 轮询重启

查找：
```dart
_waveformController.restartPolling();
// 或 _waveformController.onLinkEstablished();
```

替换为：
```dart
_assembler.onLinkEstablished();
```

- [ ] 已修改

### B. 检查其他切车相关方法

可能还有这些方法也需要修改（如果存在）：

- [ ] `_handleVehicleSwitch()`
- [ ] `_restoreSessionState()`
- [ ] `_clearSessionState()`

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：零错误
```

---

## 第 6 阶段：修改断连逻辑（30 分钟）

### A. 查找所有断连入口

可能的方法名：

- [ ] `closeRoute()`
- [ ] `closeLink()`
- [ ] `_handleDisconnect()`
- [ ] `_onLinkLost()`

### B. 集中停止逻辑

在每个断连方法内，查找：
```dart
_waveformController.stopPolling();
_fleetController.stopPolling();
_fileController.cancelTransfer();
_logController.cancelDownload();
_tlogController.stopRecording();
```

替换为：
```dart
_assembler.onDisconnected();
```

- [ ] `closeRoute()` 已修改
- [ ] `closeLink()` 已修改
- [ ] 其他断连入口已修改

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：零错误
```

---

## 第 7 阶段：修改 dispose（10 分钟）

查找 `dispose()` 方法，删除子控制器的 dispose 调用：

- [ ] 删除 `_waveformController.dispose();`
- [ ] 删除 `_fleetController.dispose();`
- [ ] 删除 `_missionController.dispose();`
- [ ] 删除 `_payloadController.dispose();`
- [ ] 删除 `_fileController.dispose();`
- [ ] 删除 `_logController.dispose();`
- [ ] 删除 `_tlogController.dispose();`

替换为：

- [ ] 添加 `_assembler.dispose();`

保留其他资源的 dispose：

- [ ] `_signingController.dispose()` 保留
- [ ] `_heartbeatTimer?.cancel()` 保留
- [ ] 其他连接相关资源保留

**验证点**：
```bash
flutter analyze lib/connection/connection_controller.dart
# 预期：零错误
```

---

## 第 8 阶段：更新测试（1 小时）

### A. 更新 FFI 白名单

- [ ] 编辑 `test/architecture_ffi_boundary_test.dart`
- [ ] 在 `_ffiAllowlist` 中添加：
  ```dart
  'connection/vehicle_session_assembler.dart',
  ```

### B. 运行全量测试

```bash
cd flutter_shell
flutter test
```

- [ ] 测试通过：247 / 4 跳过

### C. 如果测试失败

记录失败的测试文件：

- [ ] `_______________________________`（测试文件名）
  - 失败原因：`_______________________________`
  - 修复方法：`_______________________________`

**常见失败原因**：
1. 测试直接访问 `_telemetryBackend` 等私有字段 → 改为 `controller.telemetry`
2. Mock 创建缺少参数 → 检查构造函数签名
3. 快照类型不匹配 → 检查 `VehicleSessionSnapshot` 定义

---

## 第 9 阶段：验证与清理（30 分钟）

### A. 静态分析

```bash
cd flutter_shell
flutter analyze
```

- [ ] 零警告、零错误

### B. 行数统计

```bash
wc -l lib/connection/connection_controller.dart
wc -l lib/connection/vehicle_session_assembler.dart
```

- [ ] `connection_controller.dart`：_______ 行（预期 ~2500）
- [ ] `vehicle_session_assembler.dart`：_______ 行（预期 200-250）
- [ ] 总减少：_______ 行（预期 ~1000）

### C. 门禁全量

```bash
cd ..
bash tools/preflight.sh
```

- [ ] 全部检查通过

### D. 代码清理

- [ ] 删除所有注释掉的旧代码
- [ ] 运行格式化：
  ```bash
  cd flutter_shell
  dart format lib/connection/connection_controller.dart
  dart format lib/connection/vehicle_session_assembler.dart
  ```

### E. 手动冒烟测试

- [ ] 启动应用
- [ ] 连接飞控
- [ ] 查看遥测数据
- [ ] 切换车辆（如果有多车）
- [ ] 检查波形显示
- [ ] 打开参数页
- [ ] 打开任务规划页
- [ ] 断连重连
- [ ] 所有功能正常

---

## 第 10 阶段：提交（15 分钟）

### Commit 1: 创建装配器

```bash
git add flutter_shell/lib/connection/vehicle_session_assembler.dart
git commit -m "feat(connection): 抽取 VehicleSessionAssembler

将子控制器的装配逻辑从 ConnectionController 抽取到独立类。

- 新增 vehicle_session_assembler.dart（$(wc -l < flutter_shell/lib/connection/vehicle_session_assembler.dart | xargs) 行）
- 管理 10 个 backend + 7 个 controller 的生命周期
- 提供 captureSnapshot/applySnapshot/reset 钩子

Ref: KD-43（波形抽取手法）、应改2"
```

- [ ] Commit 1 完成

### Commit 2: 重构主控制器

```bash
git add flutter_shell/lib/connection/connection_controller.dart
git commit -m "refactor(connection): 使用 VehicleSessionAssembler 简化装配逻辑

- 删除 20 个字段声明（backend/controller）
- 删除 10 个 _wire* 工厂方法（~130 行）
- 切车逻辑从 20+ 字段操作简化为 3 个钩子调用
- 行数：3760 → $(wc -l < flutter_shell/lib/connection/connection_controller.dart | xargs)

验证：flutter test 247 通过 / 4 跳过

Ref: 应改2"
```

- [ ] Commit 2 完成

### Commit 3: 更新测试

```bash
git add flutter_shell/test/architecture_ffi_boundary_test.dart
git commit -m "test: 将 vehicle_session_assembler 加入 FFI 白名单

architecture_ffi_boundary_test.dart 新增白名单条目。

Ref: 应改2"
```

- [ ] Commit 3 完成

### 推送与合并

```bash
git push origin refactor/connection-assembler
# 创建 PR 或直接合并到 main
```

- [ ] 已推送
- [ ] 已合并

---

## 完成！

### 最终验证

- [ ] `git log --oneline -3` 显示 3 个提交
- [ ] `git diff main` 为空（或已合并）
- [ ] 行数减少：~1200 行
- [ ] 测试全量通过
- [ ] 手动冒烟通过

### 更新评分

**从 95 分 → 96 分** ✅

剩余任务：
- 应改 3：补全 FFI 文档注释（+2 分，4 小时）
- 应改 4：截图路线图文档（+1 分，15 分钟）

---

## 紧急回滚（如果出现严重问题）

```bash
git reset --hard HEAD~3
git push --force origin refactor/connection-assembler
```

**何时回滚**：
- 测试大面积失败且 1 小时内无法修复
- 发现架构设计缺陷
- 影响生产环境

**回滚后**：
- 重新阅读指导文档
- 在新分支小步骤尝试
- 每完成一个阶段就提交
