import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_confirm.dart';
import '../widgets/mg_floating_panel.dart';
import 'mission_document_controller.dart';
import 'mission_document_models.dart';

/// 飞控围栏与 Rally 的独立操作面板，不属于航点规划任务编辑流程。
/// 围栏/Rally 面板宽度。
const double kGeofencePanelWidth = 380;

class GeofenceFloatingPanel extends StatefulWidget {
  const GeofenceFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<GeofenceFloatingPanel> createState() => _GeofenceFloatingPanelState();
}

class _GeofenceFloatingPanelState extends State<GeofenceFloatingPanel> {
  late final MissionDocumentController _mission =
      widget.controller.missionPlanning;

  @override
  void initState() {
    super.initState();
    _mission.addListener(_handleMissionChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) unawaited(_mission.initialize());
    });
  }

  void _handleMissionChanged() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _mission.removeListener(_handleMissionChanged);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final status = _mission.status;
    final error = _mission.error;
    final enabled =
        widget.controller.isConnected &&
        !_mission.busy &&
        !_mission.transferActive;
    return SizedBox(
      width: kGeofencePanelWidth,
      child: MgFloatingPanel(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 10, 10, 10),
              child: Row(
                children: [
                  Text(
                    '围栏与集结点',
                    style: MacosTheme.of(context).typography.title3,
                  ),
                  const Spacer(),
                  MacosIconButton(
                    icon: const MacosIcon(CupertinoIcons.xmark, size: 13),
                    semanticLabel: '关闭围栏面板',
                    onPressed: widget.onClose,
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: colors.divider),
            Padding(
              padding: const EdgeInsets.all(14),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  _FenceRallyCard(mission: _mission, enabled: enabled),
                  if (_mission.busy) ...[
                    const SizedBox(height: 10),
                    const Center(child: ProgressCircle(radius: 8)),
                  ],
                  if (status != null && status != error) ...[
                    const SizedBox(height: 8),
                    Text(
                      status,
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                      style: MacosTheme.of(context).typography.caption1
                          .copyWith(color: colors.textSecondary),
                    ),
                  ],
                  if (error != null) ...[
                    const SizedBox(height: 6),
                    Text(
                      error,
                      maxLines: 3,
                      overflow: TextOverflow.ellipsis,
                      style: MacosTheme.of(
                        context,
                      ).typography.caption1.copyWith(color: colors.danger),
                    ),
                  ],
                  if (_mission.fenceVerification case final verification?)
                    _VerificationStatus(
                      label: '围栏',
                      verification: verification,
                    ),
                  if (_mission.rallyVerification case final verification?)
                    _VerificationStatus(
                      label: 'Rally',
                      verification: verification,
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FenceRallyCard extends StatelessWidget {
  const _FenceRallyCard({required this.mission, required this.enabled});

  final MissionDocumentController mission;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 10, 12, 10),
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _row(
            context,
            icon: CupertinoIcons.lock_shield,
            label: '围栏',
            hint: '围栏是独立的 MAVLink mission type，不随任务上传',
            onRead: mission.readFenceFromVehicle,
            onUpload: () => _confirmUpload(context, '围栏', mission.uploadFence),
            onClear: mission.clearFence,
            labelColor: colors.warning,
          ),
          const SizedBox(height: 8),
          _row(
            context,
            icon: CupertinoIcons.flag,
            label: 'Rally',
            hint: 'Rally 点同样是独立 mission type，不随任务上传',
            onRead: mission.readRallyFromVehicle,
            onUpload: () =>
                _confirmUpload(context, 'Rally', mission.uploadRally),
            onClear: mission.clearRally,
            labelColor: colors.accent,
          ),
        ],
      ),
    );
  }

  Future<void> _confirmUpload(
    BuildContext context,
    String label,
    Future<Object?> Function() upload,
  ) async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '下发$label到飞控？',
      message: '这会把本地$label写入飞控，覆盖飞控上的同类数据，下发后会自动回读核对。',
      confirmLabel: '下发$label',
    );
    if (!confirmed) return;
    await upload();
  }

  Widget _row(
    BuildContext context, {
    required IconData icon,
    required String label,
    required String hint,
    required Future<Object?> Function() onRead,
    required Future<Object?> Function() onUpload,
    required Future<void> Function() onClear,
    required Color labelColor,
  }) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    return Row(
      children: [
        Container(width: 3, height: 14, color: labelColor),
        const SizedBox(width: 8),
        MacosTooltip(
          message: hint,
          child: MacosIcon(icon, size: 13, color: colors.textSecondary),
        ),
        const SizedBox(width: 6),
        Text(label, style: typography.body),
        const Spacer(),
        PushButton(
          controlSize: ControlSize.regular,
          secondary: true,
          onPressed: enabled ? () => unawaited(onRead()) : null,
          child: const Text('读取'),
        ),
        const SizedBox(width: 6),
        MacosTooltip(
          message: '下发并自动回读核对',
          child: PushButton(
            controlSize: ControlSize.regular,
            onPressed: enabled ? () => unawaited(onUpload()) : null,
            child: const Text('下发'),
          ),
        ),
        const SizedBox(width: 6),
        PushButton(
          controlSize: ControlSize.regular,
          secondary: true,
          onPressed: enabled ? () => unawaited(onClear()) : null,
          child: const Text('清除'),
        ),
      ],
    );
  }
}

class _VerificationStatus extends StatelessWidget {
  const _VerificationStatus({required this.label, required this.verification});

  final String label;
  final MissionVerificationResult verification;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final matches = verification.matches;
    final text = matches
        ? '$label 回读核对通过'
        : '$label 回读发现 ${verification.differenceCount} 处差异';
    return Padding(
      padding: const EdgeInsets.only(top: 8),
      child: Row(
        children: [
          MacosIcon(
            matches
                ? CupertinoIcons.checkmark_circle
                : CupertinoIcons.exclamationmark_triangle,
            size: 14,
            color: matches ? colors.success : colors.warning,
          ),
          const SizedBox(width: 6),
          Expanded(
            child: Text(
              text,
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: matches ? colors.success : colors.warning,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
