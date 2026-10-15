# 代码盘点

盘点日期：2026-10-14　版本：`1.0.0`

本文是辅助盘点，不是规范、门禁或完整 API 文档。数量可能随代码变化；需要精确结果时重新运行检查脚本。

## 分层

| 层 | 目录 | 主要职责 |
|---|---|---|
| 前端 | `flutter_shell/` | Flutter 页面、控制器和 backend |
| 桥接 | `bridge/ffi/` | 稳定 C ABI 和 Dart FFI 绑定 |
| 核心 | `core/` | 传输、MAVLink、车辆、任务、参数、校准、日志、载荷等状态与语义 |
| 第三方 | `third_party/` | MAVLink 生成头和其他外部源码；禁止修改 |

依赖方向：`flutter_shell → bridge/ffi → core → third_party`。

## 主要入口

### 前端

- `flutter_shell/lib/connection/connection_controller.dart`：连接、遥测、参数、校准和飞行操作的主要协调器；
- `flutter_shell/lib/connection/vehicle_session_assembler.dart`：车辆会话范围内的无状态 backend 装配；
- `flutter_shell/lib/` 下的各 backend：Flutter 与 FFI 的边界；
- `flutter_shell/lib/main.dart`：主页面和导航入口。

### FFI

- `bridge/ffi/ffi_api*.h`：公开 C ABI；
- `bridge/ffi/ffi_*.cc`：ABI 实现；
- `bridge/ffi/API_CATALOG.md`：接口目录；
- `tools/check_ffi_contract.py`：声明和 Dart 绑定一致性检查。

### Core

主要模块包括 `transport`、`mavlink`、`vehicle`、`mission`、`calibration`、`firmware`、`telemetry`、`logging`、`ftp`、`video` 和 `platform`。模块边界以当前头文件、构建目标和测试为准。

## FFI 当前统计

最近一次基线检查结果为：`154 declarations and 127 Dart bindings agree`，未绑定/未测试导出债务为 `0`。这是检查结果，不是永久文档数字；改动 ABI 后重新运行：

```bash
python3 tools/check_ffi_contract.py
```

## 测试与验证入口

```bash
flutter analyze
flutter test
ctest --test-dir build_check --output-on-failure
tools/preflight.sh --changed
```

默认 CTest 数量和构建目录会变化，不在此固定记录。硬件、SITL 和手工测试需要按环境单独运行。

## 进一步查找

- 当前协作规则：`AGENTS.md`；
- 当前能力概览：`docs/capabilities.md`；
- FFI 签名：`bridge/ffi/API_CATALOG.md`；
- 历史架构决策：`docs/adr/`；
- 历史审计和重构记录：`docs/reports/`、`docs/refactoring/`。
