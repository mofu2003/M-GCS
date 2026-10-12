# ConnectionController 装配块迁移指南

**目标**：将 `connection_controller.dart` 从 3760 行降至 ~2500 行  
**方法**：抽取装配逻辑到 `vehicle_session_assembler.dart`  
**验证**：`flutter test` 全量通过（247 通过 / 4 跳过）

---

## 第 1 阶段：替换字段声明（30 分钟）

### 当前代码（connection_controller.dart:454 起）

```dart
class ConnectionController extends ChangeNotifier {
  final NativeCore _nativeCore;
  final SigningController _signingController;
  
  // Backend 层
  late final TelemetryFfiBackend _telemetryBackend;
  late final ParameterFfiBackend _parameterBackend;
  late final CalibrationFfiBackend _calibrationBackend;
  late final FlightCommandFfiBackend _flightCommandBackend;
  late final FleetFfiBackend _fleetBackend;
  late final MissionDocumentFfiBackend _missionBackend;
  late final PayloadFfiBackend _payloadBackend;
  late final FileFfiBackend _fileBackend;
  late final LogFfiBackend _logBackend;
  late final TlogFfiBackend _tlogBackend;
  
  // Controller 层
  late final WaveformController _waveformController;
  late final FleetController _fleetController;
  late final MissionDocumentController _missionController;
  late final PayloadController _payloadController;
  late final FileController _fileController;
  late final LogController _logController;
  late final TlogController _tlogController;
  
  // ... 其余字段
}
```

### 目标代码

```dart
import 'vehicle_session_assembler.dart';

class ConnectionController extends ChangeNotifier {
  final NativeCore _nativeCore;
  final SigningController _signingController;
  late final VehicleSessionAssembler _assembler;
  
  // 只保留连接状态相关的字段
  final Map<int, VehicleSessionSnapshot> _sessionSnapshots = {};
  int? _activeVehicleId;
  
  // ... 其余连接相关字段（_linkSessions、_heartbeatTimer 等）
}
```

### 操作步骤

1. **导入新文件**
   ```dart
   import 'vehicle_session_assembler.dart';
   ```

2. **删除旧字段**（约 20 行）
   - 删除所有 `late final *Backend` 字段
   - 删除所有 `late final *Controller` 字段（除了 `SigningController`）
   
3. **添加装配器字段**
   ```dart
   late final VehicleSessionAssembler _assembler;
   ```

4. **验证编译**
   ```bash
   cd flutter_shell && flutter analyze lib/connection/connection_controller.dart
   # 预期：大量 "Undefined name" 错误（下一步修复）
   ```

---

## 第 2 阶段：修改构造函数（30 分钟）

### 当前代码（构造函数内）

```dart
ConnectionController({...}) : ... {
  _wireTelemetryBackend();
  _wireParameterBackend();
  _wireCalibrationBackend();
  _wireFlightCommandBackend();
  _wireFleetBackend();
  _wireMissionBackend();
  _wirePayloadBackend();
  _wireFileBackend();
  _wireLogBackend();
  _wireTlogBackend();
  _wireWaveformController();
  // ... 其余
}

void _wireTelemetryBackend() {
  _telemetryBackend = TelemetryFfiBackend(_nativeCore);
}
// ... 其余 10 个 _wire* 方法
```

### 目标代码

```dart
ConnectionController({...}) : ... {
  _assembler = VehicleSessionAssembler(
    nativeCore: _nativeCore,
    signing: _signingController,
  );
  // 其余初始化逻辑保持不变
}
```

### 操作步骤

1. **删除所有 `_wire*` 方法调用**（构造函数内）
   
2. **替换为装配器初始化**
   ```dart
   _assembler = VehicleSessionAssembler(
     nativeCore: _nativeCore,
     signing: _signingController,
   );
   ```

3. **删除 `_wire*` 方法定义**（约 130 行）
   - 查找并删除所有以 `void _wireTelemetry` 开头的方法
   - 查找并删除所有以 `void _wireParameter` 开头的方法
   - ... 删除全部 10 个工厂方法

4. **验证编译**
   ```bash
   flutter analyze lib/connection/connection_controller.dart
   # 预期：仍有 getter 引用错误（下一步修复）
   ```

---

## 第 3 阶段：添加转发 Getter（30 分钟）

