# Mission CAD 初步设计

## 目标

Mission CAD 是 M-GCS 的参数化任务设计能力。它面向需要精确规划航线的操作员：点位、距离、方位角、扫描区域、飞行高度和飞行约束都应是明确可编辑的值，而不是只能在地图上凭感觉点击。

首个版本聚焦以下可验证能力：

- 精确放置任务点，并显示其局部坐标。
- 以方位角和距离定义航段。
- 使用多边形区域生成测绘扫描航线。
- 在上传前编译、校验并预览 MAVLink 任务。
- 给出距离、时间、任务项数量和可追溯的诊断结果。

Mission CAD 不是立即实现完整通用 CAD。圆弧、相切、全局几何求解器和所有机型的复杂转弯模型不进入首期范围，避免在基础模型稳定前把复杂度推入 UI。

## 不可突破的架构边界

Mission CAD 的任何领域功能不得绑定某个具体界面框架。后续即使为不同用户提供桌面、移动、Web、CLI 或自动化脚本界面，也不得因为界面替换而重写任务几何、约束、编译和校验逻辑。

| 层 | 职责 | 禁止承担的职责 |
| --- | --- | --- |
| `core/mission/cad` | 文档、几何、坐标换算、约束、编译、估算、校验、持久化 | UI 控件、地图 SDK、窗口状态、MAVLink 上传 |
| `bridge/ffi` | 稳定的版本化 C ABI 和 JSON 契约 | 任何前端语言类型或界面术语 |
| 各前端适配层 | 绘制地图或画布、选择、拖拽、输入、快捷操作和本地展示状态 | 权威几何计算、任务序号分配、航线编译、机型限制判断 |
| 既有任务传输 | 接收已编译的 `MissionPlan` 并执行上传、下载、进度和超时处理 | 作为 CAD 文档源数据 |

拖拽、吸附和临时预览可以由前端提供，但提交后必须表达为领域编辑请求；权威结果始终由 `core` 生成。前端不得维护第二套几何引擎或自行把 CAD 图形拼成 MAVLink 航点。

```mermaid
flowchart LR
    Editor["任意编辑器: 桌面 / 移动 / Web / CLI"]
    Contract["Mission CAD JSON / C ABI 契约"]
    Cad["core/mission/cad\n文档、几何、约束"]
    Compiler["任务编译器与诊断"]
    Plan["MissionPlan / MAVLink 任务项"]
    Transfer["既有任务传输"]

    Editor --> Contract
    Contract --> Cad
    Cad --> Compiler
    Compiler --> Plan
    Plan --> Transfer
```

## 两种任务模型

`MissionCadDocument` 是可编辑的源模型；它保留实体、约束和生成规则，因此可以在修改高度、扫描间距或航段距离后重新编译。

`MissionPlan` 是面向飞控的派生输出；它仅保存 MAVLink 任务项和传输所需信息。下载到的普通 MAVLink 任务可导入为 CAD 草图，但无法自动恢复它原本不存在的参数化约束。

```text
MissionCadDocument --compile--> CompiledMission --extract--> MissionPlan --upload--> Flight Controller
```

编译不上传、编辑不写飞控。上传仍通过既有任务传输入口，并继续遵守真实硬件操作确认要求。

## 坐标与文档模型

CAD 文档在一个明确的 WGS84 原点附近使用局部 ENU 米制坐标：`east`、`north`、`up`。这样距离、方位角、平行和转弯计算具有稳定的米制语义；WGS84 纬经度只在导入、导出、地图显示和 MAVLink 编译边界转换。

所有实体使用稳定 UUID，而不是 widget key、图层序号或界面列表索引。

第一版文档包含：

- 文档版本、文档 ID、名称和元数据。
- 原点及高度基准。
- 实体、约束、图层和飞行器假设。
- 编译配置和可选的用户注释。

建议的 `.mgcad.json` 格式如下。字段使用英文以保证跨语言兼容，前端负责本地化显示。

