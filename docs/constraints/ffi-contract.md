# FFI 契约

`bridge/ffi` 是前端与核心之间唯一的稳定边界。入口头文件为 `bridge/ffi/ffi_api.h`。

## 约束

**F1 成套变更**
修改 `bridge/ffi/ffi_api*.h` 时，同一次提交必须包含：头文件、实现（`ffi_*.cc`）、测试（`core/tests/`）、API 目录（`bridge/ffi/API_CATALOG.md`）、受影响的文档。
- 违反后果：契约与实现脱节，前端基于错误假设开发。
- 验证：提交前检查 diff 中这几类文件是否同时出现，可脚本化。

**F2 不删除已导出的函数**
弃用需要 ABI 大版本，并在说明文档中给出迁移说明。
- 违反后果：已有前端崩溃或调用失败。
- 验证：对比导出符号表，`tools/audit_status.sh` 可辅助。

**F3 字符串所有权**
`char*` 返回值由调用方使用 `mg_free_string` 释放。`mg_error_string` 返回常量，不得释放。
- 违反后果：内存泄漏，或释放非法指针。
- 验证：测试覆盖释放路径；评审检查。

**F4 新函数使用 v2 错误格式**
新函数使用 `{"success":false,"error":{"code":"...","message":"..."}}`。不得新增其他错误格式。
- 违反后果：前端需要处理多种格式，错误处理容易遗漏。
- 验证：合约测试断言 v2 结构。

## 建议

**耗时标注 [SHOULD]**
同步调用的最长耗时建议写在头文件注释中。耗时较长的调用建议同时提供异步形式（见 `architecture.md` 的 UI 线程建议）。

**API 目录 [SHOULD]**
`API_CATALOG.md` 建议由 `tools/audit_status.sh` 生成或核对，避免手工维护。
