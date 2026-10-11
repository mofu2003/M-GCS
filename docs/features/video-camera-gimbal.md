# 功能简报：视频 / 相机 / 云台（Video / Camera / Gimbal）

状态：方向已立项。**S1（native 状态层 + FFI 读契约修复）已实现并验证**；S2（前端相机/云台页）与 S3（本地视频解码/渲染）尚未开始。视频解码跨平台方案（S3）必须先做依赖与许可决策。
日期：2026-10-11
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 3 第 4 条；`docs/capabilities.md` 对 `ffi_api_payload.h` 的「待接入」说明。

本文件是阶段 3「能力扩展，逐项立项」的第四个方向。按计划要求，先完成产品需求、上游行为研究、license/外部依赖检查与验收标准，再做切片。上游源码事实来自本地快照（QGC `bf0096d97`、MP `a2fcd74d6`，见 `docs/constraints/parity.md` 参考版本表）。

## 1. 目的

M-GCS 里 `ffi_api_payload.h` 的公开导出（立项时 17 个，S1 新增 2 个云台读导出后为 19 个）立项时是「已实现但无入口」：命令类只发一帧，设备回读状态要么不保存、要么读接口返回硬编码值。这个方向的目标是：

- 让相机、云台、视频流三类设备在 M-GCS 里有**可回读、可解释的状态**，而不只是单向发命令。
- 给用户一条可信的链路：发现设备 → 看到状态（连接/录制/存储/姿态）→ 执行动作（拍照/录像/变焦/云台指向）→ 看到结果或明确失败原因。
- 视频本地解码/渲染是独立的平台工程，先做依赖与许可决策，再证明至少两端平台可用；证明前 UI 不声称「已支持视频」。

目标不是一次性对齐 QGC 的全部相机定义 XML 与云台 on-screen 控制，而是先把「状态可信、错误可解释」这一步做扎实。

## 2. 范围

