# M-GCS 工程质量改进执行计划

**计划制定**：2026-10-12  
**目标状态**：从 B+ 级达到 A 级  
**预计周期**：3-6 个月  
**负责人**：质量改进团队

---

## 第一阶段：架构安全加固（1-2 周）

### 阶段目标
- 消除并发崩溃风险（KD-49/52）
- 建立并发测试框架
- 明确锁序与所有权模型

### 1.1 任务：ADR-0005 修订与 KD-49 修复

**当前状态**：
- `mavlink_adapter_internal.h:181-190` 在 `send_mtx` 内调用外部回调
- 可能构成 ABBA 死锁（mtx → send_mtx vs send_mtx → mtx）
- 已有设计分析，需实现修复

**执行步骤**：
```
1. 编写 ADR-0005 补充文档
   - 明确 send_mtx 的锁范围和锁序
   - 定义"锁内复制、锁外调用"的模式
   
2. 修改 mavlink_adapter_internal.h
   - 改造 deliver_frame() 为锁内取回调副本、锁外调用
   
3. 补充并发测试
   - 参考 test_concurrent_command_and_disconnect.cc 的模式
   - 新增"disconnect 时回调重入"的测试
   - 用 TSan 验证
   
4. 代码审查 + 合并
```

**验收标准**：
- ✅ ADR 文档完整（≥500 字，含设计与理由）
- ✅ 代码改动 ≤50 行（minimal change）
- ✅ 新测试通过（3 个新的 TSan 用例）
- ✅ preflight.sh 全绿

---

### 1.2 任务：KD-52 所有权模型明确与修复

**当前状态**：
- 接收回调捕获裸指针：`[this, mav = mavlink_ptr.get()]`
- 安全完全依赖 `reset()` 的顺序约定
- 顺序一改就 use-after-free

**执行步骤**：
```
1. 修改 connection_manager.cc
   - 改为 weak_ptr 或 shared_ptr 捕获
   - 在回调内检查 ptr 有效性
   
2. 在 reset() 加断言测试
   - 固定 close() 先于 reset() 的顺序
   - 测试失败时明确报错（不是静默 use-after-free）
   
3. 补充压力测试
   - 并发 close + callback 触发
   - 20 线程 × 1000 迭代
```

**验收标准**：
- ✅ 零裸指针捕获（全用智能指针）
- ✅ 断言保护（顺序一变立即报错）
- ✅ 压力测试通过（无内存错误）
- ✅ TSan 干净

---

### 1.3 任务：并发测试框架完善

**当前状态**：
- 有部分并发测试（t3 贡献）
- 缺少"断链时回调并发"场景

**执行步骤**：
```
1. 新建 core/tests/test_concurrency_comprehensive.cc
   - 复制 t3 的模板（CommandEnv、mock_vehicle 等）
   - 补充 5-10 个关键场景（见下表）
   
2. 启用 TSan 守卫
   - tools/sanitize.sh --tsan-only 在 CI 上运行
   - 本地开发时 cmake -DMGCORE_ENABLE_TSAN=ON
   
3. 建立"竞态场景库"
   - 文档化已验证的场景（通过/失败都记录）
   - 避免重复测试或漏测
```

**关键场景**：
| 场景 | 触发条件 | 预期结果 |
|---|---|---|
| KD-49: disconnect + send | 并发写+断链 | 无死锁（≤5s超时） |
| KD-52: close + callback | 读线程callback + close | 无 use-after-free |
| KD-51: borrow+block | 借用指针调阻塞方法+disconnect | 改后租约保活 |
| 参数变更+车切换 | 并发改参数+切换车 | 无状态混乱 |
| 任务上传+断链 | 上传中disconnect | 失败回滚，无存储泄漏 |

**验收标准**：
- ✅ ≥8 个场景测试
- ✅ TSan 全绿（0 data race）
- ✅ 5 次并发运行无间歇失败

---

## 第二阶段：借用指针改造（2-4 周）

### 阶段目标
- 消除 KD-51：110 处借用指针阻塞风险
- 改为租约模式（`shared_active_mavlink()`）
- 分批改造（按模块）

### 2.1 批次规划

**批次 1（第 1 周）：任务命令族**
- 文件：`ffi_flight_commands.cc`
- 改动行数：估 30-50 行
- 受影响的 FFI：7 个（arm、disarm、rtl、takeoff、land、set_mode、custom_command）
- 测试：`test_sim_commands.cc`（已有，需验证）

