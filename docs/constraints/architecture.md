# 架构

本文件分两类规则：约束（MUST）与建议（SHOULD）。每条约束写明违反后果和验证方式。

## 依赖方向

`flutter_shell → bridge/ffi → core → third_party`，单向，不可逆。

## 约束

**A1 core 不依赖上层**
`core/` 不得包含 `bridge/` 或 `flutter_shell/` 的任何头文件。
- 违反后果：依赖循环，核心无法独立测试，也无法替换前端。
- 验证：编译期机制保证，例如公共 include 路径不暴露 `bridge/ffi`。当前仅靠文件名前缀匹配，需要升级（KD-20）。

**A2 模块无环**
`transport`、`mavlink`、`firmware` 之间不得形成依赖环。
- 违反后果：编译顺序与测试隔离失效。
- 验证：依赖图检查，纳入门禁。

**A3 持锁路径不阻塞**
持锁路径内禁止阻塞 I/O（串口、socket、文件），禁止调用外部回调或可能重入的对象。
- 违反后果：死锁，或长时间阻塞导致断链检测失效。
- 验证：针对关键路径的测试加代码审查。

**A4 发送回调不在锁内执行**
- 违反后果：回调重入导致死锁（当前见 KD-22）。
- 验证：测试以重入回调覆盖发送路径。

**A5 dart:ffi 只在 backend 层**
`dart:ffi` 符号只能出现在 backend 层。页面与控制器通过接口访问，不直接持有原生句柄。

仓库里没有字面的 `backend/` 目录，实际约定是「功能目录下的 `*_ffi_backend.dart`」
加上共享原语 `connection/native_core.dart`（唯一持有 `DynamicLibrary` 与 `mg_core_t`
句柄的地方）。除此之外的文件不得导入 `dart:ffi`，也不得出现 `DynamicLibrary`、
`Pointer<`、`toNativeUtf8`。
- 违反后果：无法替换界面或后端。
- 验证：`flutter_shell/test/architecture_ffi_boundary_test.dart` 自动扫描 `lib/`，
  越界即门禁失败；新增 backend 文件时需同步该测试的白名单。

**A6 架构变更写 ADR**
新增线程、新增依赖边、改变线程模型，必须先写 ADR（见 `docs/adr/`）。
- 违反后果：架构决策无记录，后续无法追溯。
- 验证：评审时检查。

## 建议

**UI 线程上的 FFI 调用 [SHOULD]**
耗时较长的 FFI 调用（任务启动、命令等待、校准）建议使用提交加轮询的形式，避免在 UI 线程同步等待。具体的耗时阈值由各功能自行给出，并在头文件中标注（参考 `ffi-contract.md` 中关于耗时标注的建议）。