- 范围内：相机发现与状态（`CAMERA_INFORMATION`/`CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`STORAGE_INFORMATION`）；云台发现与状态（`GIMBAL_MANAGER_INFORMATION`/`GIMBAL_MANAGER_STATUS`/`GIMBAL_DEVICE_ATTITUDE_STATUS`/`GIMBAL_DEVICE_INFORMATION`）；视频流元数据（`VIDEO_STREAM_INFORMATION`/`VIDEO_STREAM_STATUS`）与流选择；FFI 读契约与错误对象；前端相机/云台/视频源配置页。
- 范围外：本地视频解码与渲染（独立平台切片 S3）、录像与截图落盘（S4）、相机定义 XML 驱动的完整设置项、云台 on-screen 点击指向与 ROI 向导、热成像/双路画面、SubtitleWriter 式遥测字幕叠加。

## 3. M-GCS 现状（S1 改造前）

| 层 | 位置 | 状态 |
|---|---|---|
| 相机 FFI | `bridge/ffi/ffi_api_payload.h:12-20`、`bridge/ffi/ffi_payload_commands.cc:20-94` | 命令类（`take_photo`/`start|stop_video`/`set_zoom`/`set_mode`/`stop_capture`/`set_focus`/`request_info`）都是「发一帧即返回」；`mg_camera_get_status`（`:84`）返回**硬编码** `available:false`。`camera_id` 入参被 `(void)` 丢弃，永远打向活动 target |
| 云台 FFI | `bridge/ffi/ffi_api_payload.h:22-26`、`ffi_payload_commands.cc:96-129` | `mg_command_gimbal_control`/`recenter`/`request_info`/`configure` 都是发命令；**没有任何云台状态读接口** |
| 视频 FFI | `bridge/ffi/ffi_api_payload.h:28-31`、`ffi_payload_commands.cc:131-189` | `mg_video_get_streams`（`:155`）已能从适配器读回真实流列表；`mg_video_status`（`:146`）返回**硬编码** `status:"stopped"`；`mg_video_start`（`:131`）**忽略 `uri` 并误发 `camera_start_video`**（真实缺陷，见 5 节 G1） |
| 适配器解析 | `core/src/mavlink/mavlink_adapter_handlers.cc:591`（视频流，已存缓存）、`:622`（相机信息**只打日志**）、`:632`（拍照状态**只打日志**）、`:641`/`:646`（云台 manager info/status **只打日志**） | `VIDEO_STREAM_INFORMATION` 已进 `tel.video_streams`；相机与云台消息只落日志，没有任何状态缓存 |
| 分发 | `core/src/mavlink/mavlink_adapter_dispatch.cc:259-273` | 已分发 261/254/269/280/281；**未分发** `GIMBAL_DEVICE_ATTITUDE_STATUS`(285)、`GIMBAL_DEVICE_INFORMATION`(283)、`CAMERA_SETTINGS`(260)、`STORAGE_INFORMATION`(262)、`VIDEO_STREAM_STATUS`(270) |
| core 设备类 | `core/include/core/camera/camera_manager.h`、`core/src/camera/camera_manager.cc`、`core/include/core/video/video_receiver.h`、`core/src/video/video_receiver.cc` | `CameraManager`/`VideoReceiver` 存在且被 `Vehicle` 持有（`core/include/core/vehicle/vehicle.h:165-166,249-250`），但 `CameraManager::process_message` **全项目无调用点**（死代码）；`VideoReceiver::start` 的 FFmpeg 解码是注释占位（`core/src/video/video_receiver.cc:24-30`），只做元数据；**S1 已删除**（本仓库无任何调用点，见 3.1） |
| 车辆转发 | `core/src/vehicle/vehicle.cc:958-973` | 仅把 `VIDEO_STREAM_INFORMATION` 转发进 `video().process_stream_info`；相机/云台无转发 |
| 前端 | `flutter_shell/lib/` | **0 绑定**：`rg` 在 `flutter_shell/lib` 找不到任何 `mg_camera_*`/`mg_gimbal_*`/`mg_video_*` 调用 |

`docs/capabilities.md` 把 `ffi_api_payload.h` 标为 17 导出 / 0 接入 / 「待接入：已实现但无入口」。

### 已发现的真实问题（本轮只读核对）

- **P1（`mg_video_start` 语义错误）**：`ffi_payload_commands.cc:131` 把 `uri` 参数 `(void)` 丢弃，转而调用 `mav.camera_start_video()`。一个有 URI 的视频源启动接口，实际执行的是「让相机开始录像」。前端若按名字调用会得到与预期完全不同的行为。
- **P2（读接口是假数据）**：`mg_camera_get_status`（`:84`）与 `mg_video_status`（`:146`）返回硬编码常量。任何按契约解析 `success:true` 的调用方都会把「没有相机」当成「相机可用但未录制」。
- **P3（相机/云台状态完全丢失）**：`CameraManager::process_message` 无调用点；`handle_camera_information`/`handle_camera_capture_status`/`handle_gimbal_manager_*` 只 `MGC_LOG_INFO`，状态不落任何缓存，读接口无从取值。
- **P4（分发缺口）**：`GIMBAL_DEVICE_ATTITUDE_STATUS`(285)、`GIMBAL_DEVICE_INFORMATION`(283)、`CAMERA_SETTINGS`(260)、`STORAGE_INFORMATION`(262)、`VIDEO_STREAM_STATUS`(270) 都未进 dispatcher，即便有设备也不会被感知。
- **P5（`camera_id` 被忽略）**：所有 `mg_camera_*` 的 `camera_id` 入参与 `mg_command_gimbal_configure` 的 `gimbal_device_id` 都没有选择语义，多相机/多设备无法寻址（`ffi_payload_commands.cc:20-94`）。
- **P0（真实缺陷，影响正确性）**：dispatcher 的 `case 261` 与 `case 254` 原本被注释成 `CAMERA_INFORMATION`/`CAMERA_CAPTURE_STATUS`，但按 MAVLink 定义 `261 = STORAGE_INFORMATION`、`254 = DEBUG`。结果是相机信息与拍摄状态永远不会被分发，而 `DEBUG` 帧反而被当作 `CAMERA_CAPTURE_STATUS` 处理。**S1 修复**：所有 ID 改用 `MAVLINK_MSG_ID_*` 宏。

### 3.1 S1 落地结果（2026-10-11）

- **dispatcher（P0/P4）**：`core/src/mavlink/mavlink_adapter_dispatch.cc` 全部改用 `MAVLINK_MSG_ID_*` 宏，补齐 `CAMERA_INFORMATION`(259)/`CAMERA_SETTINGS`(260)/`STORAGE_INFORMATION`(261)/`CAMERA_CAPTURE_STATUS`(262)/`CAMERA_IMAGE_CAPTURED`(263)/`VIDEO_STREAM_STATUS`(270)/`GIMBAL_MANAGER_INFORMATION`(280)/`GIMBAL_MANAGER_STATUS`(281)/`GIMBAL_DEVICE_INFORMATION`(283)/`GIMBAL_DEVICE_ATTITUDE_STATUS`(285)。ID 已逐个对照 `third_party/mavlink` 头文件宏。
- **适配器状态层（P3）**：`core/include/core/mavlink/mavlink_adapter.h` 新增相机/云台/视频流状态结构体与读 API（`latest_cameras`、`camera_information/settings/capture_status`、`camera_storage`、`latest_gimbals`、`gimbal_manager_information/status`、`gimbal_device_information`、`gimbal_device_attitude_status`、`latest_video_stream_status`）；`core/src/mavlink/mavlink_adapter_state.h` 按 `(sysid, compid[, stream_id/storage_id])` 键控缓存；`core/src/mavlink/mavlink_adapter_handlers.cc` 用 `upsert_by` 落地，替代原来的 `MGC_LOG_INFO` 空实现；`core/src/mavlink/mavlink_adapter.cc` 加锁读取。
- **FFI 读契约（P1/P2/P5）**：`bridge/ffi/ffi_payload_commands.cc` 重写——`mg_camera_get_status` 返回真实缓存（`available`/`vendor`/`model`/`photo_count`/`recording`/`mode`/`storage` 等），无设备时 `success:true`+`available:false`（D5）；新增 `mg_gimbal_get_status`/`mg_gimbal_get_attitude` 导出（四元数 → 欧拉角）；`mg_video_status` 返回真实流列表并与 `VIDEO_STREAM_STATUS` 配对；`mg_video_start` 空 URI 返回 `MG_ERROR_INVALID_PARAM`、非空 URI 返回 `MG_ERROR_NOT_SUPPORTED`，不再误发 `camera_start_video`；`camera_id`/`gimbal_id` 按 D4 处理。
- **删除死代码（D1）**：删除 `core/include/core/camera/camera_manager.h`、`core/src/camera/camera_manager.cc`、`core/include/core/video/video_receiver.h`、`core/src/video/video_receiver.cc`，并从 `CMakeLists.txt`、`core/include/core/vehicle/vehicle.h`（访问器与成员）、`core/src/vehicle/vehicle.cc`（`on_video_stream_info` 转发块）摘除引用。`ffmpeg_decoder`（S3 解码基座）保留。
- **文档/契约（F1）**：`docs/capabilities.md`、`bridge/ffi/API_CATALOG.md` 由 `python3 tools/check_ffi_contract.py --write-catalog` 重新生成。

## 4. 上游行为研究

### QGroundControl（Apache-2.0 / GPL 双许可；参考提交 `bf0096d97`）

- **视频源配置模型**：`src/Settings/Video.SettingsGroup.json` 定义 `videoSource`（UDP/TCP/RTSP/UVC）、`udpUrl`/`rtspUrl`/`tcpUrl`、`videoSavePath`、`aspectRatio`、`gridLines`、`videoFit`、`recordingFormat`（枚举 mp4/mov/mkv，值 2/1/0）、`maxVideoSize`（默认 10240 MB）、`enableStorageLimit`、`rtspTimeout`。**优点**：源可切换、录制格式与存储上限可配。**不采用**：默认值直接绑定 Qt 设置系统。
- **流自动选择**：`src/VideoManager/VideoManager.cc:590` `_updateAutoStream()` 按 `VIDEO_STREAM_TYPE_RTSP`/`TCP_MPEG`/`RTPUDP`/`MPEG_TS` 把 `VIDEO_STREAM_INFORMATION` 的 `uri` 映射成 `rtsp://`/`tcp://`/`udp://`（H264）/`udp265://`（H265）/`mpegts://` 源串（`:698-703`）。**优点**：设备上报即自动配置源，无需用户手填 URL。
- **录制与截图**：`RecordingFormat` 枚举在 `src/VideoManager/VideoReceiver/VideoReceiver.h:50-57`（MKV=0/MOV=1/MP4=2），`VideoManager.cc:38` 有扩展名表；`:353` `startRecording()`、`:385` `stopRecording()`、`:392` `grabImage()`（截图）、`:318/:343` 存储上限与自动删除最旧录像。**优点**：格式可选、有容量上限。**参考，不在 S1**。
- **相机发现与状态**：`src/Camera/QGCCameraManager.cc:160-200` 只处理来自相机组件（`MAV_COMP_ID_CAMERA..CAMERA6`）或自驾仪代理的消息；`:217-226` 对每个 compId 建请求上下文；`:576-589` 交替用 `MAV_CMD_REQUEST_MESSAGE` 与 `MAV_CMD_REQUEST_CAMERA_INFORMATION` 请求 `CAMERA_INFORMATION`；状态来自 `CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`STORAGE_INFORMATION`。**优点**：按组件寻址 + 重试/退避；**不采用** Qt 信号槽模型。
- **云台（V2 协议）**：`src/Gimbal/GimbalController.cc:55-84` 处理 `GIMBAL_MANAGER_INFORMATION`/`STATUS`/`GIMBAL_DEVICE_ATTITUDE_STATUS`/`GIMBAL_DEVICE_INFORMATION`；`:676` `setGimbalRetract()` 用 `GIMBAL_DEVICE_FLAGS_RETRACT` 位；`:694` `sendRate()`、`:720` `_sendGimbalAttitudeRates()` 走 `GIMBAL_MANAGER_FLAGS_ROLL_LOCK|PITCH_LOCK|YAW_IN_VEHICLE_FRAME`；`:565` `gimbalOnScreenControl()` 做点击指向。`src/Settings/GimbalController.SettingsGroup.json` 有 `enableOnScreenControl`、`cameraVFov/HFov`、`cameraSlideSpeed`。**优点**：以 `GIMBAL_MANAGER_INFORMATION` 为能力/设备 id 的事实来源，姿态读取用 `GIMBAL_DEVICE_ATTITUDE_STATUS`。**参考能力模型**。
- **视频后端**：`src/VideoManager/VideoReceiver/GStreamer/`（GstVideoReceiver）与 `QtMultimedia/`（UVCReceiver）两套后端，另有 `SubtitleWriter.cc` 写遥测字幕。**优点**：backends 可切换；**不采用** GStreamer/Qt 依赖（见 9 节）。

### Mission Planner（GPLv3，**只参考行为，禁止复制源码**；参考提交 `a2fcd74d6`）

- **相机协议**：`ExtLibs/ArduPilot/Mavlink/CameraProtocol.cs:270-295` `ParseMessages()` 按 `CAMERA_INFORMATION`/`CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`VIDEO_STREAM_INFORMATION`/`CAMERA_FOV_STATUS`/`CAMERA_TRACKING_IMAGE_STATUS` 更新状态；`ExtLibs/ArduPilot/Camera.cs:21-31` 用 `MAV_CMD.REQUEST_CAMERA_INFORMATION`、`REQUEST_VIDEO_STREAM_INFORMATION`、`REQUEST_CAMERA_SETTINGS`、`SET_CAMERA_MODE`、`REQUEST_STORAGE_INFORMATION`、`VIDEO_START_STREAMING` 等标准命令。**优点**：命令集合覆盖完整、以 MAVLink 标准命令为准。
- **云台协议**：`ExtLibs/ArduPilot/Mavlink/GimbalManagerProtocol.cs` 用 `GIMBAL_MANAGER_CAP_FLAGS` 做能力门（`:236-239`：`HAS_PITCH_AXIS`/`HAS_YAW_AXIS`/`HAS_YAW_LOCK`/`HAS_YAW_FOLLOW`），`RetractAsync`（`:139`）用 `GIMBAL_MANAGER_FLAGS.RETRACT`，`SetAnglesCommandAsync`（`:233`）发 `DO_GIMBAL_MANAGER_PITCHYAW`（pitch/yaw + NaN rate），`SetAnglesStream`（`:257`）走 `GIMBAL_MANAGER_SET_PITCHYAW`。**优点**：能力位门控在发命令前——与 M-GCS 已有 `capabilities` 思路一致。
- **视频显示**：`Controls/Video.cs` 用 GStreamer 管道（`:53` `rtspsrc location=rtsp://... ! rtpjpegdepay ! videoconvert ! appsink`），另有 `Controls/VideoStreamSelector.cs`、`Controls/OSDVideo.cs`、`ExtLibs/LibVLC.NET/`。**参考抽象，不采用其依赖**（GPLv3 + 重量级本机库）。

