# S6 门禁冲突问题解决总结

**报告日期**: 2026-10-13
**状态**: ✅ 已解决  
**相关 commit**: 7084a62  
**关联文件**: 
- 原始冲突分析：`docs/reports/2026-10-13-s6-gate-plugin-ownership-conflict.md`
- 架构决策：`docs/adr/0011-firmware-parameter-ownership.md`
- 约束更新：`docs/constraints/safety.md`（S6 部分）
- 门禁改动：`tools/check_firmware_literals.py`

---

## 一、问题回顾

### 冲突的本质

S6 约束说：「固件参数名必须由插件提供，通用代码中不得出现字面量」

但门禁实现中：
- ❌ 把整个 `core/src` 当作「通用代码」扫描
- ❌ 把插件实现文件也扫进去了
- ❌ 结果：插件里有参数名 → 门禁报错

**自相矛盾**：满足 S6 的代码会违反门禁，满足门禁的代码会违反 S6。

### 为什么不能用角色表（参数重映射）

参数 `Q_ENABLE` 存在一个结构性循环：
```
要判断是否 QuadPlane
  需要知道 Q_ENABLE 参数名
    但参数名应该从 Profile（由 QuadPlane 决定）的角色表获取
      ← 循环！
```

而且 plane 和 vtol 的角色集完全相同，无法用角色表区分。

**结论**：此参数必须由插件直接提供，不可能走角色表机制。

---

## 二、解决方案

### 1. S6 约束补充

**位置**：`docs/constraints/safety.md` S6 部分

**改动**：明确两条并列的正式渠道

```markdown
参数名的持有方式（S6 的两条正式渠道）：

1. 角色表机制（标准方式，覆盖大多数参数）
   - 插件在 ParameterDescriptors.json 的 roles 段提供映射
   - 经 mg_config_get_param_roles 下发给前端
   - 前端用语义角色，通过 ConnectionController.roleParam() 解析

2. 插件方法（补充方式，用于特定场景）
   - 某些参数因循环依赖不适用角色表
   - 例：Q_ENABLE 用来决定 profile，无法从 profile 的角色表获取
   - 由插件方法直接提供（如 IFirmwarePlugin::quadplane_enable_param_id()）
   - 实现位于 core/src/firmware/ 目录
```

### 2. 新增 ADR-0011

**位置**：`docs/adr/0011-firmware-parameter-ownership.md`

**内容**：
- 问题分析（循环依赖、角色集相同）
- 方案选择（两条并列渠道）
- 实施细节（插件方法、门禁豁免）
- 收益（约束与实现对齐）
- 风险缓解

### 3. 门禁改动

**位置**：`tools/check_firmware_literals.py`

**改动**：
```python
# 豁免列表：插件实现文件是 S6 指定的合法持有者
PLUGIN_EXEMPT_PATHS = [
    "core/src/firmware/*.cc",
    "core/src/firmware/*.h",
]

# scan() 函数中添加豁免检查
def scan() -> dict[str, list[str]]:
    ...
    for path in sorted(scan_dir.rglob(pattern)):
        # 检查是否在豁免路径中
        rel = str(path.relative_to(ROOT))
        is_exempt = any(Path(rel).match(exempt) for exempt in PLUGIN_EXEMPT_PATHS)
        if is_exempt:
            continue
        ...
```

**特点**：
- 豁免范围精确到插件实现文件
- 不豁免整个 `core/src`
- 添加清晰的注释和 ADR 关联

---

## 三、验证结果

### ✅ 门禁通过

```bash
$ python3 tools/check_firmware_literals.py
[firmware-literals] 未新增固件参数字面量；基线内暂缓项 7 个文件 / 14 处
```

### ✅ 插件文件被正确豁免

- `core/src/firmware/firmware_plugin_ardupilot.cc:176` 的 `Q_ENABLE`
- 现在被正确识别为合法的参数名持有者
- 不再产生误报

### ✅ 基线保持稳定

- 之前的 7 个文件 / 14 处暂缓项未变
- 没有新增豁免，没有增加技术债

### ✅ 改动已推送

- commit: 7084a62
- 已推送到 origin/main

---

## 四、设计决策的完整性

### 理由充分

✅ **问题分析明确**：为什么 Q_ENABLE 必须这样
- 循环依赖结构明确
- 角色集相同的事实明确
- 无可否认地说明了角色表机制的不适用

✅ **方案对称**：两条渠道各取所长
- 角色表：标准方式，前端零字面量
- 插件方法：补充方式，解决循环依赖

✅ **实现可验证**：约束与实现对齐
- S6 文本明确承认两条渠道
- ADR 记录了为什么
- 门禁豁免规则明确可审查

### 长期可维护性

✅ **下一个人不会踩坑**：文档明确说明了
- "为什么 Q_ENABLE 必须这样"明白写在 S6 和 ADR 里
- 不会再有人尝试把它挪回通用代码或写进角色表

✅ **设计可扩展**：其他"决定 profile 的参数"有清晰模式可循

✅ **约束不自相矛盾**：S6 的意图得到了准确体现

---

## 五、对后续工作的影响

### VTOL 识别修复可以继续

原本被门禁阻塞的改动现在可以：
- 把 `Q_ENABLE` 从 `bridge/ffi` 移到插件
- 不会再被门禁拦下
- 代码审查时有 ADR 做背景说明

### 可选的后续清理

基线中的 2 条注释误报可以处理（但非必须）：
- 让门禁检出跳过注释中的参数名
- 从基线中移除这两条误报
- 让基线全是真实技术债

---

## 六、改动清单

| 文件 | 改动 | 行数 |
|------|------|------|
| `docs/constraints/safety.md` | S6 约束补充两条渠道说明 | +30 |
| `docs/adr/0011-firmware-parameter-ownership.md` | 新增完整的决策记录 | 163 |
| `tools/check_firmware_literals.py` | 添加插件路径豁免 | +15 |
| **总计** | | **~240 行** |

---

## 七、最终检查清单

```
✅ 约束层面
   □ S6 文本明确承认两条渠道
   □ 说明了循环依赖与角色表不可用的原因
   □ 记录了门禁豁免范围

✅ 架构决策层面
   □ ADR-0011 记录了完整的分析与决策
   □ 说明了为什么需要两条渠道
   □ 记录了收益与风险缓解

✅ 实现层面
   □ 门禁豁免规则明确可验证
   □ 豁免范围精确到文件级别
   □ 添加了清晰的代码注释

✅ 验证层面
   □ 门禁通过（无新增命中）
   □ 插件文件被正确豁免
   □ 基线保持稳定

✅ 文档层面
   □ S6 约束更新完整
   □ ADR 记录清晰完整
   □ 改动已提交并推送
```

---

## 八、总结

这个问题的解决展示了**约束与实现的对齐**的重要性：

- ❌ **症状**：自相矛盾的约束与门禁
- 🔍 **诊断**：门禁缺少对"插件是合法持有者"的理解
- ✅ **治疗**：
  1. 更新约束让其更清晰（承认两条渠道）
  2. 更新实现让其理解约束（豁免插件路径）
  3. 记录决策让其可追溯（ADR-0011）

**结果**：约束清晰、实现正确、可维护性提升。

---

**改动完全符合我们的文档体系原则**：
- ✅ 约束变化有 ADR 记录
- ✅ 技术决策透明可追溯
- ✅ 代码改动有约束支撑
- ✅ 长期维护性得到保证
