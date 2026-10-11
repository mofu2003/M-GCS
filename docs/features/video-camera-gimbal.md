# 功能简报：视频 / 相机 / 云台（Video / Camera / Gimbal）

状态：方向已立项。**S1（native 状态层 + FFI 读契约修复）、S2（前端「相机与云台」页）、S3（本地视频解码/渲染管线）与 S4（本地录制/截图落盘 + 容量管理）已实现并验证**。解码后端与帧交付方式由 `docs/adr/0008-video-decode-backend.md` 定案（FFmpeg 仅 LGPL 构建、帧经 FFI 交付、无后端诚实降级）。macOS(arm64) 上用本地 LGPL FFmpeg 构建跑通了「真实码流播放 + 录制落盘 + 截图落盘」的端到端验证（2026-10-11，见 3.4/8/11 节）；「至少两端平台」的退出条件只完成一端，未验证平台一律如实标 `unsupported`。
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

- 范围内：相机发现与状态（`CAMERA_INFORMATION`/`CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`STORAGE_INFORMATION`）；云台发现与状态（`GIMBAL_MANAGER_INFORMATION`/`GIMBAL_MANAGER_STATUS`/`GIMBAL_DEVICE_ATTITUDE_STATUS`/`GIMBAL_DEVICE_INFORMATION`）；视频流元数据（`VIDEO_STREAM_INFORMATION`/`VIDEO_STREAM_STATUS`）与流选择；FFI 读契约与错误对象；前端相机/云台/视频源配置页；本地视频解码/渲染管线（S3，含无后端诚实降级）；本地录制与截图落盘 + 容量上限清理（S4）。
- 范围外：相机定义 XML 驱动的完整设置项、云台 on-screen 点击指向与 ROI 向导、热成像/双路画面、SubtitleWriter 式遥测字幕叠加、多路并发录制、录制再编码/转码。

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
- **删除死代码（D1）**：删除 `core/include/core/camera/camera_manager.h`、`core/src/camera/camera_manager.cc`、`core/include/core/video/video_receiver.h`、`core/src/video/video_receiver.cc`，并从 `CMakeLists.txt`、`core/include/core/vehicle/vehicle.h`（访问器与成员）、`core/src/vehicle/vehicle.cc`（`on_video_stream_info` 转发块）摘除引用。`ffmpeg_decoder` 当时保留（S3 判定其无调用点后删除，见 3.3）。
- **文档/契约（F1）**：`docs/capabilities.md`、`bridge/ffi/API_CATALOG.md` 由 `python3 tools/check_ffi_contract.py --write-catalog` 重新生成。

### 3.2 S2 落地结果（2026-10-11）

- **前端页（G6）**：新增 `flutter_shell/lib/payload/`——`payload_models`（相机/云台/姿态/视频流模型）、`payload_backend`（接口 + 不可用替身）、`payload_ffi_backend`（`dart:ffi` 绑定，A5 白名单文件）、`payload_controller`（状态机）、`camera_gimbal_page`（页面）。入口：设置菜单「相机与云台」→ 配置工作区 `payload` 页。页面只经控制器读写，不直接接触 backend 或 `dart:ffi`（U1/A5）。三张卡片：相机（清单 + 型号/分辨率/模式/拍摄状态/存储）、云台（清单 + 能力范围 + 姿态）、视频源（流列表 + 地址行）。
- **多设备寻址（G5/D4）**：按 MAVLink 组件 id 选择相机/云台（`AdaptivePopupButton`，「自动（第一台）」= `selectedId:null` → native `id<=0`），选择后立即用该 compId 重新读取。D4 采用 QGC 的 compId 语义（见下）。
- **诚实措辞**：命令成功只显示「已发送…」，不谎报完成；未连接/失败复用 native 规范错误对象文案；无设备显示「已查询、当前无设备」而非错误。
- **视频**：视频区只展示 `mg_video_status`/`mg_video_get_streams` 的元数据，并明确「本地解码尚未实现（S3）」；`startVideo` 非空 URI 照实转发 native 的 `NOT_SUPPORTED`、空 URI 前端直接拒绝。
- **测试**：`flutter_shell/test/payload_controller_test.dart`（9 用例）、`camera_gimbal_page_test.dart`（3 用例）、`test/support/payload_backend_fake.dart`；`architecture_ffi_boundary_test.dart` 白名单登记 `payload/payload_ffi_backend.dart`。
- **文档/契约**：`docs/capabilities.md` 的 `ffi_api_payload.h` 改为「部分接入」（19 导出 / 17 前端绑定），`API_CATALOG.md` 由 `check_ffi_contract.py --write-catalog` 重新生成；`docs/constraints/ui-pages.md` 登记新页。

### 3.3 S3 落地结果（2026-10-11）

- **决策（D3 → ADR-0008）**：本地解码后端选 **FFmpeg 且只允许 LGPL 构建**（`--disable-gpl`）；帧交付走 **FFI 拷贝 + 前端 `ui.decodeImageFromPixels`**，不走平台纹理；无 FFmpeg 构建时诚实降级。理由与备选（GStreamer / 平台原生）对比见 `docs/adr/0008-video-decode-backend.md`。
- **native 后端**：删除遗留死代码 `core/{include,src}/video/ffmpeg_decoder.{h,cc}`（全仓库无调用点、`MGCORE_HAS_FFMPEG` 默认 `OFF` 时空壳），新增 `core/include/core/video/video_decoder.h` + `core/src/video/video_decoder.cc`：`VideoFrameBuffer`（单槽「最新帧」+ 序列号，自带互斥量）+ `VideoDecoder`（`start`/`stop`/`running`/`backend_available`/`backend_name`）。无后端时 `backend_available()=false`、`start()` 返回 `false`、`running()=false`。新增 `core/tests/test_video_pipeline.cc`（合成帧走帧缓冲/轮询契约，后端无关，35 断言）。
- **FFI**：`bridge/ffi/ffi_api_payload.h` 新增 `mg_video_renderer_status`（`{success,available,backend("none"|"ffmpeg"),running,uri,has_frame,width,height,pts_us,sequence}`）与 `mg_video_poll_frame(core,dst,cap,out_w,out_h,out_pts,out_seq)`（`>0`=字节数、`0`=无帧、`-1`=容量/参数错误并回报所需尺寸）。`mg_video_start` 语义固定为「本地视频源启停」：无后端 `NOT_SUPPORTED`、空 URI `INVALID_PARAM`、打不开 `SEND_FAILED`；`mg_video_status.playing` 仅在解码器运行且有新帧时为 `true`（与相机录像命令 `mg_camera_start_video` 彻底区分）。
- **前端（`flutter_shell/lib/payload/`）**：`payload_models` 新增 `VideoRendererStatus`；`payload_backend` 接口加 `rendererStatus()`/`pollFrame()`/`dispose()` 与 `VideoFrameData`，`PayloadUnavailableBackend` 返回 `available:false/backend:"none"`（合法能力态，不写错误）且 `pollFrame()=>null`；`payload_ffi_backend` 绑定两个新符号（复用一块原生帧缓冲，容量不足按 `out_*` 扩容重试一次）；`payload_controller` 新增 `videoRendererStatus`/`pollFrame`，`startVideo`/`stopVideo` 后静默刷新，`bindSession` 清 renderer；新增 `video_preview.dart`——仅 `running` 时 33 ms 轮询、序列号去重、`ui.decodeImageFromPixels` → `RawImage`，无后端/无帧显示确定文案；`camera_gimbal_page.dart` `_videoPanel` 重写为无后端显示「本平台未启用本地视频解码（此构建未包含 LGPL FFmpeg 后端）」、有后端显示后端名/地址/尺寸帧号 + 预览；`connection_controller.dispose()` 释放 backend。
- **测试/门禁**：`flutter analyze lib test` 无问题；`flutter test` 231 通过 / 4 跳过（较 S2 基线 +7 用例：`payload_controller_test` renderer 读取含断链、无后端合法、pollFrame、启动后刷新运行态，`camera_gimbal_page_test` 无后端文案、后端可用已停止、运行中元数据 + 卸载取消定时器）；`dart tools/check_ui_tokens.dart` 通过；`python3 tools/check_ffi_contract.py` 309 声明 / 131 绑定一致（+2 视频导出），`API_CATALOG.md`、`docs/capabilities.md` 已重生成。
- **诚实边界**：真帧 `ui.decodeImageFromPixels` 渲染未在 widget 测试覆盖（`flutter_tester` 下 `macos_ui` 等异步平台通道会污染 `runAsync`），需真实后端/设备验证；本机无 FFmpeg、无 pkg-config，`MGCORE_HAS_FFMPEG` 分支**未编译验证**；真实相机/云台硬件回读仍需真实设备。

### 3.4 S4 落地结果（2026-10-11）

- **native 后端**：`core/include/core/video/video_decoder.h` + `core/src/video/video_decoder.cc` 新增 `start_recording`/`stop_recording`/`recording_status`/`capture_frame`。录制是**码流复制**（`av_interleaved_write_frame`，不重新解码再编码），容器由扩展名决定（UI 固定用 `.mkv`/Matroska）；源没有时间戳时按帧率补一条合成时间轴。写帧在 `rec_mtx` 内，打开文件、写文件头与收尾（`av_write_trailer`/`avio_closep`）都在锁外（A7）；写失败立即收尾并把原因留在 `RecordingStatus.error`，不会留下半个「正在录制」的状态。截图取帧缓冲里的最新帧，按扩展名编码 `.png`（RGBA）或 `.jpg`（YUVJ420P）落盘。
- **FFI**：新增 `mg_video_start_recording(core, path)`、`mg_video_stop_recording(core)`、`mg_video_recording_status(core)`、`mg_video_capture_frame(core, path)`。契约：空路径 `INVALID_PARAM`（先于后端检查）；没有编译进解码后端 `NOT_SUPPORTED`；有后端但没有正在运行的本地源/没有可用帧 `INVALID_STATE`；`stop_recording` 在未录制时是幂等的成功。`recording_status` 恒为成功信封，字段 `available`/`recording`/`path`/`error`/`bytes`/`duration_us`，停止后保留最后一次结果，界面可显示「已保存 N」而不是回到空白。
- **前端**：`payload_models` 加 `VideoRecordingStatus`；`payload_backend`（含不可用替身）与 `payload_ffi_backend` 加四个方法；新增 `flutter_shell/lib/payload/video_storage.dart`——`VideoStorageSettings`（录制目录 / 容量上限开关 / 上限 MB，JSON 存应用支持目录，平台上不可用时退回临时目录）、默认上限 10240 MB、下界 100 MB（对齐 QGC `maxVideoSize`）、录制/截图文件名、`listRecordings` 与 `pruneOldRecordings`（越旧越先删，正在写的文件不删）。`payload_controller` 加 `startLocalRecording`/`stopLocalRecording`/`captureLocalFrame`/`refreshRecordingStatus`；这些是本机操作，`_send` 传 `requiresVehicle:false`，**不依赖飞控连接**（相机命令仍要求连接）。`camera_gimbal_page` 在解码后端可用时增加录制面板（录制状态 + 目录 + 保存 + 容量上限 + 开始/停止录制 + 截图）；后端不可用时整块不画，只保留「本平台未启用本地视频解码」。
- **测试**：`core/tests/test_video_pipeline.cc` 新增「video recording/capture FFI contract」组（按后端分支断言参数/状态/幂等），`test_new_modules.cc` 的 video FFI 改为按后端能力分支；Flutter 新增 `test/video_storage_test.dart`（7 用例：上限收口、文件名、列文件、清理、keep、无上限、建目录）、`payload_controller_test.dart` 的「本地录制与截图」组（6 用例：开始/空目录/断链可用/停止/截图/清理/后端失败）与 `camera_gimbal_page_test.dart` 的 2 个页面用例。`tools/check_ffi_contract.py` 记录 314 声明 / 136 绑定（+4 导出）。
- **端到端**：见 8 节与 11 节——LGPL FFmpeg 生成 `testsrc` 码流，`mg_video_start` 播放（`mg_video_poll_frame` 取到帧）、`mg_video_start_recording` 落 `.mkv`、`mg_video_capture_frame` 落 `.png`，产物均能通过 `ffprobe` 校验。

## 4. 上游行为研究

### QGroundControl（Apache-2.0 / GPL 双许可；参考提交 `bf0096d97`）

- **视频源配置模型**：`src/Settings/Video.SettingsGroup.json` 定义 `videoSource`（UDP/TCP/RTSP/UVC）、`udpUrl`/`rtspUrl`/`tcpUrl`、`videoSavePath`、`aspectRatio`、`gridLines`、`videoFit`、`recordingFormat`（枚举 mp4/mov/mkv，值 2/1/0）、`maxVideoSize`（默认 10240 MB）、`enableStorageLimit`、`rtspTimeout`。**优点**：源可切换、录制格式与存储上限可配。**不采用**：默认值直接绑定 Qt 设置系统。
- **流自动选择**：`src/VideoManager/VideoManager.cc:590` `_updateAutoStream()` 按 `VIDEO_STREAM_TYPE_RTSP`/`TCP_MPEG`/`RTPUDP`/`MPEG_TS` 把 `VIDEO_STREAM_INFORMATION` 的 `uri` 映射成 `rtsp://`/`tcp://`/`udp://`（H264）/`udp265://`（H265）/`mpegts://` 源串（`:698-703`）。**优点**：设备上报即自动配置源，无需用户手填 URL。
- **录制与截图**：`RecordingFormat` 枚举在 `src/VideoManager/VideoReceiver/VideoReceiver.h:50-57`（MKV=0/MOV=1/MP4=2），`VideoManager.cc:38` 有扩展名表；`:353` `startRecording()`、`:385` `stopRecording()`、`:392` `grabImage()`（截图）、`:318/:343` 存储上限与自动删除最旧录像。**优点**：格式可选、有容量上限。**S4 已对齐行为**：M-GCS 走码流复制录制、容量上限默认关闭、超限删最旧（只固定 Matroska 容器，不做格式选择）。
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
- **G5**：`camera_id`/`gimbal_device_id` 无寻址语义。**S1** 让 FFI 按 sysid/compid 或 device_id 寻址；**S2 完成**：前端按 compId 选择并下发，native 按 compId 寻址（`<=0` = 第一台/活动目标）。
- **G6**：没有前端入口（17 导出 0 接入）。**S2 完成**：新增配置工作区「相机与云台」页，设备清单/状态/命令与视频源元数据均有 UI 入口。
- **G7**：没有本地视频解码/渲染。**S3 完成**：ADR-0008 定案 FFmpeg(LGPL) 后端 + `VideoDecoder`/`VideoFrameBuffer` + `mg_video_renderer_status`/`mg_video_poll_frame` + 前端预览，无后端诚实降级。真实平台播放证据（退出条件）仍未达成，见 8 节。
- **G8**：没有录像/截图落盘与容量管理。**S4 完成**：native 码流复制录制 + 截图编码，FFI 四个新导出，前端录制面板与容量清理（对齐 QGC `maxVideoSize`/`enableStorageLimit`）。
- **G9**：QGC 用相机定义 XML 驱动完整设置面板，M-GCS 无。**D2 决定**：先不做 XML，只暴露 MAVLink 标准设置项。

