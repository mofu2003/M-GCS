# 功能简报：测绘与地形（Survey 与 Terrain）

状态：方向已立项。**S1（地形缓存 + provider 接入）、S2（测绘生成进 UI）、S3a（任务文件导出进 UI）与 S3b（terrain following / AGL 高度写入任务）已实现并验证**；方向 2 的四个切片（S1/S2/S3a/S3b）全部完成，剩余差距为 D3（磁盘瓦片持久化）、D4（`TERRAIN_DATA` 回发）、D7（地形页面入口）
日期：2026-10-11（S1/S2/S3a/S3b 更新 2026-10-11）
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 3 第 2 条；`docs/capabilities.md` 对 `ffi_api_environment.h` 的「内部能力」说明。

本文件是阶段 3「能力扩展，逐项立项」的第二个方向。上游源码事实来自本地快照（QGC `bf0096d97`、MP `a2fcd74d6`，见 `docs/constraints/parity.md` 参考版本表）。

## 1. 目的

把 M-GCS 里已有的「测绘网格」与「地形高程」两套原生骨架，变成一条 ArduPilot 上可信、可验收的用户流程，而不是继续放着占位 API：

- **测绘**：定义一块区域（矩形或走廊）→ 生成可上载的测绘航点 → 估算飞行时间/距离/照片数 → 导出成常用格式。
- **地形**：给地形跟随和「离地高度（AGL）」规划提供真实高程数据，并让这份数据可离线复用、失效可解释。

目标不是一次性对齐 QGC 的全部复杂项（任意多边形 survey、结构扫描、相机预设库）或 MP 的完整 Grid 工具，而是先打通一条 ArduPilot 关键流程，并在文档里写明剩余差距。

## 2. 范围

- 范围内：矩形/走廊测绘的相机模型、GSD/重叠/线间距计算、结果生成与估算；地形高程 provider 接入、批量查询与缓存；地形跟随航线的 AGL 高度；结果导出（复用现有 complex_items / waypoints，加 KML/CSV）。
- 范围外：任意多边形 survey 与自定义边界、交叉/结构扫描的完整复杂项模型、相机预设 XML 库、影像拼接与正射、DEM 瓦片地图渲染、自建地形数据分发。

## 3. M-GCS 现状

测绘在 S2 已接到前端页面（配置工作区「测绘规划」页）；地形在 S1 已打通到前端 backend 与在线 provider（尚未进页面）：

| 层 | 位置 | 状态 |
|---|---|---|
| 测绘计算 | `core/include/core/mission/survey_calculator.h`、`core/src/mission/survey_calculator.cc` | 已实现：`CameraSpec`、`SurveyConfig`、`CorridorConfig`、`SurveyResult`，以及 `generate_grid`/`generate_cross_grid`/`generate_corridor_scan`、GSD/重叠/触发距离计算 |
| 测绘 FFI | `bridge/ffi/ffi_api_mission.h:100-101`、`bridge/ffi/ffi_survey_commands.cc` | 已导出：`mg_survey_generate`、`mg_survey_generate_corridor`；JSON 配置进，JSON 结果出（`valid`/`waypoint_count`/`flight_line_count`/`total_distance_m`/`estimated_time_s`/`area_m2`/`photo_count`/`waypoints`） |
| 地形存储 | `core/include/core/terrain/terrain_store.h`、`core/src/terrain/terrain_store.cc` | **S1 已实现**：`TerrainCache`（内存、0.01° 网格键、批量淘汰）+ `TerrainStore`（`store`/`store_batch`/`lookup`/`sample_line`/`clear`/`hit_rate`）。**core 不内置 HTTP 客户端**：高程由前端 provider 拉取后写入缓存 |
| 地形 FFI | `bridge/ffi/ffi_api_environment.h:10-17`、`bridge/ffi/ffi_terrain_commands.cc` | 已导出：`mg_terrain_query`、`mg_terrain_query_line`、`mg_terrain_cache_update`、`mg_terrain_cache_clear`；缓存挂在 `mg_core_t` 上，跨调用共享 |
| 导出 | `bridge/ffi/ffi_api_mission.h:92`、`bridge/ffi/ffi_mission_interchange.cc` | 已有：`mg_mission_doc_export_format`（`waypoints`/`plan`/`qgc_plan`），S3a 已接前端「导出文件」弹层 |
| 前端 | `flutter_shell/lib/survey/`、`flutter_shell/lib/terrain/`、`flutter_shell/lib/mission/` | 测绘已接入：`survey_ffi_backend.dart`（绑定 `mg_survey_generate*`）+ `survey_controller.dart`（生成 → 并入任务文档，S3b 支持 `frame` 覆盖）+ `survey_planning_page.dart`（配置工作区「测绘规划」页，三模式 + 高度基准 + 预览统计）。任务编辑（`mission_planning_panel.dart`）支持选择高度基准帧（S3b）。地形已接入 backend 与在线 provider（`terrain_ffi_backend.dart`/`elevation_provider.dart`/`terrain_controller.dart`），仍未进页面 |
| 测试 | `core/tests/test_sim_survey.cc`、`core/tests/test_terrain.cc`、`flutter_shell/test/survey_controller_test.dart`、`flutter_shell/test/survey_planning_page_test.dart`、`flutter_shell/test/terrain_controller_test.dart` | native 是纯计算测试；前端测绘覆盖生成传参、无效结果、并入任务、帧覆盖（S3b）与 JSON 契约 + 页面用例（含 AGL 联动）；地形覆盖缓存命中/未命中、剖面缺值、清缓存与 FFI 数据契约。帧 10 的编辑弹层写入为 analyze 覆盖，缺无线程内可注入 native 的组件测试（见未决问题） |
| MAVLink 地形协议 | `core`、`bridge` | **完全缺失**：`rg "TERRAIN_REQUEST|TERRAIN_DATA|TERRAIN_REPORT"` 无命中 |

