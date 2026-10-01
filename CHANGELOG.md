# Changelog

## 0.0.58 - 2026-10-02

### Fixed

- 任务回读核对不再把 ArduPilot 的位置量化误差当成差异：`x`/`y` 以前用 degE7 整数
  精确比较，完全没用 `tolerance`；而飞控内部把经纬度存成 float 度，往返会带来
  约 0.1–0.5 m 误差，导致几乎每次上传都报“回读发现差异”。现在按当地尺度把米制
  容差换算成 degE7 阈值再比较（纬度 111320 m/度，经度按 cos(lat) 折算）。
- 非位置命令（DO_/CONDITION_ 等）的坐标帧差异归入 `normalizations`：飞控对这类
  命令统一按 frame 0 存，我们文档会带默认帧 3，属等价表示，不再报成差异。
- Flutter 侧默认核对容差从 0.1 m 调整为 1.0 m，与 ArduPilot 的存储精度匹配。

### Verification

- 用本地 ArduPlane SITL（UDP）实测：同一份 4 项任务，0.1 m 容差报 4 处差异，
  1.0 m 容差 `matches:true`、0 处差异。
- 新增 `test_sim_mission_document` 用例，模拟飞控侧 float 量化：1 m 容差判定一致、
  0.05 m 容差仍能报出量化差异。
- 后端 offline/simulation `35/35` 通过；Flutter 测试 `22/22` 通过；
  UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.57 - 2026-10-01

### Fixed

- 地图首次拿到有效定位时把视角拉回飞机：地图中心是跨会话保存的，上次可能停在默认的
  上海，而飞机在几千公里外，连上了却看不到飞机。现在首次定位与当前中心距离超过 20 km
  时自动移动到飞机（低于 20 km 不动用户视角，每个会话只做一次）。

### Verification

- 用本地 ArduPlane SITL（UDP 14550）实测：GCS 上报 `lat -35.363261 / lon 149.16523`、
  Home 同为 `-35.363261 / 149.16523`，与 SITL 设定完全一致，航向 353°、空速 0.6–0.9 m/s，
  坐标链路无偏移。
- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.56 - 2026-10-01

### Changed

- 加大航线规划面板的按钮：任务动作行（从飞控读取 / 上传到飞控 / 启动任务 / 取消）、
  围栏与 Rally 操作组、Home 卡片、核对面板的按钮从 `ControlSize.small` 提升到
  `ControlSize.regular`，与连接窗口保持一致。
- 任务动作行由 `Row` 改为 `Wrap`，按钮变大后在窄面板里自动换行，避免溢出。

### Verification

- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查和 macOS debug 构建通过；完整 `tools/preflight.sh` 通过。

## 0.0.55 - 2026-10-01

### Added

- 新增 FFI `mg_mission_get_execution_progress`：报告任务执行进度
  （`valid` / `current_index` / `current_seq` / `total` / `reached_count` /
  `last_reached_seq` / `finished`）。`current_index` 已去掉 ArduPilot home 偏移。
- 适配器开始统计 `MISSION_ITEM_REACHED`：按 seq 去重累计已到达项，最后一项到达即
  `finished`；任务下载/上传会重置统计。此前 `MISSION_ITEM_REACHED` 只透传回调，
  前端无法判断任务是否跑完。
- Flutter 侧任务面板新增执行进度条（当前第 N/共 M 项、已到达数、完成状态），
  由 `MissionDocumentController.refreshExecutionProgress` 每秒轮询；
  面板只在显示时挂载，隐藏即停止轮询。

### Verification

- 后端 offline/simulation `35/35` 通过（`test_sim_mission` 新增 5 条进度断言）。
- Flutter analyze 无问题，Flutter 测试 `22/22` 通过。
- UI token 检查通过；完整 `tools/preflight.sh` 通过。

## 0.0.54 - 2026-10-01

### Added

- `mg_operations_get_snapshot` 新增 `fence` 和 `rally` 区段（`type` 分别为
  `fence_transfer` / `rally_transfer`），字段与 `mission` 同构
  （`state`/`progress_pct`/`received`/`sent`/`total`/`result`/`error`）。此前围栏和
  Rally 传输没有进度来源，前端无法等待完成。
- Flutter 侧完成对接：`MissionDocumentBackend` 新增围栏/Rally 的
  compile/upload/download/import/verify/clear/进度查询接口，
  `MissionDocumentFfiBackend` 绑定对应 FFI，`MissionDocumentController` 新增
  `uploadFence`/`readFenceFromVehicle`/`verifyFence`/`clearFence` 及 Rally 同名流程，
  任务面板“设置”标签页新增围栏/Rally 操作组（读取 / 下发 / 清除，下发自带回读核对）。
- `MissionVerificationDifference` 新增 `area` 字段，承载围栏/Rally 核对返回的区段名。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 无问题，Flutter 测试 `21/21` 通过（新增围栏/Rally 流程用例）。
- UI token 检查和 macOS debug 构建通过；bundle 内 `libmgcore.dylib` 含全部 300 个
  `mg_*` 导出符号（含新增围栏/Rally 接口）。
- 完整 `tools/preflight.sh` 通过。

## 0.0.53 - 2026-10-01

### Changed

- QGC ComplexItem 现在可以编译：复用 QGC 保存在 `TransectStyleComplexItem.Items`
  里的已算好任务项，而不是在 M-GCS 重新推导 Survey/Corridor 几何（参照 QGC
  `TransectStyleComplexItem::_save`）。新增 `expand_complex_item_entities` 和
  `expand_document_entities`，编译与估算都会按 `entity_insert_index` 把复杂项展开到
  正确位置。
- 校验层不再对复杂项一刀切报错：`TransectStyleComplexItem.Items` 缺失的复杂项继续以
  `complex_item.unsupported` 阻断上传；可展开的复杂项改为 `complex_item.expanded`
  警告（几何来自 QGC、在 M-GCS 中只读），展开出的未知 MAV_CMD 报
  `complex_item.unsupported_command`。含可展开复杂项的任务现在可以正常校验、编译和上传。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.52 - 2026-10-01

### Added

- 新增 FFI `mg_mission_doc_verify_fence` / `mg_mission_doc_verify_rally`，与
  `mg_mission_doc_verify` 对称：在一次已完成的对应下载后比较文档与飞控的围栏/Rally，
  返回 `matches`、`difference_count` 和结构化 `differences`
  （`area` / `index` / `field` / `expected` / `actual`），支持容差。
- 围栏比较覆盖多边形数量、顶点数、inclusion、每个顶点经纬度、圆形数量/圆心/半径以及
  breach-return 点与高度；Rally 覆盖点数、经纬度和高度。