## 5. 差距与决策点

差距（G）：
- **G1（真实缺陷）**：`mg_video_start(uri)` 忽略 URI 且误发相机录像命令。**S1 修复**。
- **G2（真实缺陷）**：`mg_camera_get_status`/`mg_video_status` 返回硬编码值，读契约是假的。**S1 修复**。
- **G3（真实缺陷）**：相机与云台状态只落日志，无缓存；`CameraManager::process_message` 是死代码。**S1 修复**（适配器状态模型 + 删除死代码）。
- **G4（真实缺陷）**：dispatcher 缺 `GIMBAL_DEVICE_ATTITUDE_STATUS`/`GIMBAL_DEVICE_INFORMATION`/`CAMERA_SETTINGS`/`STORAGE_INFORMATION`/`VIDEO_STREAM_STATUS`。**S1 补齐**。
- **G5**：`camera_id`/`gimbal_device_id` 无寻址语义。**S1** 至少让 FFI 按 sysid/compid 或 device_id 寻址，或诚实返回「不支持多设备寻址」。
- **G6**：没有前端入口（17 导出 0 接入）。**S2**。
- **G7**：没有本地视频解码/渲染。**S3**（平台工程 + 依赖决策）。
- **G8**：没有录像/截图落盘与容量管理。**S4**。
- **G9**：QGC 用相机定义 XML 驱动完整设置面板，M-GCS 无。**D2 决定**：先不做 XML，只暴露 MAVLink 标准设置项。