`docs/capabilities.md` 把 `ffi_api_environment.h`（7 导出 / 4 接入）标为「部分接入」：地形四个导出已接前端 backend，SITL 仍是内部能力。`ffi_api_mission.h` 标为「已交付」；survey 生成子能力（`mg_survey_generate*`）在 S2 已进「测绘规划」页。

已发现的真实问题（S1 修复 P1–P3；P4–P6 仍待 S2/S3）：

- **P1（已修）地形 provider 曾经是桩**，恒返回 `-1001.0f`。现由前端 `OpenElevationProvider` 通过 HTTP 批量拉取真实高程，core 只做缓存与采样。
- **P2（已修）批量查询曾经逐点串行**。现由前端一次性提交缺值点列表（按 `batchSize` 分批），请求数与点数成批而非逐点。
- **P3（已修）缓存曾经不跨调用、不落盘**。`TerrainStore` 挂在 `mg_core_t` 上，进程内跨 FFI 调用共享。**磁盘持久化仍未做**（D3 未决），这是有意保留的差距。
- **P4 无 4×4 TERRAIN_DATA 回发（S3b 后仍开放）。** S3b 已让前端可把任务写成 `MAV_FRAME_GLOBAL_TERRAIN_ALT`（帧 10），地形跟随由**飞控**按自身地形数据执行；core/bridge 仍没有 `TERRAIN_REQUEST`/`TERRAIN_DATA` 处理，M-GCS 不会替飞控回发地形网格（QGC/MP 都有，12 Hz）。这是有意保留的差距（D4）。
- **P5 survey 只覆盖矩形/走廊。** 没有任意多边形、入口点、转弯半径、可编辑复杂项；生成结果是一次性 `MissionItem` 列表，编进任务文档后不能像 QGC 那样再参数化重算。
- **P6（S3a 部分修复）导出只覆盖任务文件。** S3a 已把 `mg_mission_doc_export_format` 接进 UI，支持导出 `waypoints`（编译后任务项）与 QGC `.plan`（保留复杂项）；KML/CSV（MP `writeKML`）与「把 survey 直接存成 QGC `.plan` 复杂项」仍未做（S3b 不改变这一项）。

## 4. 上游行为研究

### QGroundControl（Apache-2.0 / GPL 双许可；参考提交 `bf0096d97`）

