# Status Panel Segmented Pages Implementation Plan

> 归档说明：这是历史临时执行计划，不属于当前开发基线。文中涉及的 Flutter 路径和测试命令来自当时的前端上下文，不能作为当前验证依据。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add segmented secondary pages to the top-level status detail panels while keeping existing top-level animation and routing behavior stable.

**Architecture:** Reuse the existing connection panel segmented control by extracting it into a shared widget, then render segmented content inside `StatusBarBootstrap` only for GPS, battery, and alert panels. Keep flight mode as a single-page panel and increase all status detail panel heights by one status-label height.

**Tech Stack:** Flutter, existing motion tokens/widgets, widget tests, flutter analyze, flutter build windows

---

### Task 1: Extract shared segmented control

**Files:**
- Create: `app/lib/design_system/segmented_control.dart`
- Modify: `app/lib/features/connection/connection_panel.dart`
- Test: `app/test/features/connection/connection_panel_test.dart`

- [ ] Add a failing widget test that still finds the connection panel tabs after the control is extracted.
- [ ] Run `flutter test test/features/connection/connection_panel_test.dart` and verify the new test fails for the expected missing widget/import reason.
- [ ] Add the shared segmented control with the same visual behavior as the current connection panel private implementation.
- [ ] Replace `_SegmentedTabs` usage in `connection_panel.dart` with the shared control and remove the duplicated private widget.
- [ ] Run `flutter test test/features/connection/connection_panel_test.dart` and verify it passes.

### Task 2: Add failing status panel segmented tests

**Files:**
- Modify: `app/test/status_bar_bootstrap_test.dart`

- [ ] Add failing widget tests that open GPS, battery, and alert status panels and assert each panel shows its two segment labels.
- [ ] Add a failing widget test asserting flight mode does not show segmented controls.
- [ ] Add a failing widget test asserting the expanded status panel height is `56 * 4`.
- [ ] Run `flutter test test/status_bar_bootstrap_test.dart` and verify the new assertions fail for the expected missing content/old height.

### Task 3: Implement status panel segmented content

**Files:**
- Modify: `app/lib/features/status_bar/status_bar_bootstrap.dart`

- [ ] Add the minimal local data model for status panel subpages and the panel-to-segment mapping for GPS, battery, and alert.
- [ ] Keep flight mode on a single content branch with no segmented control.
- [ ] Increase the expanded status panel height by one status label height.
- [ ] Render the shared segmented control and switch the panel body content inside the existing expanded panel container.
- [ ] Keep content switching internal to the expanded panel and preserve existing top-level panel animation hooks.
- [ ] Run `flutter test test/status_bar_bootstrap_test.dart` and verify it passes.

### Task 4: Regression verification

**Files:**
- Verify only

- [ ] Run `flutter test test/frontend_app_behavior_test.dart` and verify top-level routing regressions still pass.
- [ ] Run `flutter test test/features/connection/connection_panel_test.dart test/status_bar_bootstrap_test.dart test/frontend_app_behavior_test.dart` and verify the focused widget suite passes.
- [ ] Run `flutter analyze` and verify there are no analyzer issues.
- [ ] Run `flutter build windows` and verify the Windows build succeeds.
