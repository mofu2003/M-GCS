import 'dart:async';
import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_inline_notice.dart';
import 'onboard_log_page.dart' show formatByteSize;
import 'tlog_controller.dart';
import 'tlog_models.dart';

/// 「飞行记录」页：链路 `.tlog` 录制开关 + 本地 `.tlog` 离线回放与消息检索。
///
/// 录制挂在活动车辆上，未连接时按钮禁用；回放只读本地文件，断开也能用。
/// 完成、截断、失败都不推测，只按 native `mg_tlog_status` 与
/// `mg_tlog_replay_*` 的返回呈现。
class FlightLogPage extends StatefulWidget {
  const FlightLogPage({super.key, required this.controller});

  final TlogController controller;

  @override
  State<FlightLogPage> createState() => _FlightLogPageState();
}

class _FlightLogPageState extends State<FlightLogPage> {
  final _pathController = TextEditingController();
  final _filterController = TextEditingController();
  double? _seekValue;
  Timer? _seekTimer;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      widget.controller.refreshRecordingStatus();
      widget.controller.refreshLocalFiles();
    });
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onChanged);
    _seekTimer?.cancel();
    _pathController.dispose();
    _filterController.dispose();
    super.dispose();
  }

  void _onChanged() {
    if (mounted) setState(() {});
  }

  /// 拖动滑杆时只更新本地指示，停手 300ms 后才真正 seek，
  /// 避免每帧都过一次 FFI（跳转会清空已读消息并重读一批）。
  void _onSeekChanged(double value) {
    setState(() => _seekValue = value);
    _seekTimer?.cancel();
    _seekTimer = Timer(const Duration(milliseconds: 300), () {
      if (!mounted) return;
      widget.controller.seekReplay(value);
    });
  }

  Future<void> _openPath(String path) async {
    await widget.controller.openReplay(path);
    if (!mounted) return;
    if (widget.controller.session != null) {
      _pathController.text = path;
      _seekValue = null;
      if (_filterController.text.isNotEmpty) {
        _filterController.clear();
        widget.controller.setFilter('');
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    final session = controller.session;
    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(
        MgMetrics.configPageMargin,
        22,
        MgMetrics.configPageMargin,
        32,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _RecordingSection(
            controller: controller,
            onToggle: controller.isRecording
                ? controller.stopRecording
                : controller.startRecording,
          ),
          const SizedBox(height: 24),
          Row(
            children: [
              Text('本地回放', style: MgTypography.sans(MgTypography.section)),
              const SizedBox(width: 12),
              if (controller.loadingFiles)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CupertinoActivityIndicator(radius: 7),
                ),
              const Spacer(),
              if (session != null)
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: controller.busy
                      ? null
                      : () => controller.closeReplay(),
                  child: const Text('关闭回放'),
                ),
              const SizedBox(width: 8),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: controller.loadingFiles
                    ? null
                    : () => controller.refreshLocalFiles(),
                child: const Text('刷新文件'),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (controller.filesError case final message?)
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_circle,
              color: colors.warning,
              text: message,
            ),
          if (controller.replayError case final message?)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MgInlineNotice(
                icon: CupertinoIcons.exclamationmark_triangle,
                color: colors.danger,
                text: message,
                trailing: MgDismissButton(onPressed: controller.clearReplayError),
              ),
            ),
          const SizedBox(height: 12),
          _ManualOpenRow(
            pathController: _pathController,
            enabled: !controller.busy,
            onOpen: () => _openPath(_pathController.text),
          ),
          const SizedBox(height: 14),
          if (session == null)
            _FileList(
              files: controller.localFiles,
              activePath: null,
              enabled: !controller.busy,
              onOpen: _openPath,
            )
          else
            _ReplayView(
              controller: controller,
              session: session,
              filterController: _filterController,
              seekValue: _seekValue,
              onSeekChanged: _onSeekChanged,
            ),
        ],
      ),
    );
  }
}

class _RecordingSection extends StatelessWidget {
  const _RecordingSection({required this.controller, required this.onToggle});

  final TlogController controller;
  final Future<void> Function() onToggle;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final recording = controller.recording;
    final active = controller.isRecording;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Text('链路录制', style: MgTypography.sans(MgTypography.section)),
            const SizedBox(width: 12),
            _StatusPill(
              label: active ? '录制中' : '未录制',
              color: active ? colors.danger : colors.textSecondary,
            ),
            const Spacer(),
            PushButton(
              controlSize: ControlSize.regular,
              onPressed:
                  (!active && !controller.backend.isConnected) ? null : onToggle,
              child: Text(active ? '停止录制' : '开始录制'),
            ),
          ],
        ),
        const SizedBox(height: 10),
        Text(
          active
              ? '把链路上的 MAVLink 帧写入 ${recording.file}'
              : '开始后会把链路上的 MAVLink 帧写入本地 .tlog，停止时关闭文件。',
          style: MacosTheme.of(
            context,
          ).typography.caption1.copyWith(color: colors.textSecondary),
        ),
        if (active || recording.bytes > 0) ...[
          const SizedBox(height: 6),
          Text(
            '已写入 ${formatByteSize(recording.bytes)}',
            style: MacosTheme.of(context).typography.caption1.copyWith(
              color: colors.textSecondary,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
        ],
        if (controller.recordError case final message?)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_triangle,
              color: colors.warning,
              text: message,
              trailing: MgDismissButton(onPressed: controller.clearRecordError),
            ),
          ),
      ],
    );
  }
}