决策点（D）：
- **D1（S1 已决定）**：状态模型的唯一事实来源放在**适配器层缓存**（与 `tel.video_streams` 一致）。`CameraManager`/`VideoReceiver` 本仓库内无任何调用点（死代码），S1 决定**直接删除**，改为在适配器状态按 `(sysid, compid)` 建最小缓存（符合 A2「适配器只翻译协议」的既有分层），避免两套事实来源。
- **D2（S1 已决定）**：不引入相机定义 XML；只做 MAVLink 标准字段（信息/设置/拍照状态/存储）。QGC 的 XML 能力留待独立议题。
- **D3（未决，S3 前必须定）**：本地视频解码用 GStreamer / FFmpeg / 平台原生（AVFoundation/MediaFoundation/MediaCodec）哪一类？涉及外部依赖与许可（见 9 节）。
- **D4（未决）**：`camera_id` 语义——按序号（第 N 个发现的相机）还是 MAVLink compId？QGC 用 compId 寻址。
- **D5（S1 已决定）**：无相机/云台时读接口返回什么？决定：**`success:true` + `available:false`**（「已查询、当前无设备」是合法状态），命令类在无目标设备时返回 `MG_ERROR_NOT_CONNECTED`/`MG_ERROR_NOT_SUPPORTED` 错误对象。缺值/无设备与「调用失败」是两回事（与 terrain 的 `valid:false` 同理）。