- `test_sim_fence_rally` 扩展为上传 → 下载 → 回读 → verify 的完整闭环，并验证清空飞控
  围栏后 verify 会报告差异（29 项断言）。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.51 - 2026-10-01

### Fixed

- 修复围栏/Rally 下载时 mission type 被写死为 0 的适配器 bug：`send_mission_request_int`
  和 `send_mission_ack` 现在携带实际 mission type，围栏/Rally 下载不再错走普通任务状态机。
  该问题由新增的端到端模拟测试发现。

### Added

- 新增 `geo_fence_from_plan` / `rally_points_from_plan`：`compile_geo_fence_plan` 和
  `compile_rally_plan` 的逆过程。围栏分组规则与 QGC `GeoFenceManager` 一致
  （相邻且 `param1` 顶点数和命令相同的顶点属于同一多边形），非法计划会被拒绝。
- 新增 FFI `mg_mission_doc_import_fence_from_vehicle` / `mg_mission_doc_import_rally_from_vehicle`：
  在一次已完成下载后把飞控围栏/Rally 读回文档，语义与 `mg_mission_doc_import_from_vehicle`
  一致（未完成下载返回 `invalid_state`）。适配器新增 `fence_get_plan_snapshot` /
  `rally_get_plan_snapshot` 提供 provenance。
- 端到端模拟测试扩展为围栏/Rally 上传 + 下载 + 回读文档的完整闭环（25 项断言）。

### Verification

- 后端 offline/simulation `35/35` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.50 - 2026-10-01

### Added

- 新增模拟测试 `test_sim_fence_rally`：mock 飞控现在按 mission type 分库存放任务项
  （`MAV_MISSION_TYPE_FENCE` / `MAV_MISSION_TYPE_RALLY`），并在 mission count、
  request、ack、clear 中回显请求的 mission type，使围栏/Rally 上传可以端到端验证。
- 测试覆盖：文档围栏经 `mg_mission_doc_upload_fence` 下发为 5 个 fence 任务项
  （多边形顶点带 `param1` 顶点数、圆形带半径、breach-return 带高度），
  `mg_mission_doc_upload_rally` 下发 2 个 `NAV_RALLY_POINT`，
  `mg_mission_doc_clear_fence` 清空飞控围栏。

### Verification

- 后端 offline/simulation `35/35` 通过（新增 1 个 simulation 测试）。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.49 - 2026-10-01

### Added

- 新增 `compile_rally_plan`：把文档 `rally_points` 编译成 MAVLink rally mission type
  任务项，编码与 QGC `RallyPointManager` 对齐（`MAV_CMD_NAV_RALLY_POINT` 5100、
  `MAV_FRAME_GLOBAL_RELATIVE_ALT`、`param5/6/7` 为纬度/经度/高度、`autocontinue=0`）。
- 新增 FFI `mg_mission_doc_compile_rally`、`mg_mission_doc_upload_rally` 和
  `mg_mission_doc_clear_rally`，把文档 Rally 点接到既有 rally mission 传输上（Rally 是
  独立 MAVLink mission type，不随任务上传）。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.48 - 2026-10-01

### Added

- 新增 `compile_geo_fence_plan`：把文档 `geo_fence` 编译成 MAVLink fence mission
  type 的任务项，编码与 QGC `GeoFenceManager` 对齐（多边形用 5001/5002 且 `param1`
  为顶点总数，圆形用 5003/5004 且 `param1` 为半径，breach-return 用 5000 +
  `MAV_FRAME_GLOBAL_RELATIVE_ALT`），顺序为多边形 → 圆形 → breach-return。
- 新增 FFI `mg_mission_doc_compile_fence`、`mg_mission_doc_upload_fence` 和
  `mg_mission_doc_clear_fence`，把文档围栏接到既有 fence mission 传输上（围栏是独立
  MAVLink mission type，不随任务上传）。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.47 - 2026-10-01

### Added

- 任务校验层补齐 GeoFence 与 Rally 几何校验：Rally 点经纬度和高度、围栏多边形顶点
  数量和经纬度范围、圆形围栏圆心和半径、breach-return 点及高度都会检查，分别返回
  `rally.*` / `geo_fence.*` 错误码。
- 围栏与 Rally 属于独立 MAVLink mission type，仍作为合法文档保留（不阻断任务上传），
  与阻断上传的 `complex_item.unsupported` 区分开。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.46 - 2026-10-01

### Fixed

- 修复含 QGC ComplexItem 的任务文档在上传时静默丢弃复杂项的安全隐患：校验层新增
  `complex_item.unsupported` 错误，文档带复杂项时 `validate`/`compile`/上传/回读
  都会失败，并提示先在 QGroundControl 处理或移除复杂项。此前编译只展开普通实体，
  上传后飞控任务会与导入的 `.plan` 不一致且没有任何提示。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.45 - 2026-10-01

### Changed

- `MissionComplexItem` 新增 `entity_insert_index`，记录复杂项在任务序列中相对普通航点的
  插入位置；任务文档 JSON 一并保存和恢复（缺失时按“末尾”兼容旧文档）。
- QGC `.plan` 导出不再把复杂项固定追加到末尾，而是按插入位置与 SimpleItem 交错输出，
  保持 Survey/Corridor 与航点的原始先后顺序。
- `doJumpId` 作为派生字段在导出时对简单项和复杂项统一重编号，避免混排后出现重复序号。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.44 - 2026-10-01

### Changed

- QGC ComplexItem（Survey、CorridorScan 等）从 opaque `extensions` 提升为正式
  `MissionComplexItem` 文档模型，保存 `id`、`item_type` 和原始 JSON 载荷。
- QGC `.plan` 导入会把 `type != SimpleItem` 的条目按 `complexItemType`（回退
  `type`）识别为 complex item；导出时重新生成完整的 QGC ComplexItem JSON，
  确保 Survey/Corridor 等复杂任务往返无损。
- 复杂任务的导入告警改为“只读保留、暂不可语义化编辑”，明确当前能力边界。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.43 - 2026-10-01

### Changed

- GeoFence 从 opaque 区段提升为正式文档模型，支持 polygon、circle、inclusion 和
  breach-return。
- QGC `.plan` GeoFence version 2 会映射为正式模型，导出时重新生成
  `polygons`、`circles` 和 `breachReturn`；version 1 继续忽略并告警。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.42 - 2026-10-01

### Changed

- Rally 不再只保存在 opaque `extensions` 中：任务文档新增正式 `rally_points` 模型，
  支持经纬度和高度，并随任务文档 JSON 一起保存和恢复。
- QGC `.plan` 的 `rallyPoints` version 2 会映射到正式 Rally 模型，导出时重新生成
  QGC 点数组；旧 version 1 继续忽略并给出 warning。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.41 - 2026-10-01

### Changed