class _ManualOpenRow extends StatelessWidget {
  const _ManualOpenRow({
    required this.pathController,
    required this.enabled,
    required this.onOpen,
  });

  final TextEditingController pathController;
  final bool enabled;
  final VoidCallback onOpen;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Expanded(
        child: MacosTextField(
          controller: pathController,
          enabled: enabled,
          placeholder: '手动输入 .tlog 完整路径',
          onSubmitted: (_) => onOpen(),
        ),
      ),
      const SizedBox(width: 8),
      PushButton(
        controlSize: ControlSize.regular,
        onPressed: enabled ? onOpen : null,
        child: const Text('打开'),
      ),
    ],
  );
}

class _FileList extends StatelessWidget {
  const _FileList({
    required this.files,
    required this.activePath,
    required this.enabled,
    required this.onOpen,
  });

  final List<File> files;
  final String? activePath;
  final bool enabled;
  final Future<void> Function(String path) onOpen;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    if (files.isEmpty) {
      return Padding(
        padding: const EdgeInsets.symmetric(vertical: 24),
        child: Text(
          '默认日志目录里还没有 .tlog 文件。先开始一次链路录制，或手动输入路径。',
          textAlign: TextAlign.center,
          style: MacosTheme.of(
            context,
          ).typography.body.copyWith(color: colors.textSecondary),
        ),
      );
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (final file in files)
          Padding(
            padding: const EdgeInsets.only(bottom: MgMetrics.configCardGap),
            child: _FileRow(
              file: file,
              active: file.path == activePath,
              enabled: enabled,
              onOpen: () => onOpen(file.path),
            ),
          ),
      ],
    );
  }
}

class _FileRow extends StatelessWidget {
  const _FileRow({
    required this.file,
    required this.active,
    required this.enabled,
    required this.onOpen,
  });

  final File file;
  final bool active;
  final bool enabled;
  final VoidCallback onOpen;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final int bytes;
    final DateTime? modified;
    try {
      final stat = file.statSync();
      bytes = stat.size;
      modified = stat.modified;
    } on Object {
      return const SizedBox.shrink();
    }
    final name = file.uri.pathSegments.isNotEmpty
        ? file.uri.pathSegments.last
        : file.path;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: colors.surfaceMuted,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: active ? colors.accent : colors.divider),
      ),
      child: Row(
        children: [
          MacosIcon(
            CupertinoIcons.doc_text,
            size: 16,
            color: colors.textSecondary,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(name, style: MacosTheme.of(context).typography.body),
                const SizedBox(height: 2),
                Text(
                  '${_formatStamp(modified)} · ${file.path}',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: MacosTheme.of(
                    context,
                  ).typography.caption1.copyWith(color: colors.textSecondary),
                ),
              ],
            ),
          ),
          Text(
            formatByteSize(bytes),
            style: MacosTheme.of(context).typography.caption1.copyWith(
              color: colors.textSecondary,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
          const SizedBox(width: 16),
          PushButton(
            controlSize: ControlSize.regular,
            onPressed: enabled ? onOpen : null,
            child: Text(active ? '已打开' : '打开'),
          ),
        ],
      ),
    );
  }
}

class _ReplayView extends StatelessWidget {
  const _ReplayView({
    required this.controller,
    required this.session,
    required this.filterController,
    required this.seekValue,
    required this.onSeekChanged,
  });