### 当前代码（Getter 散落在类中）

```dart
TelemetryFfiBackend get telemetry => _telemetryBackend;
ParameterFfiBackend get parameter => _parameterBackend;
CalibrationFfiBackend get calibration => _calibrationBackend;
FlightCommandFfiBackend get flightCommand => _flightCommandBackend;
FleetController get fleet => _fleetController;
MissionDocumentController get mission => _missionController;
PayloadController get payload => _payloadController;
FileController get file => _fileController;
LogController get log => _logController;
TlogController get tlog => _tlogController;
WaveformController get waveform => _waveformController;
```

### 目标代码

```dart
// Backend 层转发
TelemetryFfiBackend get telemetry => _assembler.telemetryBackend;
ParameterFfiBackend get parameter => _assembler.parameterBackend;
CalibrationFfiBackend get calibration => _assembler.calibrationBackend;
FlightCommandFfiBackend get flightCommand => _assembler.flightCommandBackend;

// Controller 层转发
FleetController get fleet => _assembler.fleetController;
MissionDocumentController get mission => _assembler.missionController;
PayloadController get payload => _assembler.payloadController;
FileController get file => _assembler.fileController;
LogController get log => _assembler.logController;
TlogController get tlog => _assembler.tlogController;
WaveformController get waveform => _assembler.waveformController;
```

### 操作步骤

1. **查找所有 Getter**
   ```bash
   grep -n "get telemetry\|get parameter\|get calibration" lib/connection/connection_controller.dart
   ```

2. **逐个替换右值**
   - `_telemetryBackend` → `_assembler.telemetryBackend`
   - `_parameterBackend` → `_assembler.parameterBackend`
   - ... 全部 11 个

3. **验证编译**
   ```bash
   flutter analyze lib/connection/connection_controller.dart
   # 预期：只剩切车逻辑的错误（下一步修复）
   ```

---

## 第 4 阶段：重构切车逻辑（1-2 小时）

### 当前代码（switchToVehicle 内）

```dart
Future<void> switchToVehicle(int vehicleId) async {
  // 保存旧车状态
  if (_activeVehicleId != null) {
    _sessionSnapshots[_activeVehicleId!] = VehicleSessionSnapshot(
      vehicleId: _activeVehicleId!,
      waveformSnapshot: _waveformController.captureSnapshot(),
      // ... 其他状态
    );
  }
  
  // 切换 FFI 焦点
  final result = await _nativeCore.switchFocus(vehicleId);
  if (!result.success) {
    // 错误处理
    return;
  }
  
  // 恢复新车状态或重置
  _activeVehicleId = vehicleId;
  if (_sessionSnapshots.containsKey(vehicleId)) {
    final snapshot = _sessionSnapshots[vehicleId]!;
    _waveformController.applySnapshot(snapshot.waveformSnapshot);
    // ... 恢复其他状态
  } else {
    _waveformController.reset();
    _fleetController.reset();
    _missionController.reset();
    _payloadController.reset();
    _fileController.reset();
    _logController.reset();
    _tlogController.reset();
  }
  
  // 重新启动轮询
  _waveformController.restartPolling();
  _startHeartbeatMonitor();
  notifyListeners();
}
```

### 目标代码

```dart
Future<void> switchToVehicle(int vehicleId) async {
  // 保存旧车状态
  if (_activeVehicleId != null) {
    _sessionSnapshots[_activeVehicleId!] = _assembler.captureSnapshot(_activeVehicleId!);
  }
  
  // 切换 FFI 焦点
  final result = await _nativeCore.switchFocus(vehicleId);
  if (!result.success) {
    return;
  }
  
  // 恢复新车状态或重置
  _activeVehicleId = vehicleId;
  if (_sessionSnapshots.containsKey(vehicleId)) {
    _assembler.applySnapshot(_sessionSnapshots[vehicleId]!);
  } else {
    _assembler.reset();
  }
  
  // 重新启动轮询
  _assembler.onLinkEstablished();
  _startHeartbeatMonitor();
  notifyListeners();
}
```

### 操作步骤

1. **替换快照捕获**
   - 查找 `VehicleSessionSnapshot(vehicleId: ..., waveformSnapshot: ...)`
   - 替换为 `_assembler.captureSnapshot(_activeVehicleId!)`

