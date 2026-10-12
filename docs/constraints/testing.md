# 测试

本文件分两类规则：约束（MUST）与建议（SHOULD）。每条约束写明违反后果和验证方式。

## 约束

**T1 安全规则必须有真实测试**
每条安全约束（`safety.md` 中的 S1 至 S8）至少有一条断言真实行为的测试。未覆盖的部分必须登记在 `known-deviations.md` 中。
- 违反后果：安全规则没有保障，测试全绿时也不代表规则成立。
- 验证：评审时逐条核对下面的映射表；未覆盖的条目按上一条登记。**没有**自动化脚本能做这件事（"这条规则是否有测试"无法静态判定），所以靠评审，映射表只是让评审有据可依。

| 规则 | 覆盖它的测试（示例，非穷举） |
| --- | --- |
| S1 拒绝必须传播 | `core/tests/test_sim_commands.cc`（ACK 非 ACCEPTED / 超时）、`core/tests/test_sim_mission.cc`（上传被拒后回滚）、`core/tests/test_new_modules.cc` |
| S2 命令结果以 ACK 为准 | `core/tests/test_sim_commands.cc`、`core/tests/test_ffi_contract.cc` |
| S3 飞行状态命令必须确认 | `flutter_shell/test/flight_state_command_confirmation_test.dart`（11 类命令的入口枚举 + 长按/对话框行为）、`flutter_shell/test/mg_command_confirmation_test.dart`、`flutter_shell/test/mg_flight_mode_hold_test.dart` |
| S4 超时报错 | `core/tests/test_sim_commands.cc`、`core/tests/test_state_machines.cc`、`core/tests/test_sim_ui_paths.cc` |
| S5 断链通知与状态失效 | `core/tests/test_sim_ui_paths.cc`（`test_link_loss_notifies_the_interface`：断链发事件）、`core/tests/test_sim_commands.cc`（`heartbeat_fresh` 过期） |
| S6 固件专属值经插件获取 | `tools/check_firmware_literals.py`（门禁脚本）、`core/tests/test_param_descriptors.cc` |
| S7 任务文件不静默覆盖 | `core/tests/test_sim_mission_document.cc`（`test_mission_file_is_never_rewritten`：导入/导出/上传前后源文件逐字节不变）、`core/tests/test_sim_mission.cc` |
| S8 取消必须到达飞控 | `core/tests/test_sim_mission_document.cc`（`test_mission_cancel_reaches_the_vehicle`：取消要发出 MISSION_ACK）、`core/tests/test_sim_calibration.cc`、`core/tests/test_bridge_vehicle_session.cc` |

**T2 禁止假断言**
禁止 `CHECK(true)`、以 OR 组合的恒真断言、只检查返回值非空、无条件 SKIP 的核心测试。
- 违反后果：测试通过但不验证行为。
- 验证：`python3 tools/check_fake_assertions.py` 静态扫描 `core/tests/`、`tests/` 与 `flutter_shell/test/`；`--self-test` 用合成样本证明扫描器本身能报出违规（否则脚本可能只是恒真）。评审仍需检查条件与消息是否写反——静态扫描只能挡住上面四类写法。

**T3 硬件测试隔离**
依赖真实飞控的测试打 `hardware` 或 `manual` 标签，不进入默认门禁。
- 违反后果：默认门禁依赖硬件，无法在无人值守环境运行。
- 验证：门禁命令的标签过滤。

**T4 新 FFI 函数有合约测试**
对应 `ffi-contract.md` 的 F1。
- 违反后果：新接口没有行为保证。
- 验证：`python3 tools/check_api_change_set.py`（改了 `bridge/ffi/ffi_api*.h` 时要求改动集同时含实现/测试/API_CATALOG/文档），外加评审。

**T5 断言宏只有一处定义**
原生测试的断言宏统一用 `core/tests/mgcs_test_check.h` 的 `CHECK(条件, 消息)`（以及 `CHECK_NEAR(实际, 期望, 容差, 消息)`）；测试文件不得再各自 `#define CHECK`。
- 违反后果：参数顺序在各文件之间分叉，跨文件复制调用点时抄错一次就变成恒真断言（T2 禁止的写法），且不易发现。
- 验证：评审看新增测试文件是否包含共享头；`core/tests/` 下 `grep -rn "define CHECK" core/tests/*.cc` 应只有共享头。

**测试计数与退出码**
共享头同时提供计数（`mgcs_test::passed()/failed()`）与 `MGCS_TEST_REPORT()`；测试文件可以保留自己的计数变量名，但必须引用共享计数器，避免出现"断言没跑却退出 0"。


## 建议

**等待条件 [SHOULD]**
等待条件建议使用轮询加超时，避免用固定 `sleep` 作为同步手段。固定等待会让测试在慢机器上失败，在快机器上浪费时间。

**门禁与 CI 对齐 [SHOULD]**
CI 建议运行与 `tools/preflight.sh` 等价的检查。两者不一致时，以 `preflight.sh` 为准，并修正 CI。