决策点（D）：
- **D1（S1 已决定）**：状态模型的唯一事实来源放在**适配器层缓存**（与 `tel.video_streams` 一致）。`CameraManager`/`VideoReceiver` 本仓库内无任何调用点（死代码），S1 决定**直接删除**，改为在适配器状态按 `(sysid, compid)` 建最小缓存（符合 A2「适配器只翻译协议」的既有分层），避免两套事实来源。
- **D2（S1 已决定）**：不引入相机定义 XML；只做 MAVLink 标准字段（信息/设置/拍照状态/存储）。QGC 的 XML 能力留待独立议题。
- **D3（S3 已决定 → ADR-0008）**：本地视频解码用 **FFmpeg 且只允许 LGPL 构建**（`--disable-gpl`），解码在 native core、帧经 FFI 拷贝交付、前端 `ui.decodeImageFromPixels` 渲染；无后端诚实降级。GStreamer 与平台原生（AVFoundation/MediaFoundation/MediaCodec）保留为未来可选项。理由与许可分析见 `docs/adr/0008-video-decode-backend.md` 与 9 节。
- **D4（S2 已决定）**：采用 MAVLink compId 寻址（与 QGC 一致）：`> 0` 按组件 id 选择，`<= 0` 表示「第一台 / 活动目标」。前端下拉按 compId 选择。
- **D5（S1 已决定）**：无相机/云台时读接口返回什么？决定：**`success:true` + `available:false`**（「已查询、当前无设备」是合法状态），命令类在无目标设备时返回 `MG_ERROR_NOT_CONNECTED`/`MG_ERROR_NOT_SUPPORTED` 错误对象。缺值/无设备与「调用失败」是两回事（与 terrain 的 `valid:false` 同理）。