- **Survey 复杂项**：`src/MissionManager/SurveyComplexItem.cc`（972 行）与共同基类 `TransectStyleComplexItem.cc`（1468 行）。生成流程：先算 transects（`_rebuildTransectsPhase1WorkerSinglePolygon`，`SurveyComplexItem.cc:632`），再与多边形求交（`_intersectLinesWithPolygon`，`:524`），调整入口点（`_adjustTransectsToEntryPointLocation`，`:424`），按最短距离优化顺序（`_optimizeTransectsForShortestDistance`，`:372`）；相机拍照点由 `_recalcCameraShots`（`:866`）计算。
- **相机模型**：`CameraCalc.cc`、`CameraSpec.cc`，相机高度模式含 `AltitudeFrameCalcAboveTerrain`（离地高度规划）。
- **地形跟随**：`TransectStyleComplexItem.cc:29-34` 提供 `terrainAdjustTolerance`、`terrainAdjustMaxClimbRate`、`terrainAdjustMaxDescentRate` 三个可调量；`recalcTerrainProfile`（`:88`）拉地形并重算航点高度；`_jsonTerrainFlightSpeed`（`:155-159`）写进 `.plan`。
- **地形查询**：`src/Terrain/TerrainTileManager.cc`（493 行，tile 缓存 + 磁盘持久化）、`TerrainQuery.cc`、`TerrainQueryInterface.h`（查询模式 `QueryModeCoordinates`/`Path`/`Carpet`）、`Providers/TerrainTileCopernicus.{h,cc}`。provider URL 是 `https://terrain-ce.suite.auterion.com`（`QtLocationPlugin/Providers/ElevationMapProvider.h:46`），tile 0.01°、1 arc-second、约 30 m 间距（`TerrainTileCopernicus.h:23,25`）。
- **地形协议**：`src/Vehicle/TerrainProtocolHandler.cc`（184 行），12 Hz 定时器（`1000.0/12.0`，`:22`），每个 `TERRAIN_DATA` 装 4×4 高度网格（`:99`）。
- **现实约束**：本轮实测 `https://terrain-ce.suite.auterion.com` 只对 `carpet` 端点返回 200；`/api/v1`（coordinates）与 `/path` 返回 501。QGC 的默认 provider 当前对一个第三方客户端并不可直接使用，M-GCS 不能照抄这个 URL。

### Mission Planner（GPLv3，**只参考行为，禁止复制源码**；参考提交 `a2fcd74d6`）

- **Grid 插件**：`ExtLibs/MissionPlanner.Gridv2/GridUIv2.cs`（1083 行）、`ExtLibs/SimpleGrid/SimpleGridUI.cs`（475 行）。相机库来自 `camerasBuiltin.xml`/`cameras.xml`（`GridUIv2.cs:819,821`）。KML 导出走 `MainV2.instance.FlightPlanner.writeKML()`（`:542`）。
- **地形跟随**：`ExtLibs/Utilities/TerrainFollow.cs`（166 行）订阅 `OnPacketReceived` 的 `TERRAIN_REQUEST`/`TERRAIN_REPORT`，`QueueSendGrid`（`:84`）对 56 个 bit 逐个发 4×4 网格，`Thread.Sleep(1000/12)`（`:113`）限速 12 Hz，`SendGrid`（`:127`）用 `srtm.getAltitude` 取值，无效数据直接 `return` 不发。
- **高程来源**：本地 DTED/GeoTIFF 瓦片（`ExtLibs/Utilities/DTED.cs`、`GeoTiff.cs`，目录来自 `n.datadirectory`），不依赖在线 API。

### 可选高程 provider（本轮实测）

- `https://api.open-elevation.com/api/v1/lookup?locations=39.9,116.4` → **200**，返回 `{"results":[{...,"elevation":52.42}]}`。M-GCS 已把它作为 `OpenElevationProvider`（`flutter_shell/lib/terrain/elevation_provider.dart`）的默认端点。
- `https://api.opentopodata.org/v1/srtm90m?locations=39.9,116.4` → 200，返回 SRTM 90 m 数据；但 `copernicus-dem-30m` 数据集不存在（返回 `Dataset ... not in config`）。
- 结论：可选 provider 不止一个，但都要在实现前确认服务条款、限流与离线降级。

## 5. 差距与决策点

| # | 差距 | 决策点 |
|---|---|---|
| D1 | 地形 provider（P1） | **S1 已决定**：默认 open-elevation、端点可配置；服务条款/限流仍需在正式发布前确认，必要时换自建 opentopodata |
| D2 | 逐点同步 HTTP（P2） | **S1 已决定**：缺值点按 `batchSize` 分批一次性提交（默认 100 点/批、20 s 超时） |
| D3 | 缓存（P3） | **S1 已部分决定**：进程内共享缓存已做（挂 `mg_core_t`）；磁盘瓦片持久化未做，配额与失效策略待定 |
| D4 | 无 TERRAIN_REQUEST 回发（P4） | 是否实现 4×4 网格回发与 12 Hz 限速；先只做查询，还是查询与协议一起做 |
| D5 | survey 只覆盖矩形/走廊（P5） | **S2 已决定**：本轮只交付矩形/交叉/走廊三模式，生成「一次性航点」并入任务文档；任意多边形与可编辑复杂项留待后续 |
| D6 | 无导出（P6） | **S3a 已决定**：先交付 `waypoints`（编译后任务项）与 QGC `.plan`（保留复杂项）两种导出；KML/CSV 未做，留待后续 |
| D7 | 前端零绑定 | **S2 已决定**：新增配置工作区独立「测绘规划」页（设置菜单入口），不并入任务面板 |