**批次 2（第 1 周）：参数族**
- 文件：`ffi_params.cc`
- 改动行数：估 40-60 行
- 受影响的 FFI：5 个（get/set/get_all/save/load）
- 测试：`test_sim_params.cc`

**批次 3（第 2 周）：任务传输族**
- 文件：`ffi_mission_commands.cc`（含下载/上传/验证）
- 改动行数：估 50-80 行
- 受影响的 FFI：6 个
- 测试：`test_sim_mission.cc`

**批次 4（第 2 周）：日志下载族**
- 文件：`ffi_log_commands.cc`
- 改动行数：估 30-40 行
- 受影响的 FFI：4 个
- 测试：`test_sim_logging.cc`

**批次 5（第 3 周）：校准族**
- 文件：`ffi_calibration.cc`
- 改动行数：估 20-30 行
- 受影响的 FFI：3 个
- 测试：`test_sim_calibration.cc`

**批次 6-10（第 3-4 周）：其他**
- 剩余 ~40 处分散在 10 个文件
- 单个文件 ≤5 处，改动最小

### 2.2 改造模板

**改前**：
```cpp
// bridge/ffi/ffi_params.cc
mg_param_t* mg_params_get_all(...) {
    auto mav = get_active_mavlink();  // ❌ 借用指针
    if (!mav) return ...;
    
    mav->request_all_parameters();  // 阻塞可能 ≥1s
    // 在 disconnect 并发时，mav 可能被释放
    
    while (auto param = mav->get_next_param(...)) {  // 💣 可能 use-after-free
        ...
    }
}
```

**改后**：
```cpp
mg_param_t* mg_params_get_all(...) {
    auto rental = shared_active_mavlink();  // ✅ 租约，refcount++
    if (!rental) return ...;
    
    rental->request_all_parameters();  // 阻塞时 rental 活跃
    // 即使 disconnect 并发，rental.shared_ptr 持有 refcount
    // adapter 不会在回调中释放
    
    while (auto param = rental->get_next_param(...)) {  // ✅ 安全
        ...
    }
}  // rental 超出作用域，refcount--
```

### 2.3 执行与验收

**每个批次流程**：
```
1. 改代码（按模板）
2. 运行对应测试（需 5 次连续通过）
3. 运行 TSan 验证（0 data race）
4. 提交 + 代码审查
5. 标记"已改造"
```

**全阶段验收**：
- ✅ 110 处全部改造完毕
- ✅ `grep "get_active_mavlink()" bridge/ffi/*.cc` 返回 0 结果
- ✅ 所有现有测试通过
- ✅ 无新增 TSan 警告

---

## 第三阶段：代码组织优化（2-3 周）

### 阶段目标
- 将 `connection_controller.dart` 从 3833 行 → ≤2500 行
- 继续拆分 `mission_planning_panel.dart`（已有 KD-44 模式）
- 建立"拆分证据要求"流程

### 3.1 Connection Controller 拆分计划

**已完成的拆分**（KD-43）：
- ✅ 波形子系统 → `waveform_controller.dart`（751 行）
- ✅ 原文件 4160 → 3833 行

**下一步拆分**（按共变度）：

**拆分 1：参数管理器**
- 抽取：参数字段 + 15 个参数相关方法
- 目标文件：`parameter_manager.dart`（≈400 行）
- 接线：`refreshParameters()`、`setParameter()` 等 6 个生命周期钩子
- 预期效果：3833 → 3400 行

**拆分 2：飞行命令执行器**
- 抽取：命令字段 + 12 个命令相关方法
- 目标文件：`flight_command_executor.dart`（≈350 行）
- 接线：`executeArm()`、`executeMode()` 等 4 个钩子
- 预期效果：3400 → 3000 行

**拆分 3：遥测快照**
- 抽取：遥测缓存 + 5 个遥测获取方法
- 目标文件：`telemetry_snapshot.dart`（≈200 行）
- 接线：`captureTelemetry()` 等 3 个钩子
- 预期效果：3000 → 2800 行

**验证方式**：
```bash
# 改前后运行 git log -L，确认无新增共变
git log -L 1000,1100:connection_controller.dart --oneline | wc -l
# 拆分后同一块的共变不应增加
```

### 3.2 Mission Planning Panel 继续优化