## 6. 交付切片

- **S1️⃣ native 状态层 + FFI 读契约（本轮完成）**：相机状态缓存（`CAMERA_INFORMATION`/`CAMERA_SETTINGS`/`CAMERA_CAPTURE_STATUS`/`STORAGE_INFORMATION`）、云台状态缓存（`GIMBAL_MANAGER_INFORMATION`/`STATUS`、`GIMBAL_DEVICE_ATTITUDE_STATUS`/`INFORMATION`）、`VIDEO_STREAM_STATUS`；补齐 dispatcher；`mg_camera_get_status`/`mg_video_status` 返回真实状态；修 `mg_video_start`；新增云台状态读导出（`mg_gimbal_get_status`/`mg_gimbal_get_attitude`）；`camera_id`/`gimbal_device_id` 按 `D4` 处理；单测。
- **S2️⃣ 前端相机/云台页（本轮完成）**：配置工作区新增「相机与云台」页，展示发现的相机与云台、状态（录像/存储/姿态）、执行拍照/录像/变焦/云台指向/回中；视频源设置（RTSP/UDP/TCP URL + 流列表 + 选择）。
- **S3️⃣ 本地视频解码/渲染（本轮完成方向，退出条件未达成）**：D3 决策落为 `ADR-0008`；native `VideoDecoder`/`VideoFrameBuffer` + FFI `mg_video_renderer_status`/`mg_video_poll_frame` + 前端 `video_preview` 已交付，无后端诚实降级。**仍开放**：至少两端平台播放一路真实 RTSP/UDP 流并落平台能力矩阵（需一台装 LGPL FFmpeg 的构建机与真实流），达成前平台矩阵全 `unsupported`。
- **S4️⃣ 录像与截图落盘（本轮完成）**：`VideoDecoder::start_recording`/`stop_recording`/`capture_frame`（码流复制、锁外 I/O）+ FFI `mg_video_start_recording`/`mg_video_stop_recording`/`mg_video_recording_status`/`mg_video_capture_frame` + 前端录制面板与 `video_storage`（容量上限 + 自动删除最旧，对齐 QGC `maxVideoSize`/`enableStorageLimit`）。录制容器固定 Matroska（`.mkv`），不提供格式选择（QGC 的 mp4/mov/mkv 三选一按需再开）。