2. **替换状态恢复**
   - 查找 `_waveformController.applySnapshot(...)` 及其后续 7 行
   - 替换为 `_assembler.applySnapshot(_sessionSnapshots[vehicleId]!)`

3. **替换重置逻辑**
   - 查找所有 `_*Controller.reset()` 调用（约 7 处）
   - 替换为单行 `_assembler.reset()`

4. **替换轮询重启**
   - 查找 `_waveformController.restartPolling()`
   - 替换为 `_assembler.onLinkEstablished()`

5. **验证编译**
   ```bash
   flutter analyze lib/connection/connection_controller.dart
   # 预期：零错误
   ```

---

## 第 5 阶段：修改断连逻辑（30 分钟）

### 当前代码（closeRoute 或 onDisconnected 内）

```dart
void _handleDisconnect() {
  _waveformController.stopPolling();
  _fleetController.stopPolling();
  _fileController.cancelTransfer();
  _logController.cancelDownload();
  _tlogController.stopRecording();
  // ...
  notifyListeners();
}
```

### 目标代码

```dart
void _handleDisconnect() {
  _assembler.onDisconnected();
  notifyListeners();
}
```

### 操作步骤

1. **查找所有停止轮询的位置**
   ```bash
   grep -n "stopPolling\|cancelTransfer\|cancelDownload\|stopRecording" lib/connection/connection_controller.dart
   ```

2. **集中替换**
   - 删除所有 `_*Controller.stop*()` / `_*Controller.cancel*()` 调用
   - 替换为单行 `_assembler.onDisconnected()`

3. **验证编译**
   ```bash
   flutter analyze lib/connection/connection_controller.dart
   ```

---

## 第 6 阶段：修改 dispose（10 分钟）

### 当前代码

```dart
@override
void dispose() {
  _waveformController.dispose();
  _fleetController.dispose();
  _missionController.dispose();
  _payloadController.dispose();
  _fileController.dispose();
  _logController.dispose();
  _tlogController.dispose();
  _signingController.dispose();
  // ... 其余
  super.dispose();
}
```

### 目标代码

```dart
@override
void dispose() {
  _assembler.dispose();
  _signingController.dispose();
  // ... 其余非装配器管理的资源
  super.dispose();
}
```

### 操作步骤

1. **删除子控制器的 dispose 调用**（7 行）
   
2. **添加装配器 dispose**
   ```dart
   _assembler.dispose();
   ```

3. **保留其他资源的 dispose**
   - `_signingController.dispose()` 保留
   - `_heartbeatTimer?.cancel()` 保留
   - 其余连接相关资源保留

---

## 第 7 阶段：更新测试（1 小时）

### 需要修改的测试文件

```bash
flutter_shell/test/connection_controller_test.dart       # 如果存在
flutter_shell/test/fleet_controller_test.dart           # 已有
flutter_shell/test/mission_document_controller_test.dart # 已有
flutter_shell/test/architecture_ffi_boundary_test.dart   # 白名单
```

### 测试修改点

#### 1. 白名单更新（architecture_ffi_boundary_test.dart）

```dart
const _ffiAllowlist = <String>{
  'connection/native_core.dart',
  'connection/vehicle_session_assembler.dart',  // 新增
  'connection/calibration_ffi_backend.dart',
  // ... 其余
};
```

#### 2. Mock 创建（如果测试直接创建 ConnectionController）

```dart
// 旧代码
final controller = ConnectionController(nativeCore: mockCore, ...);

// 新代码（不需要改，因为装配器在构造函数内部创建）
final controller = ConnectionController(nativeCore: mockCore, ...);
```

### 操作步骤

1. **更新 FFI 白名单**
   ```bash
   vi test/architecture_ffi_boundary_test.dart
   # 添加 'connection/vehicle_session_assembler.dart'
   ```

2. **运行测试**
   ```bash
   flutter test
   # 预期：247 通过 / 4 跳过
   ```

3. **如果测试失败**
   - 检查是否有测试直接访问 `_telemetryBackend` 等私有字段
   - 改为通过公开 getter 访问：`controller.telemetry`

---

## 第 8 阶段：验证与清理（30 分钟）

### 完整验证清单