## 6. 交付切片

- **S1 地形查询可用（已完成）**：provider 接入（`OpenElevationProvider`，端点可配置）、批量 HTTP 查询、`mg_core_t` 上的进程内共享缓存、`mg_terrain_query_line` 返回真实高程；provider 不可达时返回明确错误（`ElevationProviderException`），而不是静默无效。
  - 交付物：`core/terrain/terrain_store.{h,cc}`、`ffi_terrain_commands.cc` 的 `mg_terrain_cache_update`/`mg_terrain_cache_clear`、`flutter_shell/lib/terrain/*`。
  - 设计取舍：**core 不内置 HTTPS 客户端**——macOS/Linux/Windows/Android 的 TLS 方案各异，塞进 core 会新增平台依赖（`architecture.md` A6）。因此「网络在 Flutter 层、缓存与采样在 core」，provider 可替换。
  - 未做：磁盘瓦片持久化（D3）、4×4 `TERRAIN_DATA` 回发（D4）、页面入口（D7）。
- **S2 测绘生成进 UI（已完成）**：配置工作区新增「测绘规划」页（设置菜单入口），三模式（矩形网格 / 交叉网格 / 走廊扫描）配置区域与航线、相机与重叠参数，调用 `mg_survey_generate` / `mg_survey_generate_corridor`，展示航点数/航线数/航程/预计用时/覆盖面积/拍照数预览，并把结果并入任务文档。
  - 交付物：`flutter_shell/lib/survey/{survey_models,survey_backend,survey_ffi_backend,survey_controller,survey_planning_page}.dart`；`connection_controller.dart` 暴露 `surveyController`；入口 `widgets/mg_settings_menu.dart`、`configuration/configuration_workspace.dart`。
  - 设计取舍：**生成「一次性航点」并入任务文档**，不做 QGC 式可编辑复杂项（D5）；**页面不持有 `dart:ffi`**，只经 `ConnectionController`（架构 A5）。`SurveyController.applyToMission` 在 `replaceDocument` 后显式复查 `mission.error` 与实体数增量，避免编辑失败被 `MissionDocumentController` 收进 `error` 后仍被当作成功。
  - 未做：地图上叠加显示航线（本页只做统计预览 + 并入任务；最终航线上传与 Home 确认仍在航线规划页）；自动上传（并入后由用户在航线规划页确认 Home 再上传）；KML/CSV 导出（D6 剩余部分，留待后续议题，S3b 未覆盖）。
- **S3a 任务文件导出进 UI（已完成）**：任务规划面板新增「导出文件」按钮，弹层可选 `waypoints`（`.waypoints`，编译后任务项）或 QGC `.plan`（`.plan`，保留复杂项），复用已有 native `mg_mission_doc_export_format`，写盘到用户可编辑路径（默认 `getApplicationSupportDirectory()/missions/`）。
  - 交付物：`flutter_shell/lib/mission/mission_export.dart`（弹层 + 格式枚举 + 结果模型）、`mission_document_models.dart` 的 `MissionInterchangeExport`、`mission_document_backend.dart`/`mission_document_ffi_backend.dart` 的 `exportMissionInterchange`、`mission_document_controller.dart` 的 `exportInterchange`、`mission_planning_panel.dart` 的按钮与提示；测试 `flutter_shell/test/mission_export_test.dart`（5 用例）与 `mission_document_controller_test.dart` 的导出转发用例。
  - 设计取舍：**导出不改任务文档**（不触碰 dirty/revision）；**不引入文件选择器插件**（pubspec 无），与 `logging/log_controller.dart` 一致，用文本路径 + 默认目录；`plan` 保留复杂项设计意图，`waypoints` 只导出编译后任务项。
  - 未做：KML/CSV 导出（MP `writeKML`）；`mg_mission_doc_import_format` 的前端绑定（导入侧）。