- `MissionDocument` 新增 `extensions` 区，用于保存任务文档本身不理解的格式专属 JSON。
- QGC `.plan` 导入不再拒绝 ComplexItem、非空 GeoFence 或 Rally；这些区段会原样保留，
  并在再次导出 `.plan` 时放回，同时通过 warnings 明确说明它们仍是 opaque 数据。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.40 - 2026-10-01

### Changed

- 参数 profile 识别补齐 Rover、Submarine 和 AntennaTracker，分别映射到
  `rover`、`sub`、`tracker`，不再返回空 profile。
- 这些 profile 当前回退到通用描述库；后续可独立补充机型专属参数元数据，覆盖报告
  和分组接口会使用正确的 profile 名称。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.39 - 2026-10-01

### Changed

- 根据覆盖率报告补齐高频 ArduPilot 参数基础元数据：旧版 `WPNAV_SPEED`、
  `WPNAV_SPEED_UP`、`WPNAV_RADIUS`、`PILOT_SPEED_DN`，以及 `ARMING_CHECK`、
  `THR_MID`、`TELEM1_BAUD`。
- 模拟参数集覆盖率从 77% 提升到 97%，唯一 fallback 保留为测试用未知参数
  `MGC_UNKNOWN`；不伪造缺失参数的完整范围或枚举。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.38 - 2026-10-01

### Changed

- 参数元数据覆盖报告现在按固件版本的 canonical 参数名统计，旧固件中的
  `WPNAV_*` 等名称会映射到当前描述库名称，不再被误报为缺失。
- 补充 QGC ArduPilot 4.7 常见重映射：`PILOT_SPD_DN`、`Q_PILOT_SPD_DN` 和
  `ARMING_SKIPCHK`。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.37 - 2026-10-01

### Changed

- 新增 `mg_config_get_metadata_coverage`：按当前固件/机型 profile 统计已加载参数中
  described、fallback 和覆盖率，并列出缺少本地描述的参数名。
- 覆盖报告只读，不修改参数缓存、不发送 MAVLink 请求，也不伪造缺失元数据；用于指导
  后续按真实 ArduPilot 参数页缺口补齐描述库。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.36 - 2026-10-01

### Changed

- 参数前缀刷新现在支持版本重映射反查：在旧 ArduPilot 固件上，用户请求 `WP_*` 时，
  参数缓存中的 `WPNAV_*` 会通过 canonical 名称匹配并参与刷新。
- firmware plugin 增加 `canonical_param_name`，ArduPilot 实现反向遍历版本表，PX4
  保持原名；精确名称重映射、重试和状态查询行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.35 - 2026-10-01

### Changed

- 参数页现在会把已从飞控加载、但描述库未覆盖的参数归入稳定的 `other` 分组，不再让
  这些参数从分组接口中消失。
- `other` 分组会从 MAVLink 参数类型推断基础 `int/float` 类型，保留原始参数名和实时
  值，并用 `found:false` 明确标记缺少本地描述；不伪造范围或枚举信息。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.34 - 2026-10-01

### Changed

- 参数写入和精确刷新现在会经过 ArduPilot 固件版本参数重映射：例如旧固件收到
  `WP_SPD` 时会转换为 `WPNAV_SPEED`，新固件保持原名。
- `mg_params_set`、`mg_config_write_param`、`mg_config_write_params` 和
  `mg_params_refresh` 使用重映射后的参数名查缓存并发送；未知版本、PX4 和未知参数
  保持原样。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.33 - 2026-10-01

### Changed

- 为批量参数刷新增加异步任务状态和重试：每个名称记录发送时间、重试次数和目标
  sysid/compid，1 秒无回复时最多重发 2 次，重试耗尽进入 failed。
- 新增 `mg_params_get_refresh_status`，返回 active/total/completed/pending/round/failed；
  `PARAM_VALUE` 完成对应刷新，`PARAM_ERROR` 立即失败，原 `mg_params_refresh` 响应保持兼容。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.32 - 2026-10-01

### Changed

- 新增 `mg_params_refresh`：接受 JSON 名称数组，支持精确参数名和 `PREFIX*` 前缀，
  基于当前参数缓存去重解析，并通过现有 `PARAM_REQUEST_READ` 发送刷新请求。
- 刷新响应返回 `requested`、`resolved`、`sent`、`failed`、`duplicates` 和 `unknown`；
  不自动下载整表，也不改变现有 `mg_params_request_all` 的语义。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.31 - 2026-10-01

### Changed

- 参数描述库校验现在会拒绝 `read_only` 参数；单参数写入和批量写入使用同一套
  描述库规则，覆盖 read-only、枚举、范围、bool 和缺失描述。
- `mg_config_write_params` 不再绕过元数据校验，并在原有 `written` / `failed`
  字段外增加 `errors` 数组，逐项返回参数名和拒绝原因；合法项仍会正常发送。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.30 - 2026-10-01

### Changed

- 补上阶段 D 的参数明确拒绝路径：识别标准 MAVLink `PARAM_ERROR`，在收到
  `does not exist`、`out of range`、`permission denied`、`read-only`、类型不匹配等
  错误时立即把对应 pending 写入标记为 `failed`，保留映射后的原因并移除重试队列。
- 增加协议层和参数状态机测试，覆盖 PARAM_ERROR 路由、失败终态和失败回调；
  不修改公开 FFI 字段和参数写入调用方式。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.29 - 2026-10-01

### Changed

- 完成阶段 D 的第一个参数写入增强：`PendingWrite` 记录目标 sysid/compid；
  PARAM_SET 超时后不再只增加重试计数，而是通过 `process_param_timeouts()` 真正重发，
  重试耗尽仍进入 `failed` 并保留错误原因。
- 发送 PARAM_SET 失败时立即清理 pending 状态，避免后续 PARAM_VALUE 或超时误更新同一
  写入记录；参数下载、写入历史、FFI 和 `pending / confirmed / failed` 字段保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.28 - 2026-10-01

### Changed

- 完成第二阶段 C 的收口拆分：新增 `mavlink_adapter_command.cc`、
  `mavlink_adapter_params.cc`、`mavlink_adapter_log.cc` 和
  `mavlink_adapter_dispatch.cc`，迁出命令队列发送、参数请求/超时、日志接收、
  MAG_CAL_PROGRESS 兼容解析和 `process()` 消息分发。
- `mavlink_adapter_internal.h` 从 553 行降到 290 行，现在只保留 `Impl` 状态、
  方法声明和底层 `send_msg()`；`process()` 的锁范围、消息顺序、超时检查和锁外回调
  执行语义保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.27 - 2026-10-01

### Changed

