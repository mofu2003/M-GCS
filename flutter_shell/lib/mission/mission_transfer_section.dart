import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_command_confirmation.dart';
import '../widgets/mg_confirm.dart';
import '../widgets/mg_dialog.dart';
import '../widgets/mg_inline_notice.dart';
import 'mission_document_controller.dart';
import 'mission_document_models.dart';
import 'mission_export.dart';

/// 任务动作条、传输/校验展示与传输/执行命令。
///
/// 从 [MissionPlanningFloatingPanel] 抽出的自成一体的区块（KD-44）：只依赖任务
/// 文档控制器与连接控制器，不持有面板的标签、折叠、任务列表等状态。切车/会话
/// 门控仍由面板统一处理，这里只按传入的两个控制器渲染与发令。
class MissionTransferSection extends StatefulWidget {
  const MissionTransferSection({
    super.key,
    required this.mission,
    required this.controller,
    this.onRequestCommandConfirmation,
  });

  final MissionDocumentController mission;
  final ConnectionController controller;

  /// 把“开始任务”变成地图悬浮确认层的长按确认指令。
  final ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation;

  @override
  State<MissionTransferSection> createState() => _MissionTransferSectionState();
}

class _MissionTransferSectionState extends State<MissionTransferSection> {
  /// 导出成功后的内联提示；关闭或下一次导出时更新。
  String? _exportNotice;

  /// 命令守卫：失败信息由任务文档控制器的 status / error 呈现。
  Future<void> _mutate(Future<void> Function() operation) async {
    try {
      await operation();
    } on Object {
      // 失败信息由 controller 的 status / error 呈现。
    }
  }

