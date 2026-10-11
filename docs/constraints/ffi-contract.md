# FFI 契约

`bridge/ffi` 是前端与核心之间唯一的稳定边界。入口头文件为 `bridge/ffi/ffi_api.h`。

## 约束

**F1 成套变更**
修改 `bridge/ffi/ffi_api*.h` 时，同一次提交必须包含：头文件、实现（`ffi_*.cc`）、测试（`core/tests/`）、API 目录（`bridge/ffi/API_CATALOG.md`）、受影响的文档。
- 违反后果：契约与实现脱节，前端基于错误假设开发。
- 验证：提交前检查 diff 中这几类文件是否同时出现，可脚本化。

**F2 仓库内统一迁移后允许删除重复导出**
不承诺兼容旧版应用或仓库外二进制调用方。删除重复或错误命名的导出前，必须迁移仓库内所有调用点，并验证头文件、Flutter 绑定、测试和编译库导出一致；独立能力不得因未被 Flutter 调用而删除。
- 违反后果：当前应用或测试在构建/运行时找不到符号，或公开能力被误删。
- 验证：`tools/check_ffi_contract.py` 校验 catalog 与编译库导出；FFI smoke 和相关 core 测试覆盖调用路径。

**F3 字符串所有权**
`char*` 返回值由调用方使用 `mg_free_string` 释放。`mg_error_string` 返回常量，不得释放。
- 违反后果：内存泄漏，或释放非法指针。
- 验证：测试覆盖释放路径；评审检查。

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