## 7. 验收标准

- **S1 已满足**：`mg_video_start` 不再误发录像命令（单测钉住）；`mg_camera_get_status`/`mg_video_status`/云台读导出返回真实缓存状态，无设备时 `available:false` 而非假 `success`；dispatcher 覆盖 G4 列出的消息 ID；相机/云台状态有单测（构造消息 → 读接口 → 断言字段）。
- **S2 已满足**：页面可发现相机/云台、显示状态、下发拍照/录像/变焦/云台指向/回中，命令成功措辞为「已发送」；多设备按 compId 选择并重读；无设备与失败分别措辞；`payload_controller_test`/`camera_gimbal_page_test` 覆盖读、切换、命令成功/失败、断链、会话切换。真实硬件回读（状态更新链路）见 10 节的硬件验收待办。
- **S3 已满足（方向/工程交付）**：D3 决策写成 `ADR-0008`；native 解码器与帧缓冲、FFI 读/轮询契约、前端预览与无后端降级已交付并有单测（`test_video_pipeline` + 7 个 Flutter 用例）；`flutter analyze`/`flutter test`/`check_ui_tokens`/`check_ffi_contract` 全过。
- **S3 退出条件（部分达成）**：至少两端平台（macOS/Linux/Windows 中任选两端，Android 至少一端）能播放一路真实 RTSP 或 UDP 流，并有平台能力矩阵；未验证的平台标 `unsupported`。macOS(arm64) 已有真实播放 + 录制 + 截图的端到端证据（8 节）；仍需第二端平台。
- **S4 已满足**：native 录制/截图实现与 FFI 契约有单测（按后端分支断言 `INVALID_PARAM`/`NOT_SUPPORTED`/`INVALID_STATE`/幂等停止与状态信封）；前端录制面板、设置持久化与容量清理有 Flutter 用例；macOS 上真实码流录制出非空 `.mkv`、截图出可解码 `.png`，`ffprobe` 校验通过。
- **自动化**：新增 native 测试（消息 fixture → 状态）；`tools/preflight.sh` 通过；FFI catalog 计数一致。

