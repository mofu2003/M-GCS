# ADR-0001：依赖方向与 FFI 边界

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

状态：已接受
日期：2026-10-09

## 背景

地面站需要同时支持桌面平台、多种链路和多种飞控固件。界面变化快，核心需要长期稳定。

## 决定

- 依赖方向单向：`flutter_shell → bridge/ffi → core → third_party`。
- `bridge/ffi` 是唯一稳定边界，导出 C ABI。所有返回的字符串由调用方释放。
- `core` 不依赖 `bridge` 或前端。

## 后果

- 界面可以替换，不影响核心。
- 新增导出函数的成本较高，必须成套变更（见 `docs/constraints/ffi-contract.md`）。
- 依赖方向的编译期守卫尚未完成。

## 相关

- `docs/constraints/architecture.md`
- `docs/constraints/ffi-contract.md`
