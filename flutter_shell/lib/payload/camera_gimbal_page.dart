import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/adaptive_popup_button.dart';
import '../widgets/aligned_field_row.dart';
import '../widgets/mac_panel.dart';
import '../widgets/mg_inline_notice.dart';
import 'payload_controller.dart';
import 'payload_models.dart';
import 'video_preview.dart';
import 'video_storage.dart';

/// 相机与云台页：列出已发现的相机/云台，显示状态并下发拍摄与指向命令。
///
/// 分工与其它页一致——native 是权威状态源：
/// - 页面只通过 [PayloadController] 读写，不直接接触 backend 或 `dart:ffi`；
/// - 设备清单来自 `mg_camera_get_status`/`mg_gimbal_get_status` 的 `cameras[]`/
///   `gimbals[]`，按 MAVLink 组件 id 选择；
/// - 命令只表示数据包已发出（native 无 ACK），界面措辞为「已发送」；
/// - 视频源：先显示本机解码后端能力（`mg_video_renderer_status`）；只有后端可用
///   且真正在跑时才显示预览帧，没有后端时如实说明「本平台未启用本地解码」。
class CameraGimbalPage extends StatefulWidget {
  const CameraGimbalPage({
    super.key,
    required this.controller,
    this.scrollController,
    this.storageLoader = VideoStorageSettings.load,
  });

  final PayloadController controller;
  final ScrollController? scrollController;

  /// 读取本地录制设置的入口。默认读应用支持目录；Widget 测试可注入同步实现，
  /// 避免依赖平台通道。
  final Future<VideoStorageSettings> Function() storageLoader;

  @override
  State<CameraGimbalPage> createState() => _CameraGimbalPageState();
}

class _CameraGimbalPageState extends State<CameraGimbalPage> {
  late final TextEditingController _zoom;
  late final TextEditingController _pitch;
  late final TextEditingController _yaw;
  late final TextEditingController _videoUri;
  late final TextEditingController _recordingDir;
  late final TextEditingController _limitMb;