## 8. 平台

- S1 是纯协议/状态逻辑，全平台一致，无外部依赖。
- S2 是 Flutter UI，遵循现有跨平台约束。
- S3/S4 是仅有的平台差异切片：解码/录制后端按 OS 能力矩阵决定，未证明的平台显式 `unsupported`。当前证据等级（`ADR-0008`）：

| 平台 | 构建 | 播放/录制/截图证据 | UI 状态 |
|---|---|---|---|
| macOS (arm64) | 本地 LGPL FFmpeg（`build_ffmpeg`，`--disable-gpl`） | 2026-10-11 端到端通过（TCP/MPEG-TS 真实码流：取帧、录 `.mkv`、截 `.png`，`ffprobe` 校验通过） | 开发构建可用；默认发布构建未打包 FFmpeg，仍显示 `unsupported` 文案 |
| Linux / Windows / Android | 未构建 | 无 | `unsupported`（诚实显示） |

- 「开发构建可用」不等于「发布可用」：仓库默认构建仍是 `MGCORE_HAS_FFMPEG=OFF`，只有显式启用 FFmpeg 的构建才实际解码。要把它变成产品默认行为，需要按 R6 把 LGPL FFmpeg 打进各平台发布包（见 10 节）。
- 视频流本身来自网络（RTSP/UDP/TCP），与 conformance/GCS 平台无关。

