# M-GCS 0.0.5 发布说明

发布日期：2026-09-23

## 版本定位

0.0.5 保留完整 Flutter 前端功能，重点收敛客户端目录职责、同步 Core/FFI/Flutter 版本，并修复几处会影响长期运行和维护的状态问题。

## 主要变更

- Flutter controller 统一归入 `app/lib/presentation/controllers/`。
- MgCore FFI、JSON decoder、Demo backend 和 unavailable backend 归入 `app/lib/infrastructure/`。
- `app/lib/application/` 只保留模型、能力接口和前端无关工作流。
- 新增 application/infrastructure/presentation 依赖方向守卫。
- 隐藏的后台轮询页面不再持续采样；舵机、遥控和校准轮询按页面可见性暂停/恢复。
- 修复参数逐项写入 fallback 的异常恢复。
- 修复校准事件回调的锁边界。
- 修复截断 `MAG_CAL_PROGRESS` 报文的长度保护。
- CMake、FFI 和 Flutter 版本统一为 `0.0.5` / `0.0.5+5`。

## 验证

- `flutter analyze`：通过
- `flutter test`：119 项通过
- Native CTest：29/29 通过
- `flutter build macos --debug`：通过

## 分支说明

- `main` / `v0.0.5`：保留完整 Flutter 前端的产品版本。
- `New-main`：从 `v0.0.5` 创建的后端和接口分支，不包含 Flutter 应用。