- 完成第二阶段 C 的第三项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_mission.cc`，将任务计数、任务项、任务请求、任务
  ACK、当前航点和到达通知，以及任务发送辅助函数共 15 个方法移出头文件。
- `mavlink_adapter_internal.h` 从 769 行降到 553 行；`process()` 的锁范围、消息顺序、
  任务超时检查和延迟回调执行位置未改变，任务上传/下载和 FFI 行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.26 - 2026-10-01

### Changed

- 完成第二阶段 C 的第二项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_handlers.cc`，将心跳、系统状态、GPS、IMU、姿态、
  VFR HUD、参数值、命令 ACK、校准进度、RC、Servo、位置、电池、振动、ESC、ADS-B、
  视频流以及相机/云台共 30 个消息处理器移出头文件。
- `mavlink_adapter_internal.h` 从 1269 行降到 769 行；`process()` 的锁范围、消息顺序、
  超时检查和延迟回调执行位置未改变，公开 FFI 和 MAVLink 行为保持不变。

### Verification

- 后端 offline/simulation `34/34` 通过。
- Flutter analyze 和 Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.25 - 2026-10-01

### Changed

- 完成第二阶段 C 的第一项 MAVLink 内部拆分：新增
  `core/src/mavlink/mavlink_adapter_state.h`，将遥测最新值、链路质量、日志下载和
  Command ACK 等待状态从 `mavlink_adapter_internal.h` 迁出。
- `mavlink_adapter_internal.h` 现在只保留 `Impl` 组合、协议辅助方法和消息分发入口，
  文件从 1434 行降到 1269 行；公开 FFI、MAVLink 行为、JSON 契约和 Flutter 绑定不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.24 - 2026-10-01

### Changed

- 完成第二阶段 B 的第五个内部重构：新增 `FlightCommandFfiBackend`，将 RTL、降落、
  起飞、解锁、模式切换、任务启动、引导点、暂停、环绕和设置 Home 的动态绑定与 JSON
  解包从 `ConnectionController` 迁出。
- QGC/MP 组合的飞行模式门控、解锁确认、模式心跳验证和安全敏感流程仍由
  `ConnectionController` 管理，现有飞行命令行为保持不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.23 - 2026-10-01

### Changed

- 完成第二阶段 B 的第四个内部重构：新增 `CalibrationFfiBackend`，将校准启动、取消、
  状态读取和加速度计姿态确认的动态绑定与 JSON 解包从 `ConnectionController` 迁出。
- 校准状态机、取消收敛、MAV_CAL 进度映射、六面校准流程和 UI 状态保持在主控制器中，
  校准页面行为和现有 FFI 契约不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.22 - 2026-10-01

### Changed

- 完成第二阶段 B 的第三个内部重构：新增 `ParameterFfiBackend`，将参数请求、参数快照、
  参数进度、参数描述读取和参数写入的动态绑定与 JSON 解包从 `ConnectionController`
  迁出。
- 参数缓存、元数据批处理、写入队列、写入确认和 ArduPilot 参数语义保持在主控制器中，
  参数页面行为和现有 FFI 契约不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.21 - 2026-10-01

### Changed

- 完成第二阶段 B 的第二个内部重构：新增 `TelemetryFfiBackend`，将会话状态、GPS、
  位置、Home、VFR HUD、Servo/RC、系统状态和 telemetry freshness 的动态符号绑定与
  JSON 解包从 `ConnectionController` 迁出。
- `ConnectionController` 继续负责遥测状态缓存、轮询频率和 UI 通知，遥测页面与地图
  行为保持不变。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- UI token 检查和 macOS debug 构建通过。
- 后端 offline/simulation `34/34` 通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.20 - 2026-10-01

### Changed

- 完成第二阶段 B 的第一个内部重构：新增 `MissionDocumentFfiBackend`，将任务文档
  FFI 绑定、JSON envelope、任务传输进度、回读验证和文件互操作从
  `ConnectionController` 迁出。
- `ConnectionController` 不再直接实现 `MissionDocumentBackend`，保留共享 Core、动态库
  生命周期和连接状态职责；任务页面公开契约和用户行为保持不变。
- 任务回读验证增加可接受协议归一化结果的独立记录，并继续显示真正差异的详细字段。

### Verification

- `flutter analyze lib test` 通过。
- Flutter 测试 `20/20` 通过。
- 后端 offline/simulation `34/34` 通过。
- UI token 检查和 macOS debug 构建通过。
- 完整 `tools/preflight.sh` 通过。

## 0.0.19 - 2026-09-30

### Fixed

- 磁力计校准在真实 ArduPilot 飞控上现在会按 QGC/MP 的 MAV_CAL 流程先发送
  `MAV_CMD_DO_CANCEL_MAG_CAL` 清理旧状态，再显式请求 `MAG_CAL_PROGRESS` 10 Hz 和
  `MAG_CAL_REPORT` 1 Hz，最后发送 `MAV_CMD_DO_START_MAG_CAL`。
- 修复 `CalibrationStatus.toJson()` 将 `Map<int,int>` 直接交给 `jsonEncode` 导致的
  “Converting object to an encodable object failed” 异常。此前第一次收到罗盘进度时
  状态更新就中断，所以进度条和取消后的界面一直停留在旧状态。
- 取消磁力计校准现在先立即清除前端运行态，再向飞控发送取消并停止 191/192 消息流；
  即使取消命令发送失败，本地状态也不会再卡在“校准中”。

### Verification

- 真实 ArduPilot 飞控验证：开始校准后收到连续 `MAG_CAL_PROGRESS`，进度从 0% 正常
  增长到 8%，两个罗盘分别显示；点击取消后进度条立即消失，开始按钮恢复，后端记录
  `Calibration cancelled by user`。
- 新增 Flutter 回归测试，覆盖含罗盘进度映射的 JSON 编码。
- 聚焦后端测试通过：`test_sim_calibration`、`test_sim_ui_paths`、`test_ffi_contract`。
- 完整 `tools/preflight.sh` 在提交前执行。

## 0.0.18 - 2026-09-30

### Added

- 新增 `ffi_operations.cc` 和 `ffi_operations_internal.h`，把长任务的统一快照与取消
  从系统诊断文件中分离。

### Changed

- params、mission、calibration、log 的状态快照和取消入口现在集中在一个 operations
  模块；系统诊断通过内部接口复用同一份快照。
- `ffi_diagnostics.cc` 保留连接诊断、遥测 dashboard、参数写入历史和系统诊断聚合，
  不再同时维护长任务状态语义。
- `mg_operations_get_snapshot` / `mg_operations_cancel` 的 C ABI、JSON 字段和取消
  行为保持不变，FFI 导出总数仍为 285。

### Verification

- 聚焦测试通过：`test_ffi_contract`、`test_ffi_comprehensive`、
  `test_state_machines`、`test_sim_full_flow`、`test_sim_ui_paths`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.17 - 2026-09-30

### Added