  @override
  Widget build(BuildContext context) {
    final document = widget.mission.document;
    final colors = context.mgColors;
    final busy = widget.mission.busy;
    final transfer = widget.mission.transfer;
    final transferring = transfer?.active ?? false;
    final connected = widget.controller.isConnected;
    final status = widget.mission.status ?? widget.mission.error;
    final showStatus = status != null && !_isRoutineStatus(status);
    final verification = widget.mission.verification;
    final diagnostics = (widget.mission.validation?.diagnostics ?? const [])
        .where(
          (diagnostic) => diagnostic.severity != MissionDiagnosticSeverity.info,
        )
        .take(3)
        .toList(growable: false);
    final canStartMission =
        connected &&
        widget.mission.boundToVehicle &&
        widget.mission.boundSessionId == widget.controller.sessionId &&
        !busy &&
        !transferring &&
        document != null &&
        document.entities.isNotEmpty &&
        !widget.mission.dirty;
    final canRecoverVerification =
        connected && !busy && !transferring && document != null;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Wrap(
          spacing: 8,
          runSpacing: 8,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed: busy || transferring || !connected
                  ? null
                  : _downloadFromVehicle,
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  MacosIcon(CupertinoIcons.arrow_down_to_line, size: 13),
                  SizedBox(width: 6),
                  Text('从飞控读取'),
                ],
              ),
            ),
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed:
                  busy ||
                      transferring ||
                      !connected ||
                      document == null ||
                      !widget.mission.boundToVehicle ||
                      widget.mission.boundSessionId !=
                          widget.controller.sessionId
                  ? null
                  : _uploadToVehicle,
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  MacosIcon(CupertinoIcons.arrow_up_to_line, size: 13),
                  SizedBox(width: 6),
                  Text('上传到飞控'),
                ],
              ),
            ),
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed: busy || transferring || !connected
                  ? null
                  : _clearVehicleMission,
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  MacosIcon(CupertinoIcons.trash, size: 13),
                  SizedBox(width: 6),
                  Text('清除飞控任务'),
                ],
              ),
            ),
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed: busy || transferring || document == null
                  ? null
                  : _exportMission,
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  MacosIcon(CupertinoIcons.square_stack_3d_up, size: 13),
                  SizedBox(width: 6),
                  Text('导出文件'),
                ],
              ),
            ),
            MacosTooltip(
              message: widget.mission.dirty ? '请先上传任务' : '解锁并开始任务',
              child: PushButton(
                controlSize: ControlSize.regular,
                onPressed: canStartMission ? _startMission : null,
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    MacosIcon(CupertinoIcons.play_fill, size: 12),
                    SizedBox(width: 5),
                    Text('开始任务'),
                  ],
                ),
              ),
            ),
            if (widget.mission.dirty)
              Text(
                '未上传',
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.warning),
              ),
            if (transferring)
              PushButton(
                controlSize: ControlSize.regular,
                secondary: true,
                onPressed: widget.mission.cancelTransfer,
                child: const Text('取消'),
              )
            else if (busy)
              const ProgressCircle(radius: 7),
          ],
        ),
        if (transfer != null &&
            (transfer.active ||
                transfer.cancelled ||
                transfer.error.isNotEmpty)) ...[
          const SizedBox(height: 8),
          ProgressBar(
            value: transfer.progressPercent.toDouble(),
            height: 6,
            semanticLabel: _transferLabel(transfer),
          ),
          const SizedBox(height: 4),
          Text(
            _transferLabel(transfer),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(
              context,
            ).typography.caption1.copyWith(color: colors.textSecondary),
          ),
        ],
        if (showStatus) ...[
          const SizedBox(height: 6),
          Text(
            status,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(
              context,
            ).typography.caption1.copyWith(color: colors.textSecondary),
          ),
        ],
        if (_exportNotice != null) ...[
          const SizedBox(height: 6),
          MgInlineNotice(
            icon: CupertinoIcons.checkmark_circle,
            color: colors.success,
            text: _exportNotice!,
            trailing: MgDismissButton(
              onPressed: () => setState(() => _exportNotice = null),
            ),
          ),
        ],
        if (verification != null &&
            !verification.matches &&
            verification.differences.isNotEmpty) ...[
          const SizedBox(height: 8),
          _MissionVerificationPanel(
            result: verification,
            onSelectIndex: (index) {
              final entities = widget.mission.document?.entities ?? const [];
              if (index >= 0 && index < entities.length) {
                widget.mission.selectEntity(index);
              }
            },
            fieldLabel: _verificationFieldLabel,
            onAdoptVehicle: canRecoverVerification
                ? () => _adoptVehicleMission(context)
                : null,
            onRetryUpload: canRecoverVerification
                ? () => unawaited(_uploadToVehicle())
                : null,
          ),
        ],
        if (verification != null && verification.homeSync.isNotEmpty) ...[
          const SizedBox(height: 8),
          _HomeSyncNotice(
            result: verification,
            fieldLabel: _verificationFieldLabel,
            onAdoptVehicleHome: canRecoverVerification
                ? () => unawaited(
                    widget.mission.adoptVehicleHomeFromVerification(),
                  )
                : null,
            onKeepLocalHome: widget.mission.dismissVerification,
          ),
        ],
        if (diagnostics.isNotEmpty) ...[
          const SizedBox(height: 6),
          for (final diagnostic in diagnostics)
            _MissionDiagnosticRow(
              diagnostic: diagnostic,
              onTap: diagnostic.entityId.isEmpty
                  ? null
                  : () => widget.mission.selectEntityById(diagnostic.entityId),
            ),
        ],
        const SizedBox(height: 4),
        Text(
          _estimateSummary(),
          style: MacosTheme.of(
            context,
          ).typography.caption1.copyWith(color: colors.textTertiary),
        ),
      ],
    );
  }

  String _transferLabel(MissionTransferProgress transfer) {
    if (transfer.cancelled) return '任务传输已取消';
    if (transfer.error.isNotEmpty) return transfer.error;
    final base = switch (transfer.state) {
      MissionTransferState.downloading =>
        '正在读取 ${transfer.received}/${transfer.total}',
      MissionTransferState.uploading =>
        '正在上传 ${transfer.sent}/${transfer.total}',
      MissionTransferState.waitingAck => '正在等待飞控确认',
      MissionTransferState.idle => '传输完成 ${transfer.progressPercent}%',
    };
    return transfer.retryCount > 0
        ? '$base · 已重试 ${transfer.retryCount} 次'
        : base;
  }

  bool _isRoutineStatus(String status) =>
      status.startsWith('正在初始化任务文档') ||
      status.startsWith('正在更新任务') ||
      status.startsWith('任务已更新') ||
      status.startsWith('正在估算任务') ||
      status.startsWith('正在校验任务');

  String _estimateSummary() {
    final estimate = widget.mission.estimate;
    final count = widget.mission.document?.entities.length ?? 0;
    if (estimate == null) return '$count 个任务项';
    final distanceKm = estimate.totalDistanceMeters / 1000.0;
    final minutes = estimate.estimatedTimeSeconds / 60.0;
    return '$count 个任务项 · ${distanceKm.toStringAsFixed(2)} km · '
        '${minutes.toStringAsFixed(1)} min';
  }

  String _verificationFieldLabel(String field) => switch (field) {
    'count' => '任务数量',
    'home' => 'Home',
    'home.latitude' => 'Home 纬度',
    'home.longitude' => 'Home 经度',
    'home.altitude_m' => 'Home 高度',
    'command' => '命令',
    'frame' => '坐标帧',
    'autocontinue' => '自动继续',
    'mission_type' => '任务类型',
    'x' => '纬度',
    'y' => '经度',
    'z' => '高度',
    'param1' || 'param2' || 'param3' || 'param4' => field.toUpperCase(),
    _ => field,
  };

  /// 从飞控读取：本地已有任务项时先确认（照抄 MP 的
  /// “This will clear your existing points, Continue?” 与 QGC 的下载前提示）。
  Future<void> _downloadFromVehicle() async {
    final localCount = widget.mission.document?.entities.length ?? 0;
    if (widget.mission.dirty) {
      final confirmed = await showMgConfirmDialog(
        context,
        title: '从飞控读取任务',
        message: '本地有未上传的修改（$localCount 个任务项），读取会覆盖它们。是否继续？',
        confirmLabel: '继续读取',
      );
      if (!confirmed) return;
    }
    await _mutate(widget.mission.downloadFromVehicle);
  }

  /// 清空飞控任务：破坏性操作，先确认（MP 的 Clear 与 QGC 的 Remove 都会问）。
  Future<void> _clearVehicleMission() async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '清除飞控任务？',
      message: '这会删除飞控上已保存的任务，本地任务文档保持不变。',
      confirmLabel: '清除飞控任务',
    );
    if (!confirmed) return;
    await _mutate(widget.mission.clearVehicleMission);
  }

  /// 导出当前任务文档到文件（`waypoints` / `plan`），不改动本地文档与飞控。
  Future<void> _exportMission() async {
    final result = await showMissionExportSheet(
      context: context,
      controller: widget.mission,
    );
    if (result == null || !mounted) return;
    final warnings = result.warnings.isEmpty
        ? ''
        : '（${result.warnings.length} 条提示）';
    setState(() {
      _exportNotice = '已导出 ${result.itemCount} 条到 ${result.path}$warnings';
    });
  }

  /// 上传任务到飞控：会覆盖飞控上已保存的任务，先确认（KD-08）。
  Future<void> _uploadToVehicle() async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '上传任务到飞控？',
      message: '这会把本地任务写入飞控，覆盖飞控上已保存的任务。上传完成后需再单独“执行任务”。',
      confirmLabel: '上传任务',
    );
    if (!confirmed) return;
    await _mutate(() async {
      await widget.mission.upload();
    });
  }

  Future<void> _adoptVehicleMission(BuildContext context) async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '采用飞控任务？',
      message: '这会用最近一次从飞控回读的任务覆盖本地任务修改。',
      confirmLabel: '采用飞控任务',
    );
    if (!confirmed) return;
    await _mutate(widget.mission.adoptDownloadedVehicleMission);
  }

  Future<void> _startMission() async {
    if (widget.mission.dirty) {
      await showMgConfirmDialog(
        context,
        title: '任务尚未上传',
        message: '当前有未上传修改。请先上传任务，再执行任务。',
        confirmLabel: '知道了',
      );
      return;
    }

    final request = widget.onRequestCommandConfirmation;
    if (request != null) {
      request(
        MgCommandConfirmation(label: '开始任务', execute: _executeStartMission),
      );
      return;
    }

    // 没有悬浮确认层时退回对话框确认。任务启动会立刻让飞机按航线运动，
    // 不允许存在"缺确认组件就直发"的降级路径。
    final confirmed = await showMgConfirmDialog(
      context,
      title: '开始任务',
      message: '飞控将按当前任务开始执行，飞机随即开始运动。',
      confirmLabel: '开始任务',
    );
    if (!confirmed || !mounted) return;
    await _executeStartMission();
  }

  Future<void> _executeStartMission() async {
    try {
      await widget.controller.startMission();
    } on Object catch (error) {
      if (!mounted) rethrow;
      await showMgAlertDialog<void>(
        context: context,
        builder: (dialogContext) => MacosAlertDialog(
          appIcon: MacosIcon(
            CupertinoIcons.xmark_circle_fill,
            size: 38,
            color: dialogContext.mgColors.danger,
          ),
          title: const Text('任务启动失败'),
          message: Text(error.toString().replaceFirst('Bad state: ', '')),
          primaryButton: PushButton(
            controlSize: ControlSize.large,
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text('知道了'),
          ),
        ),
      );
    }
  }
}