```json
{
  "schema_version": 1,
  "document_id": "2c2f0c90-55c9-4d0c-a269-a978de6c3451",
  "origin": {
    "latitude_deg": 31.2304,
    "longitude_deg": 121.4737,
    "altitude_m": 12.4
  },
  "entities": [
    {
      "id": "p-start",
      "type": "point",
      "position_enu_m": { "east": 0, "north": 0, "up": 50 }
    },
    {
      "id": "p-end",
      "type": "point",
      "position_enu_m": { "east": 120, "north": 0, "up": 50 }
    },
    {
      "id": "leg-1",
      "type": "segment",
      "start_id": "p-start",
      "end_id": "p-end"
    }
  ],
  "constraints": [
    { "type": "bearing", "entity_id": "leg-1", "degrees": 90 },
    { "type": "distance", "entity_id": "leg-1", "meters": 120 }
  ],
  "layers": []
}
```

航段同时引用端点，距离和方位角作为独立约束保存。这样首期可用确定性编辑规则实现，后续接入更完整的约束求解器时不会破坏文件格式。

## 实体与约束范围

### 第一版实体

- `point`：具有 ENU 位置和高度的任务点或构造点。
- `segment`：引用两个点的直线航段。
- `survey_area`：多边形边界、扫描方向、间距、高度、速度和相机/触发配置。
- `reference`：不导出到任务的参考或构造几何。

任务动作，例如速度变化、相机触发、起飞和返航，在首期以可扩展的 `mission_action` 设计进入模型；只在对应编译 profile 支持时产出 MAVLink 项。

### 第一版约束

- 固定坐标和高度。
- 距离。
- 方位角。
- 扫描间距、速度和航高。

平行、垂直、圆弧、相切和复杂拓扑保留给后续版本。吸附不是约束类型本身：它只是前端辅助操作，提交时必须落为上述明确的实体值或约束。

## 编译、校验与诊断

核心接口的语义为：

```text
compile(document, vehicle_profile, capabilities, vehicle_constraints) -> CompiledMission
```

`CompiledMission` 至少返回：

- `MissionPlan`。
- 用于画布或地图预览的生成路径和转弯几何。
- 距离、时间、照片数、航线数和任务项数量等估算。
- 带严重级别、稳定错误码、实体 ID 和说明的诊断。
- 源实体/约束到生成任务项的映射，便于在编辑器中定位问题。

每次编译都必须校验有限坐标、引用完整性、多边形有效性、高度和速度范围、任务项数量，以及由机型 profile 和 capabilities 决定的最小转弯半径、爬升能力等约束。地形校验可以在第一版后接入，但诊断契约必须已能表示 `not_checked`、`available`、`unavailable` 和失败原因。

编译器不得隐式修改文档、写参数、发送命令或上传任务。

## FFI 契约草案

实现阶段新增 `bridge/ffi/ffi_api_mission_cad.h`，由 `ffi_api.h` 间接包含。它仅使用 C ABI 和版本化 JSON envelope，不暴露 C++ 类或任一前端语言类型。

建议第一批入口：

- `mg_mission_cad_create_document`
- `mg_mission_cad_parse`
- `mg_mission_cad_serialize`
- `mg_mission_cad_apply_edit`
- `mg_mission_cad_validate`
- `mg_mission_cad_compile`

`apply_edit` 使用语义化编辑请求，例如 `move_point`、`set_constraint`、`add_survey_area`，而不是暴露某个画布的鼠标事件或控件字段。所有结果使用统一 diagnostics 结构，并包含 `schema_version`。新增字段必须保持向后兼容，破坏性语义变化必须升级 schema 或写 ADR。

## 与现有模块的关系

- `MissionManager` 继续管理 `MissionPlan` 的编辑、统计、导入导出和 MAVLink 兼容性。
- `SurveyCalculator` 逐步成为 `survey_area` 编译器的内部算法复用点，不能在前端复制同类算法。
- 任务上传、下载、进度、超时和重试继续由现有 MAVLink 任务传输链路负责。
- 固件特定行为只从 `firmware profile` 和 capabilities 获取；Mission CAD 前端不得以参数名或机型字符串猜测飞行行为。

## 实施里程碑

1. 在 `core/mission/cad` 建立数据模型、ENU/WGS84 换算、JSON 读写和离线测试。
2. 实现点、航段、距离/方位约束，以及确定性编译和 FFI 预览。
3. 将 `SurveyCalculator` 适配到 `survey_area`，补齐源到任务项映射与估算。
4. 加入 Copter/Plane profile 的转弯、速度、爬升和任务上限校验。
5. 分别在桌面、移动、Web 或 CLI 中实现编辑器；它们共享同一份文档与编译契约。

每个里程碑都必须有不依赖 UI 和真实飞控的离线 fixture 测试。