- 新增 `ffi_mission_interchange.cc` 和 `ffi_mission_shared.cc`，把任务文件
  FFI 入口与公共文档快照/校验错误序列化从任务编辑 FFI 中分离。

### Changed

- `ffi_mission_document.cc` 现在只负责任务文档创建、编辑、校验、编译、上传、
  从飞控导入、回读验证、估算和本地文档保存/读取。
- `mg_mission_doc_export_format` / `mg_mission_doc_import_format` 的实现移至独立
  互操作文件，公开 C ABI、JSON 字段、warnings 和错误结果保持不变。
- FFI 自动审计仍报告 285 个导出；前端使用 61 个，测试触达 198 个。

### Verification

- 聚焦 FFI/任务测试通过：`test_ffi_contract`、`test_ffi_comprehensive`、
  `test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.16 - 2026-09-30

### Added

- 新增 `mission_waypoints.cc` 和 `mission_qgc_plan.cc`，把 MP `.waypoints` 与
  QGroundControl `.plan` 的读写实现从通用互操作文件中分离。

### Changed

- `mission_interchange.cc` 现在只负责格式识别和按飞控/任务复杂度推荐交换格式。
- `.waypoints` 的分隔、解析、Home 行和 MAVLink 任务行编码集中在独立模块。
- `.plan` 的 SimpleItem、Home、固件/机型、速度、warnings 和错误边界集中在独立模块。
- 公开任务文件 FFI、JSON 字段、warnings 和错误文本保持不变。

### Verification

- 聚焦互操作测试通过：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.15 - 2026-09-30

### Added

- 新增 `mission_compile.cc` 和 `mission_estimate.cc`，把任务编译、Survey 展开和
  飞行估算从任务文档服务中分离。

### Changed

- Survey/Corridor 展开统一由 `expand_survey_entity` 提供；编译和估算不再各自持有
  一套调用逻辑。
- `MissionDocumentService::compile_document` 和 `estimate_document` 保留为稳定入口，
  内部委托给独立实现，任务编译结果、错误诊断和估算字段保持不变。
- `mission_document.cc` 在 0.0.14 的基础上继续缩小，当前只保留文档服务、历史、
  序列化、持久化和实体辅助逻辑。

### Verification

- 聚焦任务测试通过：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document`、`test_sim_survey`。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.14 - 2026-09-30

### Added

- 新增分阶段重构计划，记录任务、互操作、FFI、operations、参数、连接和 Flutter
  的后续拆分顺序与验收条件。

### Changed

- 将任务文档校验层从 `mission_document.cc` 拆到独立的 `mission_validation.cc`：
  文档级、任务实体、Survey/Corridor 和 MAV_CMD 参数校验现在集中在单一模块。
- `mission_document.cc` 只保留文档服务、undo/redo、revision、序列化、持久化和实体
  辅助逻辑；内部共享类型通过 `mission_document_internal.h` 暴露。
- 公开任务文档头文件、FFI 名称、JSON 字段和错误语义均未改变。

### Verification

- 任务聚焦测试：`test_bridge_mission_workflow`、`test_mission_document`、
  `test_sim_mission_document` 全部通过。
- `tools/preflight.sh` 通过：Flutter analyze/test、后端 offline/simulation 34/34、
  UI token 检查和 macOS debug 构建。

## 0.0.13 - 2026-09-30

### Added

- 统一 operations 快照和取消入口覆盖参数、任务、校准与日志。前端可通过同一套
  `mg_operations_get_snapshot` / `mg_operations_cancel` 读取进度、失败原因和终态，
  不再为每一类长任务维护互不一致的状态语义。
- 参数写入历史加入 `mg_system_get_diagnostics`，记录 requested/actual 值、pending/
  confirmed/failed 状态、错误和时间戳，便于定位参数写入未确认或失败的问题。
- QGC `.plan` 基础互操作：支持 SimpleItem、Home、巡航速度、固件/机型元数据和旧版
  `coordinate` 格式导入导出；复杂项、非空 GeoFence/Rally 和不可无损字段会明确报错
  或返回 warnings，不再静默丢弃。
- 新增任务文件格式策略和 ADR 0008：`MissionDocument` 是唯一内部主状态，ArduPilot/
  MP 默认使用 `.waypoints`，PX4/QGC 默认使用 `.plan`，复杂规划数据保留 `.plan`。
- Android USB 串口桥接改为 QGC 路线：使用 `usb-serial-for-android`、
  `SerialInputOutputManager` 和 JNI 回调接入 native `ILink`，并补充 Android APK、
  Windows 和 Linux bundle 的 CI/预检脚本。
- 新增 MP/QGC 上游实现择优准则和质量审计文档，明确按模块比较，而不是按项目整体站队。

### Changed

- 任务 ACK 错误增加 QGC 风格的稳定 `result_text`，覆盖 unsupported frame/command、
  invalid parameters、no space、denied 和 cancellation 等结果。
- 命令目录补充中文标签、实体类型和参数元数据，任务编辑界面直接从后端目录生成
  命令和参数表单。
- Flutter 任务规划、地图、连接状态和长任务交互与新的 operations/diagnostics 契约
  对齐；前端测试更新到当前 16 项。
- 文档索引、任务规划契约、架构说明和开发准则同步到当前实现，并记录 MP/QGC 的
  分项质量归属和复用边界。

### Fixed

- 参数、任务、校准和日志的取消入口现在返回真实终态；没有活动操作时不会把已完成
  的操作追认为取消。
- `.plan` 导入不会静默忽略复杂项或非空 GeoFence/Rally；无法无损表达时返回明确错误，
  可继续但会降级的字段通过 warnings 暴露。
- Android 平台数据文件通过 JNI 和 AssetManager 解压到缓存目录，native Core 不再依赖
  自定义文件描述符接管方案。

### Verification

- `tools/preflight.sh` 通过：UI token 检查、`flutter analyze lib test`、Flutter
  test 16/16、后端 offline/simulation 34/34、macOS debug 构建通过。
- `git diff --check` 通过；版本号已同步到 CMake、pubspec、README 和 CHANGELOG。
- Windows、Linux、Android 构建由 CI 路径验证；本机未替代真机串口和权限验收。

## 0.0.12 - 2026-09-30

### Added

- Mission planning UI. The route-planning panel (docked to the map's right edge,
  collapsible into an arrow button) now has two tabs: *任务* and *设置*. The task
  tab shows a Home card plus one card per mission item — numbered badge, command
  dropdown, edit / move up / move down / delete, altitude and position. The
  settings tab edits the mission-wide default altitude and speed.
- The panel has explicit *从飞控读取* and *上传到飞控* buttons, matching Mission
  Planner's Read/Write and QGroundControl's Download/Upload. Reading confirms
  first when local items would be overwritten; neither side auto-downloads.