  /// 落盘设置是否已从磁盘读入；读入前录制控件不显示，避免用过期的空目录发起录制。
  bool _storageLoaded = false;
  bool _limitEnabled = false;
  bool _storageSaved = false;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onChanged);
    _zoom = TextEditingController();
    _pitch = TextEditingController(text: '0');
    _yaw = TextEditingController(text: '0');
    _videoUri = TextEditingController();
    _recordingDir = TextEditingController();
    _limitMb = TextEditingController();
    _loadStorage();
    // 进入页面即读一次；断链时控制器给出确定错误，而不是停在空白。
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) widget.controller.refresh();
    });
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onChanged);
    _zoom.dispose();
    _pitch.dispose();
    _yaw.dispose();
    _videoUri.dispose();
    _recordingDir.dispose();
    _limitMb.dispose();
    super.dispose();
  }

  void _onChanged() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    return SingleChildScrollView(
      controller: widget.scrollController,
      padding: const EdgeInsets.fromLTRB(
        MgMetrics.configPageMargin,
        22,
        MgMetrics.configPageMargin,
        32,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text(
                '相机与云台',
                style: MgTypography.sans(
                  MgTypography.title.copyWith(color: colors.textPrimary),
                ),
              ),
              const SizedBox(width: 12),
              if (controller.loading)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CupertinoActivityIndicator(radius: 7),
                ),
              const Spacer(),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: controller.loading
                    ? null
                    : () => controller.refresh(),
                child: const Text('刷新'),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (controller.listError case final message?)
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_circle,
              color: colors.danger,
              text: message,
            ),
          if (controller.actionError case final message?)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MgInlineNotice(
                icon: CupertinoIcons.exclamationmark_triangle,
                color: colors.warning,
                text: message,
                trailing: MgDismissButton(
                  onPressed: controller.clearActionError,
                ),
              ),
            ),
          if (controller.actionMessage case final message?)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MgInlineNotice(
                icon: CupertinoIcons.checkmark_circle,
                color: colors.accent,
                text: message,
                trailing: MgDismissButton(
                  onPressed: controller.clearActionMessage,
                ),
              ),
            ),
          const SizedBox(height: MgMetrics.configSectionGap),
          _section(context, '相机'),
          _cameraPanel(context),
          const SizedBox(height: MgMetrics.configSectionGap),
          _section(context, '云台'),
          _gimbalPanel(context),
          const SizedBox(height: MgMetrics.configSectionGap),
          _section(context, '视频源'),
          _videoPanel(context),
        ],
      ),
    );
  }

  Widget _section(BuildContext context, String title) => Padding(
    padding: const EdgeInsets.only(bottom: MgMetrics.configHeaderGap),
    child: Text(
      title,
      style: MgTypography.sans(
        MgTypography.section.copyWith(color: context.mgColors.textPrimary),
      ),
    ),
  );

  Widget _cameraPanel(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    final status = controller.cameraStatus;
    final cameras = controller.cameras;
    final enabled = controller.connected && !controller.busy;
    // 只有已选中的相机上报了能力标志时才据此禁用按钮；没有详情时按能力
    // 未知处理，允许用户尝试（native 会照实返回失败）。
    final detail = status != null && status.available;
    final canPhoto = !detail || status.hasPhoto;
    final canVideo = !detail || status.hasVideo;
    return MacPanel(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (cameras.isEmpty) ...[
            _emptyHint(
              context,
              controller.connected
                  ? '未发现相机。相机上报 CAMERA_INFORMATION 后会出现在这里。'
                  : '未连接飞控。',
            ),
            const SizedBox(height: 10),
            Align(
              alignment: Alignment.centerLeft,
              child: PushButton(
                controlSize: ControlSize.regular,
                onPressed: enabled ? controller.requestCameraInfo : null,
                child: const Text('请求相机上报'),
              ),
            ),
          ] else ...[
            AlignedFieldRow(
              label: '相机',
              labelWidth: 72,
              child: AdaptivePopupButton<int?>(
                value: controller.selectedCameraId,
                options: [null, for (final c in cameras) c.componentId],
                labelOf: (id) => _cameraLabel(cameras, id),
                onChanged: enabled ? controller.selectCamera : null,
              ),
            ),
            const SizedBox(height: 8),
            if (status != null && status.available) ...[
              _readout(context, '型号', _cameraModel(status)),
              _readout(context, '分辨率', _resolution(status)),
              _readout(
                context,
                '状态',
                status.recording ? '录像中' : (status.takingPhoto ? '拍照中' : '待机'),
              ),
              _readout(context, '已拍张数', '${status.photoCount}'),
              _readout(context, '变焦', status.zoomLevel.toStringAsFixed(1)),
              _readout(context, '模式', _modeLabel(status.modeId)),
              _readout(context, '存储', _storageText(status)),
            ] else
              _emptyHint(context, '这台相机暂未上报设置/拍摄状态。'),
            const SizedBox(height: 10),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled && canPhoto ? controller.takePhoto : null,
                  child: const Text('拍照'),
                ),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled && canVideo
                      ? controller.startRecording
                      : null,
                  child: const Text('开始录像'),
                ),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled ? controller.stopRecording : null,
                  child: const Text('停止录像'),
                ),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled ? controller.stopCapture : null,
                  child: const Text('停止拍摄'),
                ),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled ? controller.requestCameraInfo : null,
                  child: const Text('请求信息'),
                ),
              ],
            ),
            const SizedBox(height: 6),
            AlignedFieldRow(
              label: '变焦',
              labelWidth: 72,
              child: Row(
                children: [
                  Expanded(
                    child: MacosTextField(
                      controller: _zoom,
                      placeholder:
                          status?.zoomLevel.toStringAsFixed(1) ?? '1.0',
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 4,
                      ),
                      style: MgTypography.sans(
                        MgTypography.body.copyWith(color: colors.textPrimary),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  PushButton(
                    controlSize: ControlSize.regular,
                    onPressed: enabled ? _applyZoom : null,
                    child: const Text('应用'),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 6),
            AlignedFieldRow(
              label: '模式',
              labelWidth: 72,
              child: AdaptivePopupButton<int>(
                value: status != null && status.modeId >= 0 && status.modeId < 3
                    ? status.modeId
                    : null,
                options: const [0, 1, 2],
                labelOf: _modeLabel,
                onChanged: enabled ? (mode) => controller.setMode(mode) : null,
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _gimbalPanel(BuildContext context) {
    final controller = widget.controller;
    final gimbals = controller.gimbals;
    final status = controller.gimbalStatus;
    final attitude = controller.gimbalAttitude;
    final enabled = controller.connected && !controller.busy;
    return MacPanel(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (gimbals.isEmpty) ...[
            _emptyHint(
              context,
              controller.connected
                  ? '未发现云台。云台上报 GIMBAL_MANAGER_INFORMATION 后会出现在这里。'
                  : '未连接飞控。',
            ),
            const SizedBox(height: 10),
            Align(
              alignment: Alignment.centerLeft,
              child: PushButton(
                controlSize: ControlSize.regular,
                onPressed: enabled ? controller.requestGimbalInfo : null,
                child: const Text('请求云台上报'),
              ),
            ),
          ] else ...[
            AlignedFieldRow(
              label: '云台',
              labelWidth: 72,
              child: AdaptivePopupButton<int?>(
                value: controller.selectedGimbalId,
                options: [null, for (final g in gimbals) g.componentId],
                labelOf: (id) => _gimbalLabel(gimbals, id),
                onChanged: enabled ? controller.selectGimbal : null,
              ),
            ),
            const SizedBox(height: 8),
            if (attitude != null && attitude.available) ...[
              _readout(
                context,
                '姿态',
                'roll ${attitude.rollDeg.toStringAsFixed(1)}°  '
                    'pitch ${attitude.pitchDeg.toStringAsFixed(1)}°  '
                    'yaw ${attitude.yawDeg.toStringAsFixed(1)}°',
              ),
            ] else
              _emptyHint(context, '暂未收到云台姿态（GIMBAL_DEVICE_ATTITUDE_STATUS）。'),
            if (status != null && status.available) ...[
              _readout(context, '厂商 / 型号', _gimbalModel(status)),
              _readout(
                context,
                '限位',
                'pitch ${status.pitchMin.toStringAsFixed(0)}..'
                    '${status.pitchMax.toStringAsFixed(0)}°  '
                    'yaw ${status.yawMin.toStringAsFixed(0)}..'
                    '${status.yawMax.toStringAsFixed(0)}°',
              ),
            ],
            const SizedBox(height: 6),
            AlignedFieldRow(
              label: '指向',
              labelWidth: 72,
              child: Row(
                children: [
                  Expanded(
                    child: _numberField(context, _pitch, placeholder: 'pitch°'),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: _numberField(context, _yaw, placeholder: 'yaw°'),
                  ),
                  const SizedBox(width: 8),
                  PushButton(
                    controlSize: ControlSize.regular,
                    onPressed: enabled ? _applyGimbal : null,
                    child: const Text('指向'),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 10),
            Align(
              alignment: Alignment.centerLeft,
              child: PushButton(
                controlSize: ControlSize.regular,
                onPressed: enabled ? controller.gimbalRecenter : null,
                child: const Text('回中'),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _videoPanel(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    final status = controller.videoStatus;
    final streams = controller.videoStreams;
    final renderer = controller.videoRendererStatus;
    final enabled = controller.connected && !controller.busy;
    final backendAvailable = renderer?.available ?? false;
    return MacPanel(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (!backendAvailable)
            MgInlineNotice(
              icon: CupertinoIcons.info_circle,
              color: colors.textSecondary,
              text:
                  '本平台未启用本地视频解码（此构建未包含 LGPL FFmpeg 后端）。'
                  '只能读取并显示流元数据，「启动视频源」会明确返回「未支持」，'
                  '不会静默做别的事。',
            )
          else ...[
            _readout(
              context,
              '解码后端',
              '${renderer!.backend} · ${renderer.running ? '正在播放' : '已停止'}',
            ),
            if (renderer.running && renderer.uri.isNotEmpty)
              _readout(context, '播放地址', renderer.uri),
            _readout(
              context,
              '画面',
              renderer.hasFrame
                  ? '${renderer.width}×${renderer.height} · 第 ${renderer.sequence} 帧'
                  : '等待首个视频帧…',
            ),
            const SizedBox(height: 8),
            VideoPreview(controller: controller),
          ],
          const SizedBox(height: 10),
          AlignedFieldRow(
            label: '视频源',
            labelWidth: 72,
            child: Row(
              children: [
                Expanded(
                  child: MacosTextField(
                    controller: _videoUri,
                    placeholder: 'rtsp://… / udp://…',
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 4,
                    ),
                    style: MgTypography.sans(
                      MgTypography.body.copyWith(color: colors.textPrimary),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled
                      ? () => controller.startVideo(_videoUri.text)
                      : null,
                  child: const Text('启动'),
                ),
                const SizedBox(width: 8),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: enabled ? controller.stopVideo : null,
                  child: const Text('停止'),
                ),
              ],
            ),
          ),
          if (backendAvailable) ...[
            const SizedBox(height: 12),
            _recordingSection(context, colors),
          ],
          const SizedBox(height: 10),
          if (streams.isEmpty && !(status?.available ?? false))
            _emptyHint(
              context,
              controller.connected
                  ? '未收到视频流信息（VIDEO_STREAM_INFORMATION）。'
                  : '未连接飞控。',
            )
          else
            for (final stream in streams)
              Padding(
                padding: const EdgeInsets.only(bottom: MgMetrics.configCardGap),
                child: _StreamRow(stream: stream),
              ),
        ],
      ),
    );
  }

  /// 当前输入框里的落盘设置（未保存也用于发起录制，避免"改了目录却没保存"的歧义）。
  VideoStorageSettings get _currentStorage => VideoStorageSettings(
    directory: _recordingDir.text.trim(),
    limitEnabled: _limitEnabled,
    maxMegabytes: clampVideoLimit(
      int.tryParse(_limitMb.text.trim()) ?? kDefaultVideoLimitMegabytes,
    ),
  );

  Future<void> _loadStorage() async {
    final settings = await widget.storageLoader();
    if (!mounted) return;
    setState(() {
      _recordingDir.text = settings.directory;
      _limitMb.text = settings.maxMegabytes.toString();
      _limitEnabled = settings.limitEnabled;
      _storageLoaded = true;
    });
  }

  Future<void> _saveStorage() async {
    final settings = _currentStorage;
    await settings.save();
    if (!mounted) return;
    setState(() {
      // 把上限回填成规范化后的值，让"保存"有可见结果。
      _limitMb.text = settings.maxMegabytes.toString();
      _storageSaved = true;
    });
  }

  /// 本地录制与截图：把正在播放的码流写进本地文件，按需做容量清理。
  ///
  /// 这些是本机操作，不依赖飞控连接；只要求编译进了解码后端。
  Widget _recordingSection(BuildContext context, MgColors colors) {
    final controller = widget.controller;
    final status = controller.recordingStatus;
    final recording = controller.isRecording;
    final ready = _storageLoaded && !controller.busy;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 10),
          child: Container(height: 1, color: colors.divider),
        ),
        _readout(
          context,
          '本地录制',
          recording
              ? '录制中 · ${_formatDuration(status?.durationUs ?? 0)} · '
                    '${_formatBytes(status?.bytes ?? 0)}'
              : '未录制',
        ),
        if (recording && (status?.path.isNotEmpty ?? false))
          _readout(context, '文件', status!.path),
        if (!recording && (status?.error.isNotEmpty ?? false))
          _readout(context, '上次错误', status!.error),
        const SizedBox(height: 8),
        AlignedFieldRow(
          label: '录制目录',
          labelWidth: 72,
          child: Row(
            children: [
              Expanded(
                child: MacosTextField(
                  controller: _recordingDir,
                  placeholder: '录制与截图的保存目录',
                  padding: const EdgeInsets.symmetric(
                    horizontal: 8,
                    vertical: 4,
                  ),
                  onChanged: (_) {
                    if (_storageSaved) setState(() => _storageSaved = false);
                  },
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(color: colors.textPrimary),
                  ),
                ),
              ),
              const SizedBox(width: 8),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: _storageLoaded ? _saveStorage : null,
                child: const Text('保存'),
              ),
            ],
          ),
        ),
        const SizedBox(height: 6),
        AlignedFieldRow(
          label: '容量上限',
          labelWidth: 72,
          child: Row(
            children: [
              MacosCheckbox(
                value: _limitEnabled,
                onChanged: _storageLoaded
                    ? (value) => setState(() {
                        _limitEnabled = value;
                        _storageSaved = false;
                      })
                    : null,
              ),
              const SizedBox(width: 8),
              Text(
                '超出时删除最旧的录制',
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textSecondary),
                ),
              ),
              const SizedBox(width: 8),
              SizedBox(
                width: 84,
                child: MacosTextField(
                  controller: _limitMb,
                  placeholder: '$kDefaultVideoLimitMegabytes',
                  padding: const EdgeInsets.symmetric(
                    horizontal: 8,
                    vertical: 4,
                  ),
                  onChanged: (_) {
                    if (_storageSaved) setState(() => _storageSaved = false);
                  },
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(color: colors.textPrimary),
                  ),
                ),
              ),
              const SizedBox(width: 4),
              Text(
                'MB',
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textSecondary),
                ),
              ),
            ],
          ),
        ),
        if (_storageSaved)
          Padding(
            padding: const EdgeInsets.only(top: 4),
            child: MgInlineNotice(
              icon: CupertinoIcons.checkmark_circle,
              color: colors.success,
              text: '已保存录制设置',
            ),
          ),
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: ready && !recording
                  ? () => controller.startLocalRecording(_currentStorage)
                  : null,
              child: const Text('开始录制'),
            ),
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: ready && recording
                  ? controller.stopLocalRecording
                  : null,
              child: const Text('停止录制'),
            ),
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: ready
                  ? () => controller.captureLocalFrame(_currentStorage)
                  : null,
              child: const Text('截图'),
            ),
          ],
        ),
      ],
    );
  }

  /// `mm:ss`（超过一小时显示 `h:mm:ss`）。
  static String _formatDuration(int microseconds) {
    final totalSeconds = microseconds <= 0 ? 0 : microseconds ~/ 1000000;
    final hours = totalSeconds ~/ 3600;
    final minutes = (totalSeconds % 3600) ~/ 60;
    final seconds = totalSeconds % 60;
    String two(int value) => value.toString().padLeft(2, '0');
    return hours > 0
        ? '$hours:${two(minutes)}:${two(seconds)}'
        : '${two(minutes)}:${two(seconds)}';
  }

  /// 人类可读的文件大小（KB/MB/GB，保留一位小数）。
  static String _formatBytes(int bytes) {
    if (bytes < 1024) return '$bytes B';
    const units = ['KB', 'MB', 'GB', 'TB'];
    var value = bytes / 1024;
    var unit = 0;
    while (value >= 1024 && unit < units.length - 1) {
      value /= 1024;
      unit++;
    }
    return '${value.toStringAsFixed(1)} ${units[unit]}';
  }

  Widget _readout(BuildContext context, String label, String value) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 3),
    child: AlignedFieldRow(
      label: label,
      labelWidth: 72,
      child: Text(
        value,
        maxLines: 2,
        overflow: TextOverflow.ellipsis,
        style: MgTypography.sans(
          MgTypography.body.copyWith(color: context.mgColors.textPrimary),
        ),
      ),
    ),
  );

  Widget _emptyHint(BuildContext context, String text) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 10),
    child: Text(
      text,
      style: MacosTheme.of(
        context,
      ).typography.body.copyWith(color: context.mgColors.textSecondary),
    ),
  );

  Widget _numberField(
    BuildContext context,
    TextEditingController controller, {
    required String placeholder,
  }) => MacosTextField(
    controller: controller,
    placeholder: placeholder,
    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
    style: MgTypography.sans(
      MgTypography.body.copyWith(color: context.mgColors.textPrimary),
    ),
  );

  void _applyZoom() {
    final value = double.tryParse(_zoom.text.trim());
    if (value == null) {
      // 无效输入不走 backend，避免发送无意义的 0；提示走页面的统一通知通道。
      widget.controller.noteInvalidInput('请输入有效的变焦倍数');
      return;
    }
    widget.controller.setZoom(value);
  }

  void _applyGimbal() {
    final pitch = double.tryParse(_pitch.text.trim());
    final yaw = double.tryParse(_yaw.text.trim());
    if (pitch == null || yaw == null) {
      widget.controller.noteInvalidInput('请输入有效的 pitch / yaw 角度');
      return;
    }
    widget.controller.gimbalControl(pitchDeg: pitch, yawDeg: yaw);
  }

  static String _cameraLabel(List<CameraDevice> cameras, int? id) {
    if (id == null) return '自动（第一台）';
    for (final camera in cameras) {
      if (camera.componentId == id) {
        return '${camera.displayName}（comp ${camera.componentId}）';
      }
    }
    return '相机 #$id';
  }

  static String _gimbalLabel(List<GimbalDevice> gimbals, int? id) {
    if (id == null) return '自动（第一台）';
    for (final gimbal in gimbals) {
      if (gimbal.componentId == id) {
        return '${gimbal.displayName}（comp ${gimbal.componentId}）';
      }
    }
    return '云台 #$id';
  }

  static String _cameraModel(CameraStatus status) {
    final vendor = status.vendorName.trim();
    final model = status.modelName.trim();
    final joined = [vendor, model].where((s) => s.isNotEmpty).join(' ');
    return joined.isEmpty ? '未知' : joined;
  }

  static String _gimbalModel(GimbalStatus status) {
    final vendor = status.deviceVendorName.trim();
    final model = status.deviceModelName.trim();
    final joined = [vendor, model].where((s) => s.isNotEmpty).join(' ');
    return joined.isEmpty ? '未知' : joined;
  }

  static String _resolution(CameraStatus status) =>
      status.resolutionH > 0 && status.resolutionV > 0
      ? '${status.resolutionH}×${status.resolutionV}'
      : '未知';

  static String _modeLabel(int mode) => switch (mode) {
    0 => '拍照模式',
    1 => '录像模式',
    2 => '测绘模式',
    _ => '未知（$mode）',
  };

  static String _storageText(CameraStatus status) {
    if (status.storage.isEmpty) return '无存储信息';
    final bucket = status.storage.first;
    final fraction = bucket.usedFraction;
    final percent = fraction == null ? '' : '（已用 ${(fraction * 100).round()}%）';
    return '${bucket.availableCapacityMib.toStringAsFixed(0)} MiB 可用$percent';
  }
}

/// 视频流元数据行：类型、分辨率、码率与地址。
class _StreamRow extends StatelessWidget {
  const _StreamRow({required this.stream});

  final VideoStreamInfo stream;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final resolution = stream.width > 0 && stream.height > 0
        ? '${stream.width}×${stream.height}'
        : '—';
    final details = [
      VideoStreamInfo.typeLabel(stream.type),
      resolution,
      if (stream.framerate > 0) '${stream.framerate.toStringAsFixed(0)} fps',
      if (stream.bitrate > 0)
        '${(stream.bitrate / 1000).toStringAsFixed(0)} kbps',
      stream.running ? 'running' : 'idle',
    ].join(' · ');
    return Row(
      children: [
        MacosIcon(
          CupertinoIcons.video_camera,
          size: 14,
          color: colors.textSecondary,
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                stream.displayName,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textPrimary),
                ),
              ),
              const SizedBox(height: 2),
              Text(
                stream.uri.isEmpty ? details : '$details · ${stream.uri}',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.textSecondary),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
