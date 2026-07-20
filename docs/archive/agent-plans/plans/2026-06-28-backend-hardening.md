# Backend Hardening Implementation Plan

> 归档说明：这是历史临时执行计划，不属于当前开发基线。当前实现状态以 `docs/PROJECT_STATUS.md`、`docs/DEVELOPMENT.md` 和 `docs/ARCHITECTURE.md` 为准。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add industrial-grade backend diagnostics, operation visibility, and frontend-friendly telemetry snapshots without another architecture rewrite.

**Architecture:** Keep the existing C++ core + C FFI split. Add high-level FFI contracts that aggregate existing connection, telemetry, params, mission, and sensor state, while preserving the lower-level APIs for detailed consumers.

**Tech Stack:** C++17, C FFI, cJSON, existing JsonBuilder, CMake, existing offline/simulation tests.

---

### Task 1: Public FFI Contract

**Files:**
- Modify: `bridge/ffi/ffi_api_core.h`
- Modify: `bridge/ffi/ffi_api_connection.h`
- Modify: `bridge/ffi/ffi_api_telemetry.h`
- Modify: `core/tests/test_ffi_contract.cc`

- [ ] Add contract tests for `mg_system_get_diagnostics`, `mg_operations_get_snapshot`, `mg_operations_cancel`, `mg_connection_get_diagnostics`, and `mg_telemetry_get_dashboard_snapshot`.
- [ ] Verify the test fails before implementation because the functions do not exist.
- [ ] Add declarations to the domain headers.
- [ ] Implement enough behavior for disconnected/null-core safety.
- [ ] Run `test_ffi_contract`.

### Task 2: Diagnostics Implementation

**Files:**
- Create: `bridge/ffi/ffi_diagnostics.cc`
- Modify: `CMakeLists.txt`

- [ ] Aggregate connection state, active vehicle identity, last error, telemetry freshness, parameter progress, mission progress, and sensor health into `mg_system_get_diagnostics`.
- [ ] Add operation snapshot entries for params and mission workflows.
- [ ] Implement cancellation as a safe no-op envelope when no cancellable operation is active.
- [ ] Run contract and comprehensive FFI tests.

### Task 3: Dashboard Snapshot

**Files:**
- Modify: `bridge/ffi/ffi_diagnostics.cc`
- Modify: `core/tests/test_ffi_contract.cc`

- [ ] Add a frontend-friendly dashboard snapshot with connection, vehicle summary, attitude, GPS, global position, battery, VFR HUD, sensor health, and freshness.
- [ ] Include validity/freshness metadata so the frontend does not infer it field by field.
- [ ] Run telemetry and FFI tests.

### Task 4: Documentation And Verification

**Files:**
- Modify: `docs/ARCHITECTURE.md`
- Modify: `docs/PROJECT_STATUS.md`

- [ ] Document the new high-level backend contracts.
- [ ] Run `cmake --build build --config Release`.
- [ ] Run offline and simulation `ctest` excluding hardware/manual.
- [ ] Record any remaining hardware-only limitations.