## 9. 许可与外部依赖

- **QGC**：Apache-2.0 / GPL 双许可。S1 以行为对齐为主，不复制源码；若引用其代码需先写 ADR（`upstream.md` P4）。
- **MP**：GPLv3（`COPYING.txt`）。**禁止复制源码**，只参考行为与协议事实。
- **MAVLink**：`CAMERA_*`/`GIMBAL_*`/`VIDEO_STREAM_*` 使用官方定义（`upstream.md` P5）。
- **S3（已决定 → ADR-0008）**：选 **FFmpeg 且只允许 LGPL 构建**（`--disable-gpl`），动态链接；禁止 GPL 传染，发布须随包附 LGPL 许可与源码获取方式（见 `docs/constraints/release-and-hardware.md` R6、ADR-0008 后果）。GStreamer（LGPL）与平台原生（系统 API）保留为未来可选项，届时另写 ADR。默认不引入任何 GPL 组件。
- **参考源码版本**：QGC `bf0096d97`、MP `a2fcd74d6`（`parity.md` 参考版本表）。

## 10. 未决问题

- **D3（视频解码后端）**：已决定（S3/ADR-0008）——FFmpeg 仅 LGPL 构建 + FFI 帧交付 + 无后端诚实降级。
- **S3/S4 平台证据**：只完成 macOS(arm64) 一端（真实播放 + 录制 + 截图，见 8 节）；至少还需第二端平台。其他平台在拿到证据前一律 `unsupported`。
- **发布打包 FFmpeg**：默认构建仍 `MGCORE_HAS_FFMPEG=OFF`。要把 macOS 的「开发构建可用」变成发布可用，需按 `release-and-hardware.md` R6 把 LGPL FFmpeg 动态库打进发布包并附许可/源码获取方式；Linux/Windows/Android 的打包方案未定。
- **录制格式选择**：QGC 允许 mp4/mov/mkv，M-GCS 固定 Matroska。当前按「一个可用容器」取舍，等有兼容性需求再开格式选择。
- **录制期间断流/换源**：`stop()`/`start()` 会先收尾当前录制再释放输入；会话切换时 Flutter 侧不自动续录（用户需重新开始），是否需要自动恢复未决。
- **D4（`camera_id` 语义）**：已决定（S2）——按 MAVLink compId 寻址，`<= 0` 表示第一台/活动目标。
- **G5 完整多设备寻址**：已完成（S2）——前端按 compId 选择并下发，native 按 compId 寻址。
- **热成像/双路画面**：QGC 有 thermal 通道，M-GCS 未建模。
- **真实硬件验收**：相机与云台行为需真实设备或能模拟 `CAMERA_*`/`GIMBAL_*` 的仿真链路验证；SITL 是否覆盖待确认（ArduPilot SITL 默认无相机/云台组件）。

