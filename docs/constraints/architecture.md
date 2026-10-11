# 架构

本文件分两类规则：约束（MUST）与建议（SHOULD）。每条约束写明违反后果和验证方式。

## 依赖方向

`flutter_shell → bridge/ffi → core → third_party`，单向，不可逆。

## 约束

**A1 core 不依赖上层**
`core/` 不得包含 `bridge/` 或 `flutter_shell/` 的任何头文件。
- 违反后果：依赖循环，核心无法独立测试，也无法替换前端。
- 验证：**编译期强制**。`CMakeLists.txt` 把 core（`mgcore_core`）与 bridge（`mgcore`/`mgcore_static`）拆成两个目标，只有 bridge 目标带 `bridge/ffi` 的 include 路径；core 里一旦 `#include "ffi_api.h"` 之类就直接编译失败。`core/tests/test_bridge_architecture_guards.cc` 的文本扫描作为第二道防线保留（它同时检查 core 只引用 core 自己的头）。

**A2 模块无环**
`transport`、`mavlink`、`firmware` 之间不得形成依赖环。
- 违反后果：编译顺序与测试隔离失效。
- 验证：依赖图检查，纳入门禁。

**A3 持锁路径不阻塞**
持锁路径内禁止阻塞 I/O（串口、socket、文件），禁止调用外部回调或可能重入的对象。
- 违反后果：死锁，或长时间阻塞导致断链检测失效。
- 验证：针对关键路径的测试加代码审查。

**A4 发送回调不在锁内执行**
- 违反后果：回调重入导致死锁（原 KD-22，已闭环）。
- 验证：`core/tests/test_mavlink_protocol.cc` 用重入回调覆盖发送路径——发送中查询
  adapter 状态要拿同一把锁，发送若在锁内就会自锁死。持锁派发路径的发送改用延迟
  帧队列（见 ADR-0005），公开入口用"临时解锁 → 发送 → 回锁"。

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

**A7 不为假设的未来抽抽象**
任何新抽出的模块、接口或层，必须有当前真实的使用方：至少两个独立调用点，或一个
现在已经存在的变更原因。为「将来可能扩展」预留的空接口、只有单一实现的抽象、纯粹
把函数换个位置的转发层，都算过度工程化。
- 违反后果：抽象成本高于收益，调用链变长、调试变难，改一处要读三处——和边界缺失的
  屎山一样难维护。
- 验证：评审时检查使用方数量。公开接口部分已由工具强制：`tools/check_ffi_contract.py`
  以 `bridge/ffi/API_CATALOG.md` 的「已定性的公开导出」表为准，把「无前端绑定、
  无内部能力定性、无测试引用」的导出冻结进 `tools/ffi_unbound_baseline.json`。该基线
  **只允许缩小**——新增未定性（内部能力/被取代）导出、或已覆盖的符号仍留在基线里，都会让门禁失败
  （新增导出必须先绑定 UI、补测试引用，或登记为内部能力；退役的符号从基线删除）。
  存量清单与逐轮处置方式见 `docs/constraints/known-deviations.md` 的 KD-45。

## 建议

**UI 线程上的 FFI 调用 [SHOULD]**
耗时较长的 FFI 调用（任务启动、命令等待、校准）建议使用提交加轮询的形式，避免在 UI 线程同步等待。具体的耗时阈值由各功能自行给出，并在头文件中标注（参考 `ffi-contract.md` 中关于耗时标注的建议）。

**按变更耦合拆分大文件 [SHOULD]**
判断一个文件是否需要拆分，依据是内部代码的变更耦合，而不是文件行数。当同一文件里
存在两坨代码，修改其中一坨几乎从不触碰另一坨、且各自有独立测试时，应拆开；当两部分
必须共享状态、只能成对修改时，保持同文件。拆分一个 god file 之前，先在
`docs/constraints/known-deviations.md` 登记它内部的哪几坨东西是分开演进的。
- 验证：评审时检查拆分理由；拆分提交须跑一次全量 `tools/preflight.sh`。
