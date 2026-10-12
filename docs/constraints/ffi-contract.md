# FFI 契约

`bridge/ffi` 是前端与核心之间唯一的稳定边界。入口头文件为 `bridge/ffi/ffi_api.h`。

## 约束

**F1 成套变更**
修改 `bridge/ffi/ffi_api*.h` 时，同一次提交必须包含：头文件、实现（`ffi_*.cc`）、测试（`core/tests/`）、API 目录（`bridge/ffi/API_CATALOG.md`）、受影响的文档。
- 违反后果：契约与实现脱节，前端基于错误假设开发。
- 验证：`python3 tools/check_api_change_set.py`（默认对比工作区与 `HEAD`，可加 `--base <ref>`）。改动集里出现 `ffi_api*.h` 时就要求另外四类同时出现，缺一类即失败；`--self-test` 用合成改动集验证判定逻辑。脚本只能看见"文件是否出现过"，文档内容是否真的同步仍靠评审。

**F2 无使用方的导出应删除，不得为「将来可能用」保留**

公开 ABI 只承载**当前有真实使用方**的能力。一个导出只要满足下列任一条，就是有使用方，
必须保留：被 Flutter 绑定；被 bridge 内部调用（如 `mg_command_submit_for_vehicle` 的
worker 调用各 `mg_command_*`）；被 core/tests 或 tests 引用；被打包校验脚本要求
（`tools/ffi_bundle_symbols.dart`）。**四条都不满足的导出必须删除**——它没有前端入口、
没有内部调用方、没有测试引用，留着只会增加 ABI 表面积与维护成本。

不允许的分类借口：「将来可能要用」「保持架构扩展性」「预留低延迟备选」。
`architecture.md` 的 A7 已经把「为假设的未来抽抽象」定为过度工程化；对外导出一视同仁。
确实计划在后续版本交付、且**当前已有原生实现与明确验收标准**的能力，可以登记进
`DEFERRED_SYMBOL_NOTES` 保留，但必须有版本归属，不得无限期挂着。

不承诺兼容旧版应用或仓库外二进制调用方。删除时：
- 迁移或删除仓库内**全部**调用点（F1 保证头文件/实现/测试/目录/文档成套变化）；
- 被删符号登记进 `tools/check_ffi_contract.py` 的 `RETIRED_SYMBOLS`，重新引入即门禁失败；
- 功能级删除（如整个 PX4 / 测绘 / 围栏模块）要在 `CHANGELOG.md` 说明替代路径或「不再提供」。

- 违反后果：ABI 表面积无谓膨胀（该删的没删）；或误删仍有使用方的能力（当前应用或测试
  在构建/运行时找不到符号）。
- 验证：`tools/check_ffi_contract.py` 以「无前端绑定、无分类、无测试引用」为准维护基线，
  基线**只允许缩小**；`tools/ffi_unbound_baseline.json` 为空表示没有未定性债务；
  带 `--library` 时逐个 `dlsym` 核对编译库导出与公开头一致。

**F3 字符串所有权**
`char*` 返回值由调用方使用 `mg_free_string` 释放。`mg_error_string` 返回常量，不得释放。
- 违反后果：内存泄漏，或释放非法指针。
- 验证：测试覆盖释放路径（`core/tests/test_ffi_contract.cc` 的 "F3 mg_error_string ownership" 分组断言两次调用返回同一常量地址，并核对 `ffi_error_name` 的符号名）；评审检查新增返回值的所有权注释。

**F4 结构化 JSON 使用统一响应格式**
任何 `success:false` 的结构化响应必须使用同一个错误对象：

```json
{"success":false,"error":{"code":-6,"name":"TIMEOUT","message":"command ack timeout"}}
```

`code` 是对应 `mg_error_t` 的数值，`name` 是稳定符号名，`message` 是可直接展示的文本。
领域附加字段（`revision`、`result`、`result_str`、`diagnostics` 等）作为同一层的兄弟字段保留。
响应根级的 `error` 字段只用于该错误对象；嵌套领域对象（operation、mode history、diagnostics 条目等）的原因字段由各自的 schema 定义。

没有 `success` 字段的查询结果是数据，不是失败响应：`valid`、`found` 之类的领域判定，以及 operation 对象里的原因文本，都按各自的数据契约解析，不受本条约束。
- 违反后果：前端需要处理多种格式，错误处理容易遗漏。
- 验证：合约测试断言统一结构；前端只通过 `flutter_shell/lib/ffi_response.dart` 读取错误。

## 建议

**耗时标注 [SHOULD]**
同步调用的最长耗时建议写在头文件注释中。耗时较长的调用建议同时提供异步形式（见 `architecture.md` 的 UI 线程建议）。

**API 目录 [SHOULD]**
`API_CATALOG.md` 建议由 `tools/audit_status.sh` 生成或核对，避免手工维护。