## 6. 交付切片

- **S1️⃣ native 状态层 + FFI 读契约（本轮完成）**：相机状态缓存（`CAMERA_INFORMATION`/`CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`STORAGE_INFORMATION`）、云台状态缓存（`GIMBAL_MANAGER_INFORMATION`/`STATUS`、`GIMBAL_DEVICE_ATTITUDE_STATUS`/`INFORMATION`）、`VIDEO_STREAM_STATUS`；补齐 dispatcher；`mg_camera_get_status`/`mg_video_status` 返回真实状态；修 `mg_video_start`；新增云台状态读导出（`mg_gimbal_get_status`/`mg_gimbal_get_attitude`）；`camera_id`/`gimbal_device_id` 按 `D4` 处理；单测。
- **S2️⃣ 前端相机/云台页**：配置工作区新增「相机与云台」页，展示发现的相机与云台、状态（录像/存储/姿态）、执行拍照/录像/变焦/云台指向/回中；视频源设置（RTSP/UDP/TCP URL + 流列表 + 选择）。
- **S3️⃣ 本地视频解码/渲染**：先完成 D3 决策（依赖 + 许可 + 平台矩阵），再用最小可行后端证明至少两端平台可播放；证明前 UI 只显示流元数据、不声称「已支持视频」。
- **S4️⃣ 录像与截图落盘**：录制格式选择 + 容量上限 + 自动删除最旧（对齐 QGC `maxVideoSize`/`enableStorageLimit`）。

## 7. 验收标准

- **S1 已满足**：`mg_video_start` 不再误发录像命令（单测钉住）；`mg_camera_get_status`/`mg_video_status`/云台读导出返回真实缓存状态，无设备时 `available:false` 而非假 `success`；dispatcher 覆盖 G4 列出的消息 ID；相机/云台状态有单测（构造消息 → 读接口 → 断言字段）。
- **S2 目标**：完成一次「发现相机 → 见状态 → 拍照 → 状态更新」与「发现云台 → 云台指向 → 姿态变化」的可观察链路。
- **S3 目标**：至少两端平台（macOS/Linux/Windows 中任选两端，Android 至少一端）能播放一路 RTSP 或 UDP 流，并有平台能力矩阵；未验证的平台标 `unsupported`。
- **自动化**：新增 native 测试（消息 fixture → 状态）；`tools/preflight.sh` 通过；FFI catalog 计数一致。

## 8. 平台

- S1 是纯协议/状态逻辑，全平台一致，无外部依赖。
- S2 是 Flutter UI，遵循现有跨平台约束。
- S3 是唯一有平台差异的切片：解码后端按 OS 能力矩阵决定，未证明的平台显式 `unsupported`。
- 视频流本身来自网络（RTSP/UDP/TCP），与 conformance/GCS 平台无关。

## 9. 许可与外部依赖

- **QGC**：Apache-2.0 / GPL 双许可。S1 以行为对齐为主，不复制源码；若引用其代码需先写 ADR（`upstream.md` P4）。
- **MP**：GPLv3（`COPYING.txt`）。**禁止复制源码**，只参考行为与协议事实。
- **MAVLink**：`CAMERA_*`/`GIMBAL_*`/`VIDEO_STREAM_*` 使用官方定义（`upstream.md` P5）。
- **S3（未决）**：GStreamer（LGPL）、FFmpeg（LGPL/GPL 视构建）、平台原生（系统 API）三条路的许可与打包成本不同，`D3` 决策时必须写成 ADR；默认倾向避免 GPL 构建。
- **参考源码版本**：QGC `bf0096d97`、MP `a2fcd74d6`（`parity.md` 参考版本表）。

## 10. 未决问题

- **D3（视频解码后端）**：未决——S3 的前置条件。
- **D4（`camera_id` 语义）**：未决。S1 暂按 MAVLink `(sysid, compid)` 寻址并在文档写明。
- **G5 完整多设备寻址**：S1 只做到「按连接的活动目标/compId」，多相机 UI 选择留待 S2。
- **热成像/双路画面**：QGC 有 thermal 通道，M-GCS 未建模。
- **真实硬件验收**：相机与云台行为需真实设备或能模拟 `CAMERA_*`/`GIMBAL_*` 的仿真链路验证；SITL 是否覆盖待确认（ArduPilot SITL 默认无相机/云台组件）。
