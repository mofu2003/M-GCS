# S6 门禁与固件插件持有参数名的冲突

**报告日期**: 2026-10-13
**状态**: 待决定（需要架构决策，未修改门禁）
**关联**: ADR-0010、S6（`docs/constraints/safety.md:37-41`）
**触发于**: 修复 QuadPlane 的 VTOL 识别（去掉 `bridge/ffi` 中四处 `Q_ENABLE` 字面量）

---

## 摘要

按 S6「参数名只能由固件插件提供」的要求，把 `Q_ENABLE` 收敛进固件插件
（`core/src/firmware/firmware_plugin_ardupilot.cc:176`）后，S6 门禁
`tools/check_firmware_literals.py` 失败，且**只**标记这同一个文件。

即：门禁的报错文案与门禁的实现相互矛盾——

```
[firmware-literals] 新增固件参数字面量：core/src/firmware/firmware_plugin_ardupilot.cc: Q_ENABLE
[firmware-literals] S6：参数名必须来自核心插件或参数描述符，不得在通用代码中硬编码。
```

第二行明确承认「核心插件」是合法来源；第一行标记的却正是核心插件本身。

这不是本次改动引入的疏漏，而是门禁缺少「插件目录」这一概念所致：它无法表达
「这个文件是值的持有者」，因此**任何满足 S6 的写法都会失败**。需要一次架构决策，
本次未擅自修改门禁。

---

## 一、S6 的原文与其声明的验证方式

`docs/constraints/safety.md:37-41`：

> **S6 固件专属值经插件获取**
> 模式号、参数名、命令编码、校准流程只能由固件插件提供。通用代码中不得出现这些值的字面量。
> - 违反后果：更换或升级固件后行为错误。
> - 验证：静态检查通用代码路径中是否出现已知模式号和参数名字面量。
> - 参数名的落地方式：插件在 `core/src/config/ParameterDescriptors.json` 的 `roles` 段
>   按机型 profile 提供「语义角色 → 固件参数名」，经 `mg_config_get_param_roles` 下发；
>   前端只写语义角色（如 `pid.rate.roll.p`、`rc.map.yaw`），用
>   `ConnectionController.roleParam()` 解析。门禁 `tools/check_firmware_literals.py`
>   挡住新增字面量；`tools/firmware_literals_baseline.json` 记录已核实的暂缓项
>   （当前 7 个文件 / 14 处）……

两层要求是清楚的：

1. 固件专属值的**持有者是插件**；
2. **通用代码**中不得出现这些值的字面量。

问题在于，代码库里没有一处**结构化地定义**「哪段路径算插件、哪段算通用代码」，
门禁只能靠路径二分，而它选的二分方式把插件划进了「通用代码」一侧。

---

## 二、门禁实现与冲突点

`tools/check_firmware_literals.py:49-53`：

```python
SCAN_DIRS: list[tuple[Path, tuple[str, ...]]] = [
    (ROOT / "flutter_shell" / "lib", ("*.dart",)),
    (ROOT / "core" / "src", ("*.cc", "*.h")),   # ← 插件实现位于此
    (ROOT / "bridge" / "ffi", ("*.cc", "*.h")),
]
```

`core/src` 被当作一个**不分层的「通用代码」桶**。而 S6 指定的合法持有者
`core/src/firmware/firmware_plugin_ardupilot.cc` 恰好住在里面。`scan()`
（`:154-166`）对整个目录树做 `rglob`，没有任何路径豁免。

**结论：门禁按现状无法被正确设计满足。** 只要插件真的去持有参数名，它就必然报错；
唯一能让它变绿的做法是把名字挪回通用代码——正好是 S6 禁止的行为。

### 参数名识别本身是正确的

门禁把 `ParameterDescriptors.json` 里所有符合 `NAME_RE`
（`tools/check_firmware_literals.py:61`，`^[A-Z][A-Z0-9]*_[A-Z0-9_]+$`）的键收集成
「参数名全集」，再拿扫描到的字符串去匹配。实测 `Q_ENABLE` **确实在全集内**
（`core/src/config/ParameterDescriptors.json:217925` 等），所以门禁判定
「`"Q_ENABLE"` 是一个固件参数名」是**正确**的。

冲突**纯粹在于位置**，不在于识别。这一点很重要：它说明不需要提高检出的精度，
而是需要给「持有者」一个可表达的位置。

---

## 三、为什么角色表这条路对本参数走不通

S6 正文声明的落地机制是**角色表**：由 `ParameterDescriptors.json` 的 `roles` 段
提供「语义角色 → 固件参数名」，经 `mg_config_get_param_roles` 下发。角色表**确实**
定义了该角色（`core/src/config/ParameterDescriptors.json:1115`）：

```json
"frame.quadplane_enable": "Q_ENABLE"
```

但角色表在本参数上**可证明不可用**，因为存在循环依赖：

```
mg_config_get_param_roles
  → ffi_param_profile(core)                    (bridge/ffi/ffi_internal.h:257)
    → ffi_ardupilot_vehicle_class(core)        (bridge/ffi/ffi_internal.h:233)
      → 读取 "Q_ENABLE"                        ← 需要先知道参数名
```

也就是说：后端要下发角色表，得先决定用哪个 profile；要决定 profile，得先知道
它是不是 QuadPlane；而判据正是角色表要告诉它的那个参数名。角色表因此**不可能是**
这个参数的来源。