  final TlogController controller;
  final TlogReplayFileInfo session;
  final TextEditingController filterController;
  final double? seekValue;
  final ValueChanged<double> onSeekChanged;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final visible = controller.visibleMessages;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _SessionCard(session: session, progress: controller.progress),
        if (session.truncated || controller.truncated)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_triangle,
              color: colors.warning,
              text: '文件末尾是半条记录，最后一段消息可能不完整（录制中途被中断）。',
            ),
          ),
        const SizedBox(height: 12),
        Row(
          children: [
            Expanded(
              child: MacosSlider(
                value: (seekValue ?? controller.progress).clamp(0.0, 1.0),
                semanticLabel: '回放位置',
                onChanged: onSeekChanged,
              ),
            ),
            const SizedBox(width: 8),
            Text(
              '${((seekValue ?? controller.progress) * 100).toStringAsFixed(0)}%',
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: colors.textSecondary,
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            Expanded(
              child: MacosTextField(
                controller: filterController,
                placeholder: '按消息名或 msgid 过滤，例如 HEARTBEAT / 33',
                onChanged: controller.setFilter,
              ),
            ),
            const SizedBox(width: 8),
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: controller.busy || controller.eof
                  ? null
                  : () => controller.stepReplay(),
              child: Text(controller.eof ? '已到末尾' : '读取下一批'),
            ),
          ],
        ),
        const SizedBox(height: 14),
        Row(
          children: [
            Text('消息', style: MgTypography.sans(MgTypography.section)),
            const SizedBox(width: 10),
            Text(
              '已读 ${controller.messages.length} 条'
              '${controller.filter.isEmpty ? '' : ' / 命中 ${visible.length} 条'}'
              '${controller.droppedMessages > 0 ? '（仅保留最近 $kTlogMaxRetainedMessages 条）' : ''}',
              style: MacosTheme.of(
                context,
              ).typography.caption1.copyWith(color: colors.textSecondary),
            ),
          ],
        ),
        const SizedBox(height: 10),
        if (visible.isEmpty)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 20),
            child: Text(
              controller.messages.isEmpty
                  ? '还没有读到消息。点「读取下一批」。'
                  : '没有匹配「${controller.filter}」的消息。',
              textAlign: TextAlign.center,
              style: MacosTheme.of(
                context,
              ).typography.body.copyWith(color: colors.textSecondary),
            ),
          )
        else
          for (final message in visible.reversed)
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: _MessageRow(message: message),
            ),
      ],
    );
  }
}

class _SessionCard extends StatelessWidget {
  const _SessionCard({required this.session, required this.progress});

  final TlogReplayFileInfo session;
  final double progress;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            session.file,
            style: MacosTheme.of(context).typography.body,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
          const SizedBox(height: 6),
          Text(
            '${session.messages} 条记录 · ${formatByteSize(session.bytes)} · '
            '时长 ${_formatDuration(session.duration)}',
            style: MacosTheme.of(
              context,
            ).typography.caption1.copyWith(color: colors.textSecondary),
          ),
          const SizedBox(height: 10),
          ProgressBar(
            value: progress * 100,
            semanticLabel: '回放进度 ${(progress * 100).toStringAsFixed(0)}%',
          ),
        ],
      ),
    );
  }
}

class _MessageRow extends StatelessWidget {
  const _MessageRow({required this.message});

  final TlogReplayMessage message;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final timestamp = message.timestamp;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        color: colors.surfaceMuted,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
      ),
      child: Row(
        children: [
          Expanded(
            child: Text(
              message.name,
              style: MacosTheme.of(context).typography.body,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          Text(
            'id ${message.msgid} · ${message.sysid}/${message.compid}',
            style: MacosTheme.of(
              context,
            ).typography.caption1.copyWith(color: colors.textSecondary),
          ),
          const SizedBox(width: 12),
          Text(
            timestamp == null ? '--:--:--' : _formatClock(timestamp),
            style: MacosTheme.of(context).typography.caption1.copyWith(
              color: colors.textSecondary,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
        ],
      ),
    );
  }
}

class _StatusPill extends StatelessWidget {
  const _StatusPill({required this.label, required this.color});

  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
    decoration: BoxDecoration(
      color: color.withValues(alpha: 0.14),
      borderRadius: BorderRadius.circular(6),
    ),
    child: Text(
      label,
      style: MacosTheme.of(
        context,
      ).typography.caption1.copyWith(color: color),
    ),
  );
}

String _formatStamp(DateTime? stamp) {
  if (stamp == null) return '修改时间未知';
  String two(int value) => value.toString().padLeft(2, '0');
  return '${stamp.year}-${two(stamp.month)}-${two(stamp.day)} '
      '${two(stamp.hour)}:${two(stamp.minute)}:${two(stamp.second)}';
}

String _formatClock(DateTime stamp) {
  String two(int value) => value.toString().padLeft(2, '0');
  return '${two(stamp.hour)}:${two(stamp.minute)}:${two(stamp.second)}'
      '.${stamp.millisecond.toString().padLeft(3, '0')}';
}

String _formatDuration(Duration duration) {
  if (duration <= Duration.zero) return '0s';
  final seconds = duration.inMilliseconds / 1000.0;
  if (seconds < 60) return '${seconds.toStringAsFixed(seconds < 10 ? 2 : 1)}s';
  final minutes = duration.inSeconds ~/ 60;
  final rest = duration.inSeconds % 60;
  return '${minutes}m ${rest}s';
}