- Clicking the map with the panel closed opens the guided-actions sheet (move to
  here, RTL, land, takeoff, pause, orbit). Availability follows QGC's
  `GuidedActionsController` gating — move-to-here and orbit require the vehicle to
  be flying, RTL additionally requires armed and not already in RTL, land requires
  armed and excludes fixed wing, takeoff is only offered while not flying, and
  every action asks for confirmation. Clicking the map with the panel open adds a
  waypoint at that position and opens the editor; tapping a waypoint marker edits
  it.
- `mg_command_set_home` (MAV_CMD_DO_SET_HOME), so the Home card can write the
  flight controller's home. Parameters match QGC's `Vehicle::doSetHome`
  (`param1=0`, `param5/6` degrees, `param7` metres); out-of-range coordinates are
  rejected locally. `API_CATALOG.md` notes that this changes the RTL reference.
- `mg_mission_doc_import_from_vehicle` reads the vehicle mission into the
  document. It requires a completed download (`invalid_state` otherwise) and
  replaces the entity list only.
- Command catalog entries now carry a Chinese `label` and an `entity_type`, so
  the UI renders the command dropdown and parameter forms straight from the
  backend instead of hard-coding MAV_CMD semantics.

### Changed

- The vehicle now requests `HOME_POSITION` and `EXTENDED_SYS_STATE` streaming at
  1 Hz once it has identified the flight controller. ArduPilot only sends Home on
  first boot and on arming, and does not send EXTENDED_SYS_STATE unless asked
  (QGC's `APMFirmwarePlugin` documents both), so without the request the map's
  Home marker stayed empty and `landed_state` was never available for guided-action
  gating.
- `MissionDocumentController` is no longer unused: the panel drives it directly.

### Fixed

- `_INT` coordinate frames (5/6/11) are normalised to 0/3/10 when importing a
  mission. Vehicle readback uses the `_INT` frames, and the document validator
  only accepted 0/2/3/10, so a real mission imported from a flight controller
  failed its own validation with `entity.frame_unsupported`.
- Mission upload no longer reports success on the start of the transfer alone;
  the client polls `mg_mission_get_progress` until the transfer finishes.

### Verification

- Backend: `ctest -L "offline|simulation"` 34/34. `test_sim_commands` 64/64
  (includes the SET HOME parameter encoding), `test_sim_ui_paths` 49/49
  (Home plus EXTENDED_SYS_STATE streaming requests), `test_sim_mission_document`
  20/20 (import from vehicle, including the frame normalisation),
  `test_mission_document` 122/122.
- Frontend: `flutter analyze` clean, `flutter test` 12/12,
  `dart tools/check_ui_tokens.dart` pass, `flutter build macos --debug` succeeds.

## 0.0.11 - 2026-09-29

### Added

- `core/tests/test_sim_ui_paths.cc` covers the five FFI entry points that real
  UI paths call but no test touched: `mg_calibration_cancel`,
  `mg_params_get_by_name`, `mg_stream_set_interval`,
  `mg_vehicle_request_firmware_version` and `mg_serial_get_uart_names`.
- `MockFlightController` now answers a minimal MAVLink FTP subset
  (ResetSessions / OpenFileRO / ReadFile / TerminateSession), so the
  `@SYS/uarts.txt` download behind `mg_serial_get_uart_names` runs offline in
  under a second instead of waiting out the 3 s x 4 retry budget.
- `test_sim_commands` pins the encoding of the safety commands
  (`mg_command_set_mode`, `mg_command_rtl`, `mg_command_land`,
  `mg_command_guided_point`) plus the takeoff-altitude and loiter-radius
  setters.
- `test_sim_params` covers the batch parameter change set: duplicate collapse,
  unknown/unchanged accounting, malformed JSON rejection, and `_ENABLE` written
  after its dependents.

### Changed

- `API_CATALOG.md` registers `mg_config_read_param`, `mg_config_write_param` and
  `mg_config_write_params` as the preferred parameter read/write entry points,
  and spells out how they differ from `mg_params_set` (descriptor validation,
  no need for a completed parameter download, REAL32 fallback).
- The flight mode page derives its PWM bands from the vehicle profile: fixed
  wing mode 1 additionally accepts `>= 2050`, and out-of-range RC values no
  longer select a slot.
- `tools/audit_status.sh` scans the test dimension for bare `mg_*` symbols
  instead of only call sites, so entries passed as function pointers are counted.
- `component_gallery_demo.dart` is documented as a development-only target: it
  is not reachable from `main.dart`, does not ship in a release build, and is the
  one intentional token-scan exemption.

### Fixed

- Calibration verification no longer reports success when the flight controller
  parameter readback fails. The previous check only failed on `Poor` quality, so
  a failed readback still surfaced as "calibration successful".
- Radio calibration treated `MAV_RESULT_ACCEPTED` as completion. The ACK only
  confirms the start command; the operator still has to move the sticks, so the
  stage now stays `Calibrating`.
- `mg_calibration_get_history` rejected type 4, so radio calibration history was
  always empty; the bound now matches the 0..4 contract in `API_CATALOG.md`.
- `mg_command_guided_point` carried `lat*1e7` / `lon*1e7` in the param5/param6
  slots of a `COMMAND_LONG`, where `MAV_CMD_DO_REPOSITION` expects degrees. The
  flight controller multiplies by 1e7 itself, so the coordinate overflowed.

### Verification

- Backend: `ctest -L "offline|simulation"` 34/34 pass; `test_sim_ui_paths` 26/26,
  `test_sim_commands` 58/58, `test_sim_params` 39/39, `test_sim_calibration` 75/75.
- Frontend: `flutter analyze lib test` clean, `flutter test` 12/12,
  `dart tools/check_ui_tokens.dart` pass, `flutter build macos --debug` succeeds.
- `tools/audit_status.sh` reports 281 exports / 48 used by frontend / 187 touched
  by tests / 93 untouched.

## 0.0.10 - 2026-09-29

### Added

- Map now draws the flight controller's Home point and the aircraft's current
  position, fed by the existing `mg_vehicle_get_global_position` and
  `mg_vehicle_get_home_position` caches at the regular telemetry rate. The
  aircraft marker rotates with heading; neither marker is drawn while the fix is
  invalid.
- Map view preferences are persisted (tile source, zoom, centre) under the app
  support directory, so the map reopens where it was left. New dependency:
  `path_provider` (BSD-3-Clause).
- Per-compass magnetometer calibration progress. `mg_calibration_get_status` now
  carries `compass_progress: [{compass_id, completion_pct}]` alongside the
  aggregate `progress`, and the calibration card draws one bar per detected
  compass.
- `docs/FONT_ALTERNATIVES_RESEARCH.md` and ADR 0007 record how the UI font stack
  was chosen, with per-glyph similarity measurements.

### Changed

- UI font is now a single bundled family, `MiSans`, on every platform, so macOS
  and Windows render identically. The previous stack silently resolved to
  Helvetica Neue / PingFang SC / Menlo on macOS because `SF Pro Text` is not a
  resolvable family name.
- Only titles are bold. Numeric readouts, status labels, badges, sensor models,
  firmware names and sidebar entries all use `w400`; the sidebar selection is
  expressed by its accent background instead of font weight.
- The sidebar is collapsible from a chevron next to the (now removed) search box
  and collapses to icons only.
- Waveform polling runs at 10 Hz instead of 1 Hz, matching the requested stream
  rate; `MgAnimatedNumber` tweens in 100 ms so the readout keeps up.
- Waveform window is 40 s.
- Motor output PWM bars are 18 px tall and use a smaller numeric size; the same
  widget is reused for RC calibration.
- Flight mode cards highlight the slot selected by the live `FLTMODE_CH` PWM, and
  RC5-16 cards highlight only when the channel sits in its high band.
- All-parameters page is a divider-separated table with Chinese display names and
  descriptions where the descriptor database provides them, plus unit labels.
- Empty pages keep their widget skeleton with a status strip instead of showing a
  blank page and a few lines of text.
- Arming checks use `ARMING_SKIPCHK` on current firmware with `ARMING_CHECK` as a
  fallback, rendered as a divided grid.
- Compass orientation and calibration tolerance are bound to real parameters
  (`COMPASS_ORIENT`, `COMPASS_CAL_FIT`) instead of the previous dead controls.
- Magnetometer field strength is refreshed for every compass instance, not only
  the selected one.

### Fixed

- Map coordinates: the flight controller reports WGS-84 while AMap tiles use
  GCJ-02, so markers were offset by 100-700 m. Conversion now follows Mission
  Planner's `MercatorProjectionGCJ`, with a three-step inverse that converges to
  centimetres.
- Calibration failures no longer discard the flight controller's own message.
  Both the STATUSTEXT and COMMAND_ACK paths previously replaced it with generic
  guidance, which is why every failure looked like "move away from magnetic
  interference".
- Accelerometer quality thresholds were an order of magnitude too strict for
  ArduPilot's `INS_ACCOFFS_*` range (+/-3.5 m/s2), so successful calibrations
  were reported as failed.
- Compass quality now trusts ArduPilot's own fitness score when available instead
  of taking the worse of fitness and an offset-magnitude heuristic.
- `DO_START_MAG_CAL` now sends `param2=1` (retry on failure) like Mission
  Planner, instead of disabling the flight controller's own retry.
- Motor output table width follows the panel; it previously stayed at 1040 px, so
  widening the window left blank space and narrowing it overpainted the border.
- `copy_points` with `max_points=1` returns zero points; the "latest sample"
  lookup now uses a 1 ms window instead, with a regression test pinning both
  behaviours.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 12 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.9 - 2026-09-29

### Added

- Added the mission planning engine with its Flutter-side document controller,
  validator, estimator, and upload/verify bridge.
- Added the arming checks page: 19 checks driven by a built-in bit table, reading
  and writing `ARMING_SKIPCHK` on newer firmware with `ARMING_CHECK` as a
  fallback for older ones.
- Added RC transmitter calibration end to end: a new `CalibrationType::Radio`
  strategy sending `MAV_CMD_PREFLIGHT_CALIBRATION` param4=1, `RCn_MIN/TRIM/MAX`
  read-back verification, and a Flutter page with live channel travel and
  `RCn_REVERSED` toggles.
- Added a staged PID tuning page with basic/extended levels for multirotor,
  fixed-wing, and VTOL. Multirotor and VTOL share one layout and differ only in
  the `ATC_`/`PSC_` versus `Q_A_`/`Q_P_` prefix; fixed-wing uses
  `RLL_RATE_`/`PTCH_RATE_`/`YAW_RATE_` plus `RLL2SRV`/`PTCH2SRV`/`YAW2SRV`
  and TECS.

### Changed

- Reworked the motor output page: `macos_ui` progress bars with centered PWM
  readouts, `MacosSwitch` for servo reverse, and optimistic parameter writes that
  render immediately and roll back when the flight controller rejects the value.
- Raised the regular telemetry poll rate from 2 Hz to 10 Hz and removed the
  separate servo output limiter, so the PWM bars track live output.
- Dropped the "waiting for parameter confirmation" banner; write failures are
  still surfaced.
- Removed unused frontend declarations (`MgTypography.group`, `hasParameters`,
  `isCalibrationActive`, `CalibrationStatus.isSuccess/isFailed`, ...) and merged
  the duplicated parameter lookup and error-formatting helpers.
- Renamed `widgets/mg_range_bar.dart` to `mg_motor_output_bar.dart` so the file
  matches its widget.
- Tightened the arming checks layout into a divided grid with uniform row heights.

### Fixed

- Corrected the position and velocity PID parameter names to the ArduPilot 4.8
  `PSC_NE_*` / `PSC_D_*` spelling; the previous `PSC_POSXY_P`-style names do not
  exist on current firmware, so that block read nothing.
- Hardened the Windows serial runtime and bridge packaging.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 7 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.8 - 2026-09-28

### Changed

- Connected the Flutter FFI loader to Windows `mgcore.dll`, Linux `libmgcore.so`,
  and Android `libmgcore.so` loading paths.
- Added Windows and Linux Flutter build steps that build and stage MgCore and its
  runtime JSON resources beside the desktop executable.
- Added the Android Flutter project and USB Host capability declaration. Android
  still requires a native USB-serial driver bridge before real USB flight-controller
  connections can be validated.
- Expanded the magnetometer calibration surface with per-instance discovery,
  compass mounting state, orientation controls, enable toggles, and field-strength
  summaries.
- Added a Flutter-rendered configuration workspace shell with responsive sidebar,
  search, navigation, toolbar, and explicit light/dark backgrounds; the page
  keeps its existing visual language without depending on macOS-only window
  plugins for these elements.
- Added shared typography and metrics tokens, animated numeric readouts, and a
  PWM range bar with minimum/midpoint/maximum markers for the motor output page.

### Verification

- Cross-platform workspace widget tests: 3 passed.
- Flutter static analysis: passed.
- macOS debug build: passed.

## 0.0.6 - 2026-09-27

### Added

- Added the minimal macOS Flutter shell for connection setup, telemetry, sensor waveforms and calibration workflows.
- Added IMU and magnetometer instance selectors with per-instance waveform buffers and sensor model labels.
- Added accelerometer calibration controls for six-face and level calibration, including step status, orientation placeholder, confirmation and cancellation.

### Changed

- Removed the `1s` mean readout from waveform value cards so the cards show only the latest sample.
- Aligned waveform instance selectors and chart content, and tightened the spacing between the waveform area and calibration cards.
- Added macOS bundle-resource lookup for `SensorDeviceInfo.json`, enabling sensor model decoding from packaged apps.
- Synchronized MgCore and Flutter shell version metadata to `0.0.6` (build `6`).

### Verification

- Flutter static analysis: passed.
- macOS debug build: passed.
- Native sensor and calibration tests: 3/3 passed.

## Backend branch - 2026-09-23

`New-main` was created from `v0.0.5` as the backend and interface branch. Its current shell and backend changes are released together in `main` as `v0.0.6`.

### Removed on this branch

- The Flutter desktop application and all Dart sources.
- Flutter desktop build scripts and toolchain checks.
- Frontend architecture, component and review documents.
- Frontend-specific release notes and the frontend session contract ADR.

### Changed on this branch

- CI now builds and tests the backend on Ubuntu, Windows and macOS.
- The Docker image and compose file only provision the backend toolchain.
- `README.md` and the maintained docs describe the backend contract instead of the Flutter client.
- `mg_vehicle_open()` now creates a session without changing active focus; `mg_vehicle_select()` is the explicit focus operation, while legacy single-connection helpers retain their compatibility behavior.

## 0.0.5 - 2026-09-23

### Changed

- Preserved the complete Flutter desktop frontend while restructuring the client into `application`, `infrastructure`, and `presentation` layers.
- Moved MgCore FFI and JSON adapters into `app/lib/infrastructure/mgcore/` and Flutter controllers/UI into `app/lib/presentation/`.
- Added dependency-direction guards for application, infrastructure, and presentation code.
- Fixed parameter fallback recovery, calibration callback lock boundaries, truncated magnetometer progress parsing, and hidden backstage polling.
- Synchronized CMake, FFI, and Flutter version metadata to `0.0.5` (build `5`).

### Verification

- Flutter tests: 119 passed.
- Native CTest offline/simulation gate: 29/29 passed.
- macOS Debug build: succeeded.

### Notes

- This release keeps the existing Flutter frontend intact. The backend-only client branch is created from this tag and intentionally removes the Flutter application there.

## 0.0.4 - 2026-09-23

### Added

- Capability-based application contracts for mission planning, geofence/Rally, flight logs, MAVLink FTP files, fleet management, and flight control.
- Mission editor support for local waypoint creation, map-coordinate waypoint insertion, validation, JSON/CSV export, and mission transfer progress.
- Flight Logs page with onboard storage browsing, file selection, download progress, cancellation, and upload progress.
- Fleet controller and frontstage vehicle panel for vehicle discovery, active-vehicle selection, batch arm/disarm, mode changes, RTL, landing, per-vehicle results, and active-vehicle restoration.
- Geofence and Rally workflows with validation plus JSON/KML import and export.
- Chinese/English localization catalogs, generated localization bindings, and localization contract tests.
- MacGlass shell contracts and shared frontstage models so the visual shell can be replaced without rewriting application logic.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.4` (build `4`).
- Improved connection/session handling, parameter-session isolation, telemetry polling, and reconnect/disconnect behavior.
- Extended MAVLink FTP and logging FFI paths for onboard file listing, upload, download, cancellation, and progress reporting.
- Raised the macOS deployment target to 12.0 to match the current Flutter/Xcode toolchain baseline.
- Expanded the regression suite to cover fleet control, mission editing/validation, localization, coordinate conversion, parameter synchronization, vehicle files, and session capabilities.

### Verification

- Flutter static analysis: passed with no issues.
- Flutter tests: 114 passed.
- macOS debug build: `mgcs.app` built successfully.
- Native CTest offline/simulation gate: 29/29 passed.

### Known limitations

- Native multi-vehicle batch commands still route through the active-vehicle selection as a compatibility bridge; a future explicit `vehicle_id` command target would remove this temporary coupling.
- Camera, gimbal, and video FFI primitives are present but are not yet exposed as complete Flutter workflows.
- Mission command coverage, log analysis/replay, offline map packages, firmware flashing, mobile packaging, and real-hardware write-operation acceptance remain future work.

## 0.0.3 - 2026-09-10

### Added

- End-to-end TCP and UDP connection flow from the Flutter application through the MgCore FFI backend, including endpoint validation and focused network tests.
- Airframe configuration page for Copter and Plane/QuadPlane, driven by the flight controller's existing parameter metadata.
- Native platform font fallback for Windows, macOS, and Linux.
- Regression coverage for the unified connection drawer layout and network connection callbacks.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.3`.
- Unified the Serial, TCP, UDP, and Bluetooth connection pages around the same `302 x 48` field geometry, label hierarchy, spacing, and glass control styling.
- Made the connection watchdog wake immediately when a disconnect or reset stops it.
- Kept vehicle shutdown outside the multi-vehicle manager mutex to avoid blocking transport teardown.
- Copied runtime metadata into every multi-config output directory so Windows Release tests resolve `SensorDeviceInfo.json` and related data files.
- Improved the Windows desktop build workflow and local toolchain usage.

### Notes

- Bluetooth remains a UI-only connection entry until a native Bluetooth transport and Dart application contract are added.
- Release verification passed 29 backend offline/simulation tests and 84 Flutter tests; Flutter static analysis reported no issues.
- A Windows Release package was built locally with the project-local toolchain and contains `mgcs.exe` and `mgcore.dll`.

## 0.0.2 - 2026-08-12

### Added

- Docker Compose development environment for Linux-based MgCore builds and Flutter checks.
- Shared MAVLink header generation script pinned to `pymavlink 2.4.49`.
- PowerShell and shell wrappers for the Docker development workflow.
- Flutter analyze/test job and reproducible MAVLink generation in GitHub Actions.
- Updated Chinese architecture and optimization reports.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.2`.
- Isolated generated MAVLink headers in a Docker named volume.
- Made the UDP transport test use an available local port.
- Updated widget tests to assert the current calibration and safety UI contract.
- Fixed MSVC standard-library portability and kept multi-config DLL output at `build/mgcore.dll` for Flutter Windows packaging.
- Built the Linux MgCore shared library before Flutter live FFI tests in CI.
- Added a native PowerShell MAVLink generator and a Windows Flutter Release packaging CI gate.
- Packaged MgCore parameter, translation, sensor, and USB board metadata beside the Windows executable.

### Notes

- Docker validates shared C++ and Flutter code but does not produce native macOS or Windows desktop bundles.
- Hardware and manual tests remain opt-in and are not part of the default container or CI gate.
- Release verification passed 29 backend offline/simulation tests and 79 Flutter tests; Flutter static analysis reported no issues.
- The Docker Compose definition was validated, but the local image build could not reach Docker Hub authentication and should be repeated on an unrestricted network.