还有第二重障碍：`plane` 与 `vtol` 两个 profile 的角色集**逐字节相同**（各 103 个角色，
`plane == vtol` 为 `True`），且只有 `plane` 定义了 `frame.quadplane_enable`。
所以即使忽略循环，读角色表也**永远无法区分 plane 与 vtol**。

**这不是「懒得用角色表」，而是角色表在此处结构上不可用。** 因此 S6 正文把角色表
写成参数名的唯一落地方式，与实际需要之间存在缺口；插件方法
（`IFirmwarePlugin::quadplane_enable_param_id()`）是目前唯一可行的持有方式。

---

## 四、当前门禁已经在产出需要人工裁决的命中

基线 `tools/firmware_literals_baseline.json` 现有 **7 个文件 / 14 处**，每条都必须
手写理由。其中若干条并非真实违规，而是机制无法表达的合法情形：

| 文件 | 处数 | 理由性质 |
|---|---|---|
| `bridge/ffi/ffi_config.cc` | 1 | 「注释中引用…非硬编码使用，是文档说明」 |
| `flutter_shell/lib/connection/parameter_ffi_backend.dart` | 1 | 「注释中引用…是文档说明」 |
| `flutter_shell/lib/configuration/widgets/parameter_helpers.dart` | 3 | 含一条注释引用；两条为待重构欠账 |
| `flutter_shell/lib/configuration/motors/motor_output_page.dart` | 5 | 待重构欠账 |
| `flutter_shell/lib/configuration/ports/ports_configuration_page.dart` | 2 | 待重构欠账 |
| `flutter_shell/lib/configuration/calibration/rc_calibration_view.dart` | 1 | 待重构欠账 |
| `flutter_shell/lib/components/realtime_waveform.dart` | 1 | 待重构欠账 |

**其中 2 条是注释里提到参数名而被判违规**，属于检出层面的误报（门禁不区分字符串
与注释）。这说明该机制已经在承担「人工逐条裁决」的角色，而它本应是一个自动门禁。

因此，「再加一条插件豁免」的做法会顺着这个趋势走：让门禁**永久无法区分
「正确持有者」与「泄漏的字面量」**，并使「基线只允许缩小」的承诺更难兑现。

---

## 五、三个选项

### 选项 A（推荐）：让门禁承认插件是指定提供者

把插件实现所在的路径当作 S6 语义中的「插件」，而非通用代码。门禁届时严格执行的
正是 S6 原话：「插件**之外**不得出现固件专属值字面量」。

- 改动点：`tools/check_firmware_literals.py` 的 `SCAN_DIRS` / `scan()` 增加路径豁免，
  并在 S6 正文补一句「门禁豁免插件实现路径」，使规则与实现一致。
- 优点：门禁可被正确设计满足；不产生新欠账；把 S6 的**意图**而非**现状**编码进去。
- 风险：**修改门禁本身**。必须谨慎，且在改动定稿前把门禁 diff 单独提交评审，
  不能与业务改动混在一起，否则等于给自己放行。
- 注意：豁免范围应尽量窄且可验证。建议精确到插件实现文件或
  `core/src/firmware/` 下的实现文件，**不要**豁免整个 `core/src`。

### 选项 B：登记一条基线豁免

保持门禁不变，在基线中为插件加一条，理由写「角色表因循环依赖不可用」。

- 优点：改动最小，不碰门禁。
- 缺点：门禁学不到任何东西，同类问题会重复出现；新增欠账与「基线只允许缩小」相悖；
  且这条豁免与那 2 条注释误报混在一起，进一步稀释基线的含义。

### 选项 C：其他

例如：

- 把角色表机制扩成「**不需要 profile 就能解析**」的形式（例如增一个跨 profile 的
  全局角色段），使本参数回到 S6 正文声明的轨道上；
- 或明确接受「角色表只覆盖需要 profile 的参数」，并在 S6 正文中把插件方法写成
  与角色表并列的第二条正式落地方式。

---

## 六、建议

采纳 **选项 A**，并顺带处理两个相邻问题：

1. **豁免范围要窄且可验证**，精确到插件实现文件而非整个 `core/src`。
2. **门禁 diff 单独评审**，不与 VTOL 业务改动混在同一个提交里。
3. **S6 正文与实现对齐**：正文应写明插件实现路径是合法持有者，以及角色表存在
   循环依赖、对「决定 profile 本身的参数」不可用。否则下一个人会再次尝试把
   `Q_ENABLE` 塞进角色表而失败。
4. **可选**：那 2 条「注释中引用」的误报值得单独修（让检出跳过注释），
   从而让基线条目都能代表真实欠账。

---

## 附：本次改动的其余部分状态

与门禁冲突无关的部分均已完成并通过验证（`ctest` **34/34**，编译无警告）：

| 项 | 性质 | 状态 |
|---|---|---|
| `classify_ap_vehicle` 的 `default` 不再回退成 `Copter` | bug 修复 | 完成 |
| VTOL 区间收敛为 `is_vtol_mav_type()` 谓词，含 25 | bug 修复 | 完成 |
| `bridge/ffi` 四处 `Q_ENABLE` 字面量收敛 | S6 整改 | 完成 |
| 命令目录不再依赖 `Q_ENABLE`（消除会话锁死） | 设计修复 | 完成 |
| 插件持有参数名（`quadplane_enable_param_id()`） | 能力扩展 | 完成，被本文所述门禁冲突阻塞 |

**未修改任何门禁，未登记任何新的基线豁免。**