class _HomeSyncNotice extends StatelessWidget {
  const _HomeSyncNotice({
    required this.result,
    required this.fieldLabel,
    required this.onAdoptVehicleHome,
    required this.onKeepLocalHome,
  });

  final MissionVerificationResult result;
  final String Function(String field) fieldLabel;
  final VoidCallback? onAdoptVehicleHome;
  final VoidCallback onKeepLocalHome;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final visible = result.homeSync.take(3);
    return Container(
      padding: const EdgeInsets.fromLTRB(8, 7, 8, 7),
      decoration: BoxDecoration(
        color: colors.accentSurface,
        border: Border.all(color: colors.accent.withValues(alpha: 0.38)),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MacosIcon(
                CupertinoIcons.location,
                size: 13,
                color: colors.accent,
              ),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  '飞控 Home 已自动更新',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.accent),
                  ),
                ),
              ),
              Text(
                '${result.homeSyncCount} 项同步提示',
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textSecondary),
                ),
              ),
            ],
          ),
          const SizedBox(height: 5),
          for (final item in visible)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Text(
                '${fieldLabel(item.field)}：${item.expected} → ${item.actual}',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textPrimary),
                ),
              ),
            ),
          const SizedBox(height: 7),
          Wrap(
            spacing: 7,
            runSpacing: 5,
            children: [
              if (onAdoptVehicleHome != null)
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: onAdoptVehicleHome,
                  child: const Text('采用飞控 Home'),
                ),
              PushButton(
                controlSize: ControlSize.regular,
                secondary: true,
                onPressed: onKeepLocalHome,
                child: const Text('保留本地 Home'),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _MissionVerificationPanel extends StatelessWidget {
  const _MissionVerificationPanel({
    required this.result,
    required this.onSelectIndex,
    required this.fieldLabel,
    this.onAdoptVehicle,
    this.onRetryUpload,
  });

  final MissionVerificationResult result;
  final ValueChanged<int> onSelectIndex;
  final String Function(String field) fieldLabel;
  final VoidCallback? onAdoptVehicle;
  final VoidCallback? onRetryUpload;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    const maxVisibleDifferences = 6;
    final visible = result.differences.take(maxVisibleDifferences);
    final remaining = result.differences.length - visible.length;
    return Container(
      padding: const EdgeInsets.fromLTRB(8, 7, 8, 7),
      decoration: BoxDecoration(
        color: colors.warning.withValues(alpha: 0.08),
        border: Border.all(color: colors.warning.withValues(alpha: 0.45)),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MacosIcon(
                CupertinoIcons.exclamationmark_triangle_fill,
                size: 13,
                color: colors.warning,
              ),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  '回读发现 ${result.differenceCount} 处差异',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.warning),
                  ),
                ),
              ),
              Text(
                '${result.expectedCount} → ${result.vehicleCount} 项',
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textSecondary),
                ),
              ),
            ],
          ),
          const SizedBox(height: 5),
          for (final difference in visible)
            GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: difference.index < 0
                  ? null
                  : () => onSelectIndex(difference.index),
              child: Padding(
                padding: const EdgeInsets.symmetric(vertical: 2),
                child: Row(
                  children: [
                    SizedBox(
                      width: 38,
                      child: Text(
                        difference.index < 0
                            ? 'Home'
                            : '第 ${difference.index + 1} 项',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                    ),
                    const SizedBox(width: 5),
                    Expanded(
                      child: Text(
                        '${fieldLabel(difference.field)}：${difference.expected} → ${difference.actual}',
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textPrimary,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          if (remaining > 0)
            Text(
              '还有 $remaining 处差异，点击对应任务项可定位明细。',
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: colors.textSecondary),
              ),
            ),
          if (onAdoptVehicle != null || onRetryUpload != null) ...[
            const SizedBox(height: 7),
            Wrap(
              spacing: 7,
              runSpacing: 5,
              children: [
                if (onAdoptVehicle != null)
                  PushButton(
                    controlSize: ControlSize.regular,
                    onPressed: onAdoptVehicle,
                    child: const Text('采用飞控任务'),
                  ),
                if (onRetryUpload != null)
                  PushButton(
                    controlSize: ControlSize.regular,
                    secondary: true,
                    onPressed: onRetryUpload,
                    child: const Text('重新上传本地任务'),
                  ),
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _MissionDiagnosticRow extends StatelessWidget {
  const _MissionDiagnosticRow({required this.diagnostic, required this.onTap});

  final MissionDiagnostic diagnostic;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final color = diagnostic.severity == MissionDiagnosticSeverity.error
        ? colors.danger
        : colors.warning;
    return GestureDetector(
      onTap: onTap,
      behavior: HitTestBehavior.opaque,
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 2),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Padding(
              padding: const EdgeInsets.only(top: 1),
              child: MacosIcon(
                diagnostic.severity == MissionDiagnosticSeverity.error
                    ? CupertinoIcons.xmark_circle_fill
                    : CupertinoIcons.exclamationmark_triangle_fill,
                size: 12,
                color: color,
              ),
            ),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                _message(),
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: color),
              ),
            ),
          ],
        ),
      ),
    );
  }

  String _message() => switch (diagnostic.code) {
    'schema.unsupported' => '任务文档版本不受支持',
    'document.id_missing' => '任务文档缺少 ID',
    'defaults.altitude' => '默认高度必须大于 0',
    'defaults.speed' => '默认速度必须大于 0',
    'home.missing' => '未设置 Home，相对高度预览不可靠',
    'home.latitude' || 'home.longitude' => 'Home 坐标无效',
    'entity.id_missing' => '任务项缺少 ID',
    'entity.id_duplicate' => '任务项 ID 重复',
    'entity.order' => '任务项顺序已重新整理',
    'entity.command_missing' => '任务项缺少命令',
    'entity.command_unsupported' => '命令不在支持列表中',
    'entity.frame_unsupported' => '不支持该坐标帧',
    'entity.autocontinue' => '自动继续值无效',
    'entity.parameter_non_finite' => '参数值不是有效数字',
    'entity.latitude' || 'entity.longitude' => '航点坐标无效或缺失',
    'entity.altitude' => '高度必须大于等于 0',
    'entity.altitude_high' => '高度异常偏高',
    'mission.empty' => '任务中没有启用的任务项',
    'mission.first_item' => '第一项建议为起飞或航点',
    'mission.last_item' => '最后一项建议为降落或返航',
    _ => diagnostic.message,
  };
}
