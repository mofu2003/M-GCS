# ADR-0008：本地视频解码后端与帧交付方式

状态：已接受
日期：2026-10-11

## 背景

阶段 3 方向 4（视频/相机/云台）的 S3 是「本地视频解码/渲染」。S1/S2 已把
MAVLink 侧的相机/云台/视频**元数据**做可信（`VIDEO_STREAM_INFORMATION` 等进
适配器缓存，前端可读），但「把一路 RTSP/UDP 流解成像素并显示」还没实现：
`mg_video_start` 当前显式返回 `NOT_SUPPORTED`，`mg_video_status.playing` 恒为
`false`。

仓库里另有一份初始版本遗留的 `core/src/video/ffmpeg_decoder.{h,cc}`：编译进
core（`CMakeLists.txt` 第 167 行），但**全仓库无调用点**；`MGCORE_HAS_FFMPEG`
默认 `OFF`，实际编译成空壳。它是死代码，也是这次要处理的对象。

解码后端有三条路，成本与许可差异很大，必须在动手前定下来（本 ADR 即方向 4 的
D3 决策）：

| 方案 | 许可 | 打包成本 | 覆盖 | 与本仓库架构的关系 |
|---|---|---|---|---|
| FFmpeg（仅 LGPL 构建） | LGPL-2.1+（禁用 `--enable-gpl`） | 与 core 同进程链接，按平台分发动态库 | RTSP/UDP/TCP、H.264/H.265、文件 | 解码在 native core，帧经 FFI 出 | 
| GStreamer | LGPL-2+ | 插件式运行时，部署较重（QGC 走这条） | 同上，插件决定 | 需要新的进程内管线与线程模型 | 
| 平台原生（AVFoundation / Media Foundation / MediaCodec） | 系统 API | 无需外部分发 | 每平台一套实现（4 份） | 每平台一套代码，维护面最大 | 

约束条件：

- 依赖方向 `flutter_shell → bridge/ffi → core → third_party` 单向（A1/A2）。
- 视频流**元数据**的唯一事实来源已在 native core（S1），本地解码只是消费
  `VIDEO_STREAM_INFORMATION` 给出的 `uri`。
- 禁止 GPL 传染（`docs/constraints/upstream.md`）；MP 是 GPLv3，只可参考行为。
- 平台能力要有证据：没跑过的平台不能标 `supported`。

## 决定

1. **后端选 FFmpeg，且只允许 LGPL 构建**（`--disable-gpl`）。理由：仓库已有
   同方向的骨架；解码留在 native core 与「单一 FFI 核心、前端可替换」的既定架构
   一致；无需引入重型 Flutter 插件或第二套进程内线程模型；LGPL 动态链接可用。
   GStreamer 与平台原生方案保留为未来可选项，届时另写 ADR。
2. **帧交付走 FFI，不走平台纹理。** core 解码线程把 RGBA8888 帧写进一个
   **有界「最新帧」缓冲**（单槽 + 序列号，带自己的互斥量）；FFI 侧
   `mg_video_poll_frame` 把最新帧拷进调用方缓冲区。前端用
   `ui.decodeImageFromPixels` 生成 `ui.Image` 渲染。
   - 这样解码不在 Flutter 插件里，`dart:ffi` 仍只在 backend 层（A5）。
   - 代价是每帧一次跨 FFI 拷贝与一次 `ui.Image` 重建；MVP 可接受，
     未来若需高帧率再评估平台纹理，另写 ADR。
3. **没有 FFmpeg 构建时诚实降级。** `mg_video_renderer_status` 报
   `available:false` + `backend:"none"`；`mg_video_start` 返回
   `MG_ERROR_NOT_SUPPORTED`；`mg_video_poll_frame` 返回 0。前端据此显示
   「本平台未启用本地解码」，**不显示假进度、不谎报正在播放**。
   `mg_video_status.playing` 只在本地解码器确实在跑且有新帧时为 true。
4. **`mg_video_start` / `mg_video_stop` 的语义固定为「本地视频源启停」**，
   与相机录像（`mg_camera_start_video`）区分开；后者是 MAVLink 命令。
5. 删除遗留的 `ffmpeg_decoder.{h,cc}`，由新的 `core/video/video_decoder.*`
   （含帧缓冲与 FFmpeg 适配）取代；旧文件无调用点，删除不改变行为。

## 平台矩阵与证据等级（当前）

| 平台 | build | 播放证据 | UI 状态 |
|---|---|---|---|
| macOS | 仅 `MGCORE_HAS_FFMPEG=OFF` 构建 | 无 | `unsupported`（诚实显示） |
| Linux | 未构建 | 无 | `unsupported` |
| Windows | 未构建 | 无 | `unsupported` |
| Android | 未构建 | 无 | `unsupported` |

本 ADR 只定**方向**。S3 的退出条件（至少两端平台能播放一路 RTSP/UDP，Android
至少一端）需要一台装有 LGPL FFmpeg 的构建机与真实流；在拿到该证据前，所有平台
一律按上表显示 `unsupported`，不得因为「编译通过」就把状态改成 `supported`。

## 后果

- FFmpeg 的 LGPL 合规要求写进发布检查：动态链接、随包附许可与源码获取方式、
  不得启用 GPL 组件。`docs/constraints/release-and-hardware.md` 与 SBOM 流程需覆盖。
- `core` 新增一个可选外部依赖（默认 `OFF`），`third_party/` 不变；FFmpeg 头文件
  只在 `MGCORE_HAS_FFMPEG` 时可见。
- FFmpeg 适配代码在本环境（无 FFmpeg、无 pkg-config）**无法编译验证**，只能与
  `OFF` 路径共存；启用构建时必须先过一遍编译与一路流的实测，再更新上表。
- 帧缓冲与 FFI 契约是后端无关的，可脱离 FFmpeg 用合成帧单测；这部分进 CI。

## 相关

- `docs/constraints/architecture.md`（A1、A2、A3、A5、A6）
- `docs/constraints/upstream.md`（许可）、`docs/constraints/release-and-hardware.md`
- `docs/features/video-camera-gimbal.md`（D3）
- `core/include/core/video/video_decoder.h`、`core/src/video/video_decoder.cc`、`bridge/ffi/ffi_api_payload.h`