```bash
# 1. 静态分析
cd flutter_shell
flutter analyze

# 2. 单元测试
flutter test

# 3. 行数统计
wc -l lib/connection/connection_controller.dart
# 预期：2500 左右（从 3760 降低）

wc -l lib/connection/vehicle_session_assembler.dart
# 预期：200-250

# 4. 门禁全量
cd ..
bash tools/preflight.sh

# 5. 手动冒烟测试
# - 启动应用
# - 连接飞控
# - 切换多个车辆
# - 断连重连
# - 检查波形、参数、任务等功能正常
```

### 代码清理

1. **删除注释掉的旧代码**
   ```bash
   # 搜索 // 旧代码 或 /* 旧代码 */
   grep -n "// 旧代码\|/\* 旧代码" lib/connection/connection_controller.dart
   ```

2. **格式化代码**
   ```bash
   dart format lib/connection/connection_controller.dart
   dart format lib/connection/vehicle_session_assembler.dart
   ```

3. **更新导出**（如果 connection_controller.dart 被 re-export）
   ```dart
   // lib/connection.dart 或类似的桶文件
   export 'connection/vehicle_session_assembler.dart';  // 新增
   ```

---

## 常见问题

### Q1：装配器应该暴露 backend 还是 controller？

**A**：都暴露。`ConnectionController` 的 getter 转发到装配器，外部调用方无感知。

```dart
// 外部调用（保持不变）
final params = connectionController.parameter.readAll();

// 内部实现（新）
ParameterFfiBackend get parameter => _assembler.parameterBackend;
```

### Q2：某些子系统需要在装配器外管理怎么办？

**A**：保留在 `ConnectionController`。例如：
- `SigningController`：与连接强耦合，不随切车变化
- `NativeCore`：全局单例，不属于会话状态

### Q3：快照需要保存哪些状态？

**A**：**按需扩展**。当前只保存波形，将来可以加：
- 参数脏标记（未提交的修改）
- 任务编辑缓存（未保存的航点）
- 校准进度（如果允许切车后恢复）

原则：只保存"切回来时希望看到的"状态。

### Q4：测试覆盖率会下降吗？

**A**：不会。`VehicleSessionAssembler` 通过 `ConnectionController` 的测试间接覆盖。将来可以为装配器单独写测试。

---

## 提交清单

### Commit 1: 创建装配器
```
feat(connection): 抽取 VehicleSessionAssembler

将子控制器的装配逻辑从 ConnectionController 抽取到独立类。

- 新增 vehicle_session_assembler.dart（250 行）
- 管理 10 个 backend + 7 个 controller 的生命周期
- 提供 captureSnapshot/applySnapshot/reset 钩子

Ref: KD-43（波形抽取手法）、应改2
```

### Commit 2: 重构 ConnectionController
```
refactor(connection): 使用 VehicleSessionAssembler 简化装配逻辑

- 删除 20 个字段声明（backend/controller）
- 删除 10 个 _wire* 工厂方法（~130 行）
- 切车逻辑从 20+ 字段操作简化为 3 个钩子调用
- 行数：3760 → 2500

验证：flutter test 247 通过 / 4 跳过

Ref: 应改2
```

### Commit 3: 更新测试白名单
```
test: 将 vehicle_session_assembler 加入 FFI 白名单

architecture_ffi_boundary_test.dart 新增白名单条目。

Ref: 应改2
```

---

## 成果验证

### 定量指标

| 指标 | 重构前 | 重构后 | 改善 |
|---|---|---|---|
| ConnectionController 行数 | 3760 | ~2500 | -33% |
| 装配字段数 | 20+ | 1 | -95% |
| 切车逻辑行数 | ~30 | ~10 | -67% |
| 工厂方法数 | 10 | 0 | -100% |

### 定性收益

1. **职责单一**：`ConnectionController` 只管连接，装配器只管子系统
2. **易测试**：装配器可独立 mock，不依赖真实连接
3. **易扩展**：新增子系统只改装配器，主控制器无感知
4. **模式对齐**：与 `waveform_controller.dart` 抽取手法一致

---

## 参考资料

- KD-43：波形控制器抽取（已完成，6 个生命周期钩子）
- `docs/constraints/architecture.md` A7：不为假设的未来抽抽象
- Martin Fowler《重构》：Extract Class
