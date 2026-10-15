# ADR-0011: 固件参数名持有机制与循环依赖处理

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

**状态**: Accepted  
**日期**: 2026-10-13
**作者**: Architecture Team  
**关联**: S6 (`docs/constraints/safety.md`)、门禁 (`tools/check_firmware_literals.py`)、VTOL 识别修复

---

## 问题

S6 约束「参数名由插件提供」的意图是明确的，但落地机制（角色表）存在结构性缺口。

某些参数（如 `Q_ENABLE`）的存在本身用来**决定 profile**，因此无法从 profile 对应的角色表中获取：

```
要决定 Profile (plane / vtol)
  需要先判断是否 QuadPlane
    需要先知道 Q_ENABLE 参数名
      但参数名应该从 Profile 对应的角色表中获取
        ↑ 循环依赖！
```

此外，plane 和 vtol 两个 profile 的角色集**逐字节相同**，无法用角色表区分它们。

**结论**：角色表机制虽然适用于大多数参数，但对这一类"决定 profile 本身的参数"**结构上不可用**。

---

## 方案

保持角色表为标准机制，但明确两条并列的正式渠道：

### 1. 角色表机制（标准，覆盖大多数参数）

- **适用**：不依赖 profile 决策的参数
- **实现**：`core/src/config/ParameterDescriptors.json` 的 `roles` 段提供跨 profile 映射
- **下发**：通过 `mg_config_get_param_roles` 告知前端"此 profile 中 pid.rate.roll.p 对应 PID_RATE_ROLL_P"
- **前端使用**：只写语义角色，通过 `ConnectionController.roleParam()` 解析为具体参数名
- **好处**：前端代码中零参数名字面量

### 2. 插件方法（补充，用于特定场景）

- **适用**：用来决定 profile 本身的参数（Q_ENABLE、hybrid_mode 等）
- **实现**：插件方法直接返回参数 ID（如 `IFirmwarePlugin::quadplane_enable_param_id()`）
- **所在位置**：`core/src/firmware/firmware_plugin_*.cc` 中
- **下发**：通过 FFI 下发具体参数名
- **必要性**：这是唯一可行的方式，因为角色表无法解决循环依赖

---

## 实施

### 代码层面

1. **插件实现** (`core/src/firmware/firmware_plugin_ardupilot.cc`)
   ```cpp
   std::string_view Ardupilot::quadplane_enable_param_id() const {
     return "Q_ENABLE";
   }
   ```

2. **FFI 暴露** (`bridge/ffi/ffi_internal.h`)
   - 添加 FFI 接口使前端能调用插件方法

3. **前端使用** (`flutter_shell/`)
   - 调用 FFI 获取参数名，而非硬编码

4. **通用代码中移除**
   - `bridge/ffi` 中的 `Q_ENABLE` 字面量（共 4 处）
   - 改用插件方法或角色表

### 门禁层面

1. **路径豁免** (`tools/check_firmware_literals.py`)
   ```python
   # 在 SCAN_DIRS 或 scan() 中增加豁免规则
   # 豁免: core/src/firmware/ 中的插件实现文件
   ```

2. **规则说明** 
   - 添加注释说明为什么豁免此路径
   - 豁免范围精确到插件实现路径，**不豁免整个 core/src**

### 文档层面

1. **S6 约束补充** (`docs/constraints/safety.md`)
   - 明确两条正式渠道（角色表 + 插件方法）
   - 说明适用场景与循环依赖的原因
   - 记录门禁豁免范围

2. **本 ADR** 
   - 记录为什么需要插件方法

---

## 决策

**采纳此方案的理由**：

1. **语义正确**：S6 的本意是"插件负责提供"，不管是角色表还是方法，都体现了这一点

2. **实现可行**：角色表和插件方法并行，各取所长
   - 大多数参数走角色表（前端零字面量）
   - 特殊参数走插件方法（解决循环依赖）

3. **门禁可被正确设计满足**：明确豁免后，门禁能够区分"合法的持有者"与"泄漏的字面量"

4. **防止重复**：明确记录"为什么 Q_ENABLE 必须这样"，下一个人不会再踩坑

5. **长期可维护**：如果将来有新的"决定 profile 的参数"，已有清晰的模式可循

---

## 约束

- **豁免范围必须精确**：只豁免 `core/src/firmware/` 下的插件实现文件，不豁免整个 `core/src`
- **门禁 diff 单独评审**：不能与业务改动（VTOL 识别修复）混在同一个提交
- **S6 正文必须同步**：约束描述与实现的对齐是这个 ADR 的核心目的
- **基线管理**：不为插件路径新增豁免项，因为它是合法持有者

---

## 收益

- ✅ S6 的描述与实现对齐，消除自相矛盾
- ✅ 明确了参数名来源的两条正式渠道，降低理解成本
- ✅ 门禁能够被正确设计满足，减少人工裁决
- ✅ 防止下一个人重复尝试角色表或其他不可行的方案
- ✅ 为多固件支持（PX4 vs ArduPilot）提供清晰的参数管理模式

---

## 成本

- 修改门禁实现（tools/check_firmware_literals.py）——低风险，改动清晰
- 更新 S6 约束文档——必要，无成本
- 新增 ADR 记录——标准做法，无成本

---

## 风险缓解

**风险 1：门禁豁免范围过宽，后续泄漏难以发现**
- **缓解**：精确豁免到 `core/src/firmware/` 而非整个 `core/src`
- **缓解**：门禁 diff 单独评审，确保豁免规则清晰可验证

**风险 2：角色表机制失效，所有参数都用插件方法**
- **缓解**：在约束中明确标记角色表为标准、插件方法为补充
- **缓解**：代码审查时优先推荐角色表方案

**风险 3：下一个开发者理解不到位，又加新豁免**
- **缓解**：本 ADR + S6 补充写得足够清楚
- **缓解**：代码审查时按 S6 + ADR-0011 检查

---

## 参考

- S6 约束：`docs/constraints/safety.md`
- 门禁实现：`tools/check_firmware_literals.py`
- 冲突分析：`docs/reports/2026-10-13-s6-gate-plugin-ownership-conflict.md`
- 参数描述符：`core/src/config/ParameterDescriptors.json`