## 11. 手动验证步骤（真实码流，S3/S4）

在一台装了 LGPL FFmpeg 的机器上（本仓库验证环境：macOS arm64，FFmpeg 7.1.1，`--disable-gpl`，前缀 `/Users/fansicheng/.local/ffmpeg-lgpl`）：

1. 用 FFmpeg 造一路本地码流（TCP 更省事，不需要额外的 RTSP 服务）：
   `ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=15 -c:v mpeg4 -q:v 5 -f mpegts -listen 1 tcp://127.0.0.1:12345`
2. 用启用 FFmpeg 的构建（`-DMGCORE_HAS_FFMPEG=ON`，链接上述 dylib）调 FFI：`mg_video_start(core, "tcp://127.0.0.1:12345")` → 轮询 `mg_video_poll_frame` 直到返回 `> 0`（确认真的在解码，而不是只报了 `running`）。
3. `mg_video_start_recording(core, "<dir>/rec.mkv")`，跑几秒；`mg_video_recording_status` 应报 `recording:true` 且 `bytes` 增长。
4. `mg_video_stop_recording(core)` 后 `recording_status` 应保留最终 `bytes`/`duration_us`；`mg_video_capture_frame(core, "<dir>/shot.png")` 应返回 `success:true` 与正确的 `width`/`height`。
5. 产物校验：`ffprobe rec.mkv`（Matroska，含视频流）与 `ffprobe shot.png`（PNG，尺寸与画面一致）。截图像素应与预览画面一致，而不是黑帧或空文件。

未编译进 FFmpeg 后端时，第 2 步的 `mg_video_start` 会返回 `NOT_SUPPORTED`，第 3/4 步返回 `NOT_SUPPORTED`；这是预期行为，UI 会显示「本平台未启用本地视频解码」，不画录制控件。

### 在 macOS 应用里看效果（开发构建）

默认应用构建是 `MGCORE_HAS_FFMPEG=OFF`，所以打开应用只会在视频面板看到「本平台未启用本地视频解码」。
要让它真正预览/录制/截图，先把应用的 mgcore 构建缓存指向 LGPL FFmpeg，再构建应用
（Xcode 内嵌阶段复用这个缓存；详细说明见 `docs/development.md`「运行前端」）：

```bash
cmake -S . -B flutter_shell/build/mgcore -DCMAKE_BUILD_TYPE=Release \
  -DMGCORE_BUILD_SHARED=ON -DMGCORE_BUILD_TESTS=OFF \
  -DMGCORE_HAS_FFMPEG=ON -DFFMPEG_ROOT=/Users/fansicheng/.local/ffmpeg-lgpl
cd flutter_shell && flutter build macos --debug
```

然后按第 1 步起一路 `testsrc`，在应用里打开「相机与云台」页，视频地址填 `tcp://127.0.0.1:12345` 并启动。
录制面板只有在后端可用时才出现。用 `otool -L .../Contents/Frameworks/libmgcore.dylib` 确认链接的是
LGPL 那份 FFmpeg；要回到默认构建，删掉 `flutter_shell/build/mgcore` 后重新构建即可。
这里的动态库路径是本机绝对路径，仅用于开发构建；发布打包按 `release-and-hardware.md` R6 单独处理。