**已完成**（KD-44）：
- ✅ 任务传输段 → `mission_transfer_section.dart`（751 行）
- ✅ 原文件 3053 → 2362 行

**下一步**：
- 抽取"设置页"相关（≈300 行）
- 抽取"任务实体编辑弹窗"（≈400 行）
- 预期：2362 → 1600 行

### 3.3 AMap 地图组件评估

**现状**：2116 行，KD-46 决定"暂不拆分"

**新证据收集**：
```
如果后续提交里出现"两坨代码独立演进"现象
（某两个方法集在多个提交里只一起改动，但各自也有独立改动），
则触发拆分
```

**收集周期**：每 10 个提交检查一次

---

## 第四阶段：参数与功能清理（2-3 周）

### 4.1 KD-53：PX4 陀螺参数硬编码清理

**当前问题**：
```cpp
// core/src/calibration/calibration_validator.cc:59-72
const auto params = {
    kGyroOffsX,  // "INS_GYROFFS_X" → PX4 无此参数
    kGyroOffsY,  // "INS_GYROFFS_Y" → 回读失败
    kGyroOffsZ   // "INS_GYROFFS_Z"
};
```

**解决方案**：
1. 利用已有的角色表机制（KD-42）
2. 在 `ParameterDescriptors.json` 的 `roles` 段补充"陀螺偏移参数"
3. 改 calibration_validator 改为按角色查找

**执行步骤**：
```
1. 读 ParameterDescriptors.json，理解 roles 结构
2. 为 ardupilot 和 px4 分别定义"陀螺偏移"角色
3. 修改 calibration_validator.cc
   旧：find_param("INS_GYROFFS_X")
   新：find_param_by_role("gyro_offset_x")
4. 补充验收用例（ardupilot + px4 各一轮）
```

**验收标准**：
- ✅ 0 个硬编码参数名（grep "INS_GYROFFS" 返回 0）
- ✅ 测试通过（ardupilot 陀螺校准 ✓，px4 陀螺校准 ✓）
- ✅ 角色表完整

### 4.2 KD-54：STATUSTEXT 口径统一

**当前问题**：
- Gyro：`"Success"` / `"Complete"`（严格）
- Radio：`"RC "` / `"radio"` / `"complete"`（宽松）
- 同一条文案在不同校准类型结论不同

**解决方案**：
```cpp
// core/src/calibration/strategies/calibration_strategy.h
// 新增统一的 STATUSTEXT 判定接口

class CalibrationStrategy {
public:
    // 旧的子类各实现的 parse_statustext()
    // 都改为调用通用的判定器
    
    struct StatusTextResult {
        enum { UNKNOWN, PROGRESS, SUCCESS, FAILURE } type;
        int progress_percent = -1;
    };
    
    static StatusTextResult parse_common_statustext(
        const std::string& text,
        CalibrationPhase phase);  // 不同校准类型、同一判定规则
};
```

**关键变更**：
| 校准类型 | 现状 | 新规 |
|---|---|---|
| Gyro | `"Success"`、`"Complete"` | 两者都可（大小写无关） |
| Baro | `"Success"` | 同 Gyro |
| Accel | `"Success"` | 同 Gyro |
| Radio | `"RC "`、`"complete"` | 改为严格匹配 `"radio calibration.*complete"` |

**验收标准**：
- ✅ 新增 `test_statustext_parsing_consistency.cc`
- ✅ 5 种校准类型各测 10 条真实飞控文案
- ✅ 结果一致（同文案在所有校准类型返回相同判定）

### 4.3 KD-50：模式号字面量守卫扩展

**当前状态**：
- `check_firmware_literals.py` 只守卫参数名
- 模式号、命令编码无守卫

**待扩展的字面量**：
```cpp
// ffi_flight_commands.cc
0x04040000U  // Mode AUTO（ArduPilot）
10U          // Mode GUIDED
3U           // MAV_AUTOPILOT_ARDUPILOTMEGA
12           // MAV_AUTOPILOT_PX4
241          // MAV_CMD_PREFLIGHT_CALIBRATION
42426        // MAV_CMD_DO_CANCEL_MAG_CAL
42429        // MAV_CMD_ACCELCAL_VEHICLE_POS
```