- **S3b terrain following / AGL 高度写入任务（已完成）**：任务编辑新增「高度基准」选择（相对高度 Home / 地形跟随 AGL），测绘页在选择基准后「并入任务」时把整条航线统一写成所选帧。
  - 交付物：`mission_document_models.dart` 的 `kMissionFrameRelativeAlt`/`kMissionFrameTerrainAlt`/`missionFrameLabel`/`kMissionFrameChoices`；`mission_planning_panel.dart` 的实体编辑弹层「高度基准」+ 任务设置「默认高度基准」；`survey_models.dart` 的 `toMissionEntity({int? frame})`、`survey_controller.dart` 的 `entitiesFor(..., {int? frame})`/`applyToMission({..., int? frame})`、`survey_planning_page.dart` 的「高度基准」行；测试 `survey_controller_test.dart`（帧覆盖 + 常量）、`survey_planning_page_test.dart`（AGL 联动）。
  - 设计取舍：**只写帧，不做地形高度换算**——选 AGL 时写入的是用户在「航高」里填的离地数值，`param7` 由飞控按地形数据解释执行（与 QGC `SimpleMissionItem.cc:74,737` 把 `AltitudeFrameTerrain` 映射到 `MAV_FRAME_GLOBAL_TERRAIN_ALT` 的做法一致）。**M-GCS 不回发地形网格**（D4 仍不做），因此弹层/页面文案明确提示「需要飞控载有该区域地形数据」。帧 10 在 core 侧早已合法（`mission_validation.cc` 允许 0/2/3/10，`mission_compile.cc`/`mission_waypoints.cc`/`mission_qgc_plan.cc` 原样透传），S3b 只补齐前端选择与应用。
  - 未做：`TERRAIN_REQUEST`/`TERRAIN_DATA` 回发（D4）；磁盘瓦片持久化（D3）；地形页面入口（D7）。

## 7. 验收标准

- 测绘（S2 已满足「生成 → 预览 → 并入任务文档」，自动上载与地图叠加为后续项）：矩形、交叉网格、走廊各完成一次「生成 → 预览 → 并入任务文档」；`waypoint_count`/`flight_line_count`/`total_distance_m`/`estimated_time_s`/`photo_count` 与手算或 QGC 同参对照在容差内。
- 地形（S1 已满足）：`mg_terrain_query`/`mg_terrain_query_line` 返回真实高程（非 `-1001`）并有自动化测试（`core/tests/test_terrain.cc`、`flutter_shell/test/terrain_controller_test.dart`）；provider 不可达时返回明确错误，而不是静默无效。
- 若实现 TERRAIN_REQUEST 回发：12 Hz 限速、无效数据不发送、4×4 网格布局与 MAVLink 定义一致，并有 fixture 测试。
- 自动化：新增 native 测试（provider mock）+ Flutter widget/controller 测试；`tools/preflight.sh` 通过。

## 8. 平台

- 测绘计算与导出是纯逻辑，全平台（macOS/Linux/Windows）一致。
- 地形 provider 依赖网络：需定义离线降级（缓存命中用缓存，未命中明确报错）。
- 前端 UI 遵循现有 Flutter 跨平台约束。

## 9. 许可与外部依赖

- **QGC**：Apache-2.0 / GPL 双许可。引用 QGC 代码需先记 ADR（`docs/constraints/upstream.md` P4）；本方向以行为对齐为主，不复制源码。
- **MP**：GPLv3（`COPYING.txt`）。**禁止复制源码**，只参考行为与协议事实。
- **MAVLink**：`TERRAIN_*` 消息使用官方定义（`upstream.md` P5）。
- **高程服务**：open-elevation / opentopodata 是第三方免费服务，使用前确认服务条款与限流；文档写清 provider 可配置与离线降级。
- **参考源码版本**：QGC `bf0096d97`、MP `a2fcd74d6`（`parity.md` 参考版本表）。

## 10. 未决问题

- D1（部分决定）provider 默认 open-elevation，端点可配置；服务条款/限流仍需在正式发布前确认，必要时换成自建 opentopodata。
- D3 是否做磁盘瓦片持久化（QGC 有，M-GCS 目前无）。
- D4 是否在本方向就实现 TERRAIN_REQUEST 回发。
- D5（S2 已决定）：生成一次性航点并入任务文档；可编辑复杂项与任意多边形留待后续。
- D6（S3a 已决定）导出先交付 `waypoints` 与 QGC `.plan` 两种；KML/CSV 未做。
- 真实硬件验收：地形跟随需要真机或带地形的 SITL 验证，SITL 是否覆盖待确认。
- 测试覆盖：任务规划面板的实体编辑弹层（选 AGL 后写入帧 10）目前只有 `flutter analyze` 覆盖，缺可注入 native 的组件测试；测绘页与 `SurveyController` 的帧覆盖已有自动化用例。
