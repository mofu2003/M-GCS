# 快速参考

更新日期：2026-10-14

当前规则以根目录 [AGENTS.md](../../AGENTS.md) 为准。本页只帮助快速定位，不新增规则。

## 代码放在哪里

```text
Flutter 页面/状态        → flutter_shell/
C ABI 和 JSON 边界        → bridge/ffi/
链路、MAVLink、飞控语义   → core/
第三方源码                → third_party/（不修改）
```

依赖方向：`flutter_shell → bridge/ffi → core → third_party`。

## 遇到一个 Bug

1. 找到入口、状态来源、调用方和测试。
2. 写出具体复现条件。
3. 确认实际后果，而不是只列理论风险。
4. 做最小修复。
5. 补回归测试并运行相关验证。

没有复现路径、实际后果和可验证行为的问题，不自动扩大成重构任务。

## 多机和异步操作

- 明确绑定 `vehicle_id`、会话和目标 `sysid/compid`。
- 不使用当前 UI 焦点代替异步操作的目标。
- 切换焦点、断开或销毁期间仍要保证对象生命周期有效。

## 飞行命令结果

- “已发送”不等于“飞控接受”。
- `COMMAND_ACK` / `MISSION_ACK` 的拒绝、超时和取消必须可见。
- 取消在飞控确认前不能报告为已完成。

## 什么时候加抽象

只有存在当前真实使用方、能减少真实复杂度，并且有测试边界时才增加抽象。不要为了
未来的 PX4、未来的页面或未来的插件先搭空壳工厂。

## 常用验证

```bash
git diff --check
tools/preflight.sh --changed
tools/preflight.sh
```

相关入口：

- 构建和测试：[docs/development.md](development.md)
- 当前能力：[docs/capabilities.md](capabilities.md)
- 当前路线：[docs/ROADMAP.md](ROADMAP.md)
- FFI 目录：[bridge/ffi/API_CATALOG.md](../bridge/ffi/API_CATALOG.md)