**解决方案**：
1. 优先改用官方枚举（`MAV_AUTOPILOT_*`、`MAV_CMD_*`）
2. 模式号集中到插件表，由插件下发
3. 扩 `check_firmware_literals.py` 拦这两类

**执行步骤**：
```
1. 替换硬编码为枚举
   0x04040000U → ARDUPILOT_MODE_AUTO  // 定义在...
   241 → MAV_CMD_PREFLIGHT_CALIBRATION
   
2. 集中模式号到插件
   core/src/firmware/mode_table.cc
   新增 get_mode_by_enum()
   
3. 扩 tools/check_firmware_literals.py
   - 新增"禁止模式号字面量"规则
   - 新增"禁止命令编码字面量"规则
```

**验收标准**：
- ✅ `grep "[0-9]\{2,\}U\|= [0-9]\{2,\}" bridge/ffi/*.cc` 清零
- ✅ 所有模式号改为枚举或由插件表提供
- ✅ 门禁脚本通过（检出所有新字面量）

---

## 第五阶段：功能补全与规范（3+ 周）

### 5.1 KD-48：围栏与 ADS-B 接线（P3 后续）

**暂不处理**（属于 P3+ 功能开发）

建议后续时单独立项：
- 围栏数据源接线（mission 协议）
- 围栏距离提示（参照 MP FenceDist）
- ADS-B 回调接线

### 5.2 KD-55：测绘 Turnaround 实现（P3 后续）

**暂不处理**，建议后续立项：
- 相机横竖拍处理
- 固定翼掉头延伸段

### 5.3 KD-56：许可文档补充

**当前问题**：
- `USBBoardInfo.json` 与 QGC 同名文件高度重叠
- 没有许可文本和 ADR 登记

**执行步骤**：
```
选项 1（推荐）：补充许可文档
- 新建 core/src/transport/USBBoardInfo.LICENSE.txt
- 保留 QGC 版权声明 + Apache-2.0 许可
- 在 docs/adr/0002 补充来源、许可分支

选项 2：自建数据
- 只保留 M-GCS 实际需要的板卡（≈20 条）
- 独立维护，无许可包袱
```

**推荐选项 1**（工作量小）

---

## 执行时间表

```
第 1 周：
  [ ] ADR-0005 修订
  [ ] KD-49/52 修复 + 并发测试框架
  
第 2-3 周：
  [ ] KD-51 改造批次 1-5
  [ ] connection_controller 拆分 1-2
  
第 4 周：
  [ ] KD-51 改造批次 6-10
  [ ] connection_controller 拆分 3
  
第 5 周：
  [ ] KD-53 参数清理
  [ ] KD-54 STATUSTEXT 统一
  [ ] KD-50 字面量扩展
  [ ] KD-56 许可补充
  
第 6 周+：
  [ ] KD-48/55 后续立项
  [ ] 完整回归测试 + 质量报告
```

---

## 质量指标跟踪

### 改进前后对比

| 指标 | 改前 | 改后 | 目标 |
|---|---|---|---|
| TSan 报警数 | 未知 | 0 | 0 |
| 借用指针阻塞 | 110 | 0 | 0 |
| 待修复偏差 | 10 | 0 | 0 |
| 单文件最大行 | 3833 | 2000-2500 | ≤2000 |
| 并发测试用例 | 5+ | 15+ | 20+ |
| 门禁通过率 | 99% | 100% | 100% |

---

## 风险与应对

| 风险 | 影响 | 应对 |
|---|---|---|
| 改造过程引入新 bug | 高 | 每批改造后完整回归测试 |
| 测试覆盖不足 | 高 | 补充 TSan 和压力测试 |
| 拆分后性能下降 | 低 | Dart 侧拆分无性能影响 |
| 审查周期长 | 中 | 并行处理不相关改动 |

---

## 成功标志

✅ 所有改动完毕后的状态：

1. **并发安全**
   - ADR-0005 明确且实现
   - KD-49/52 修复完毕
   - KD-51 全部改为租约
   - TSan 全绿

2. **代码质量**
   - 最大单文件 ≤2000 行
   - 0 个借用指针阻塞
   - 所有参数改为角色表或枚举

3. **测试完善**
   - 15+ 并发测试用例
   - STATUSTEXT 一致性测试
   - 门禁更新完毕

4. **文档齐全**
   - ADR 补充完毕
   - 许可文档补充
   - 项目质量升级到 A 级

---

**下一步**：按阶段 1 开始执行
