import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_confirm.dart';
import '../widgets/mg_dialog.dart';
import 'fleet_controller.dart';
import 'fleet_models.dart';
import 'squad_controller.dart';
import 'squad_models.dart';

/// 地面站本地的“小队”页：离线也能查看/整理配置，连接后可整队执行。
///
/// 与多机面板分工：多机面板只做当前会话的勾选与批量执行，小队成员与身份
/// 归这一页管理。
class SquadSettingsPage extends StatefulWidget {
  const SquadSettingsPage({
    super.key,
    required this.scrollController,
    required this.squads,
    required this.fleet,
    required this.onRequestAction,
    required this.onReturnToMap,
    this.embedded = false,
  });

  final ScrollController scrollController;
  final SquadController squads;
  final FleetController fleet;
  final void Function(FleetBatchTarget target) onRequestAction;
  final VoidCallback onReturnToMap;
  final bool embedded;

  @override
  State<SquadSettingsPage> createState() => _SquadSettingsPageState();
}

class _SquadSettingsPageState extends State<SquadSettingsPage> {
  bool _busy = false;
  String? _error;

  SquadController get _squads => widget.squads;

  Future<void> _run(Future<void> Function() action) async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      await action();
    } on Object catch (error) {
      if (mounted) {
        setState(
          () => _error = error.toString().replaceFirst('Bad state: ', ''),
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _create() async {
    final name = await _promptName(
      context,
      title: '保存为小队',
      confirmLabel: '保存',
      initial: '小队 ${_squads.squads.length + 1}',
    );
    if (name == null) return;
    await _run(() => _squads.createFromSelection(name));
  }

  Future<void> _rename(Squad squad) async {
    final name = await _promptName(
      context,
      title: '重命名小队',
      confirmLabel: '保存',
      initial: squad.name,
    );
    if (name == null) return;
    await _run(() => _squads.rename(squad.id, name));
  }

  Future<void> _delete(Squad squad) async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '删除小队',
      message: '删除“${squad.name}”？不会影响飞机当前状态。',
      confirmLabel: '删除',
    );
    if (!confirmed) return;
    await _run(() => _squads.delete(squad.id));
  }

  Future<void> _addMembers(Squad squad) async {
    await _run(() => _squads.addSelectedMembers(squad.id));
  }

  Future<void> _removeMember(Squad squad, SquadMember member) async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '移除小队成员',
      message:
          '从“${squad.name}”移除“${member.label}”？'
          '只改变本地小队，不影响飞机当前状态。',
      confirmLabel: '移除',
    );
    if (!confirmed) return;
    await _run(() => _squads.removeMember(squad.id, member.identityKey));
  }

  void _selectOnline(SquadClaim claim) {
    widget.fleet.selectVehicles(claim.vehicleIds);
    widget.onReturnToMap();
  }

  /// 整队执行：先用已确认的在线成员替换批量选择，再走现有确认/下发流程。
  void _execute(SquadClaim claim, FleetBatchAction action) {
    final blockers = squadExecutionBlockers(claim, action);
    if (blockers.isNotEmpty) {
      setState(() => _error = blockers.join('；'));
      return;
    }
    try {
      widget.fleet.selectVehicles(claim.vehicleIds);
      final target = widget.fleet.captureBatchTarget(action);
      widget.onRequestAction(target);
    } on Object catch (error) {
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return AnimatedBuilder(
      animation: Listenable.merge([_squads, widget.fleet]),
      builder: (context, _) {
        final claims = [
          for (final squad in _squads.squads) _squads.claimFor(squad),
        ];
        final canCreate = widget.fleet.selectedVehicles.any(
          (vehicle) => vehicle.stateFresh && vehicle.identified,
        );
        final selectedReadyCount = widget.fleet.selectedVehicles
            .where((vehicle) => vehicle.stateFresh && vehicle.identified)
            .length;
        return ListView(
          controller: widget.scrollController,
          padding: EdgeInsets.all(widget.embedded ? 16 : 20),
          children: [
            if (!widget.embedded) ...[
              Text(
                '小队',
                style: MgTypography.sans(
                  MgTypography.title.copyWith(color: colors.textPrimary),
                ),
              ),
              const SizedBox(height: 4),
              Text(
                '把常用飞机保存成命名小队，按硬件身份认回成员；这里离线也能查看和整理。',
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textSecondary),
                ),
              ),
              const SizedBox(height: 18),
            ],
            _Card(
              title: '新建小队',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    canCreate
                        ? '将当前选中的 $selectedReadyCount 架飞机保存为新的持久小队。'
                        : '先在多机面板勾选已连接的飞机，再回到这里保存。',
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(
                        color: colors.textSecondary,
                      ),
                    ),
                  ),
                  const SizedBox(height: 10),
                  PushButton(
                    key: const ValueKey('squad-create'),
                    controlSize: ControlSize.regular,
                    secondary: !canCreate,
                    onPressed: _busy || !canCreate ? null : _create,
                    child: const Text('保存当前选择为小队…'),
                  ),
                ],
              ),
            ),
            if (_error != null) ...[
              const SizedBox(height: 10),
              Text(
                _error!,
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.danger),
                ),
              ),
            ],
            if (_squads.loadError != null) ...[
              const SizedBox(height: 10),
              Text(
                _squads.loadError!,
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.warning),
                ),
              ),
            ],
            const SizedBox(height: 18),
            if (_squads.loading)
              const Center(child: CupertinoActivityIndicator())
            else if (claims.isEmpty)
              Text(
                '还没有小队。',
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textSecondary),
                ),
              )
            else
              for (final claim in claims) ...[
                _SquadCard(
                  claim: claim,
                  readiness: evaluateSquadReadiness(claim),
                  busy: _busy,
                  canAddMembers: canCreate,
                  onAddMembers: () => _addMembers(claim.squad),
                  onRemoveMember: (member) =>
                      _removeMember(claim.squad, member),
                  onSelectOnline: claim.vehicleIds.isEmpty
                      ? null
                      : () => _selectOnline(claim),
                  onConfirmTemporary: claim.pendingCount == 0
                      ? null
                      : () => _squads.confirmTemporaryMembers(claim.squad),
                  onExecute: (action) => _execute(claim, action),
                  onRename: () => _rename(claim.squad),
                  onDelete: () => _delete(claim.squad),
                ),
                const SizedBox(height: 12),
              ],
          ],
        );
      },
    );
  }
}

class _Card extends StatelessWidget {
  const _Card({required this.title, required this.child});

  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Container(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title,
            style: MgTypography.sans(
              MgTypography.bodyStrong.copyWith(color: colors.textPrimary),
            ),
          ),
          const SizedBox(height: 8),
          child,
        ],
      ),
    );
  }
}

class _SquadCard extends StatelessWidget {
  const _SquadCard({
    required this.claim,
    required this.readiness,
    required this.busy,
    required this.canAddMembers,
    required this.onAddMembers,
    required this.onRemoveMember,
    this.onSelectOnline,
    this.onConfirmTemporary,
    required this.onExecute,
    required this.onRename,
    required this.onDelete,
  });

  final SquadClaim claim;
  final SquadReadinessReport readiness;
  final bool busy;
  final bool canAddMembers;
  final VoidCallback onAddMembers;
  final ValueChanged<SquadMember> onRemoveMember;
  final VoidCallback? onSelectOnline;
  final VoidCallback? onConfirmTemporary;
  final ValueChanged<FleetBatchAction> onExecute;
  final VoidCallback onRename;
  final VoidCallback onDelete;

  bool _canExecute(FleetBatchAction action) =>
      !busy && squadExecutionBlockers(claim, action).isEmpty;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final summaryColor = switch (readiness.overall) {
      SquadReadiness.ready => colors.success,
      SquadReadiness.warning => colors.warning,
      SquadReadiness.blocked => colors.danger,
    };
    return _Card(
      title: claim.squad.name,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            readiness.summary,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: summaryColor),
            ),
          ),
          const SizedBox(height: 6),
          for (final item in readiness.members)
            _MemberLine(
              squadId: claim.squad.id,
              readiness: item,
              onRemove: busy ? null : () => onRemoveMember(item.member.member),
            ),
          const SizedBox(height: 10),
          Wrap(
            spacing: 6,
            runSpacing: 6,
            children: [
              for (final action in const [
                FleetBatchAction.arm,
                FleetBatchAction.disarm,
                FleetBatchAction.returnToLaunch,
                FleetBatchAction.startMission,
              ])
                _SmallButton(
                  key: ValueKey('squad-exec-${action.name}-${claim.squad.id}'),
                  label: switch (action) {
                    FleetBatchAction.arm => '整队解锁',
                    FleetBatchAction.disarm => '整队上锁',
                    FleetBatchAction.returnToLaunch => '整队返航',
                    FleetBatchAction.startMission => '整队开始任务',
                  },
                  onPressed: _canExecute(action)
                      ? () => onExecute(action)
                      : null,
                ),
            ],
          ),
          const SizedBox(height: 6),
          Wrap(
            spacing: 6,
            runSpacing: 6,
            children: [
              if (claim.pendingCount > 0)
                _SmallButton(
                  key: ValueKey('squad-confirm-${claim.squad.id}'),
                  label: '确认临时成员',
                  onPressed: busy ? null : onConfirmTemporary,
                ),
              _SmallButton(
                key: ValueKey('squad-select-${claim.squad.id}'),
                label: '选中在线成员',
                onPressed: busy ? null : onSelectOnline,
              ),
              _SmallButton(
                key: ValueKey('squad-add-${claim.squad.id}'),
                label: '添加当前选择',
                onPressed: busy || !canAddMembers ? null : onAddMembers,
              ),
              _SmallButton(
                key: ValueKey('squad-rename-${claim.squad.id}'),
                label: '重命名',
                onPressed: busy ? null : onRename,
              ),
              _SmallButton(
                key: ValueKey('squad-delete-${claim.squad.id}'),
                label: '删除',
                onPressed: busy ? null : onDelete,
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _MemberLine extends StatelessWidget {
  const _MemberLine({
    required this.squadId,
    required this.readiness,
    this.onRemove,
  });

  final String squadId;
  final SquadMemberReadiness readiness;
  final VoidCallback? onRemove;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final status = readiness.member;
    final (stateText, stateColor) = switch (status.state) {
      SquadMemberState.offline => ('离线', colors.textTertiary),
      SquadMemberState.conflict => ('身份冲突', colors.warning),
      SquadMemberState.online => switch (readiness.status) {
        SquadReadiness.ready => ('在线 · 就绪', colors.success),
        SquadReadiness.warning => ('在线 · ${readiness.detail}', colors.warning),
        SquadReadiness.blocked => (
          '在线 · 不可用：${readiness.detail}',
          colors.danger,
        ),
      },
    };
    final suffix = status.temporary ? ' · 临时身份' : '';
    return Padding(
      padding: const EdgeInsets.only(top: 1),
      child: Row(
        children: [
          Expanded(
            child: Text(
              status.member.label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: colors.textSecondary),
              ),
            ),
          ),
          Flexible(
            child: Text(
              '$stateText$suffix',
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              textAlign: TextAlign.right,
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: stateColor),
              ),
            ),
          ),
          if (onRemove != null) ...[
            const SizedBox(width: 4),
            MacosIconButton(
              key: ValueKey(
                'squad-remove-$squadId-'
                '${readiness.member.member.identityKey}',
              ),
              icon: const MacosIcon(CupertinoIcons.minus_circle, size: 14),
              semanticLabel: '移除成员 ${readiness.member.member.label}',
              boxConstraints: const BoxConstraints.tightFor(
                width: 22,
                height: 22,
              ),
              padding: EdgeInsets.zero,
              backgroundColor: colors.transparent,
              hoverColor: colors.mapControlHover,
              onPressed: onRemove,
            ),
          ],
        ],
      ),
    );
  }
}

class _SmallButton extends StatelessWidget {
  const _SmallButton({super.key, required this.label, this.onPressed});

  final String label;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return PushButton(
      controlSize: ControlSize.small,
      secondary: true,
      onPressed: onPressed,
      child: Text(
        label,
        style: MgTypography.sans(
          MgTypography.caption.copyWith(
            color: onPressed == null ? colors.textTertiary : colors.textPrimary,
          ),
        ),
      ),
    );
  }
}

Future<String?> _promptName(
  BuildContext context, {
  required String title,
  required String confirmLabel,
  required String initial,
}) => showMgAlertDialog<String>(
  context: context,
  builder: (dialogContext) => _NamePromptDialog(
    title: title,
    confirmLabel: confirmLabel,
    initial: initial,
  ),
);

class _NamePromptDialog extends StatefulWidget {
  const _NamePromptDialog({
    required this.title,
    required this.confirmLabel,
    required this.initial,
  });

  final String title;
  final String confirmLabel;
  final String initial;

  @override
  State<_NamePromptDialog> createState() => _NamePromptDialogState();
}

class _NamePromptDialogState extends State<_NamePromptDialog> {
  late final TextEditingController _controller =
      TextEditingController(text: widget.initial)
        ..selection = TextSelection(
          baseOffset: 0,
          extentOffset: widget.initial.length,
        );

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    final name = _controller.text.trim();
    if (name.isEmpty) return;
    Navigator.of(context).pop(name);
  }

  @override
  Widget build(BuildContext context) => MacosAlertDialog(
    appIcon: MacosIcon(CupertinoIcons.group, size: 44),
    title: Text(widget.title),
    message: SizedBox(
      width: 320,
      child: MacosTextField(
        controller: _controller,
        placeholder: '小队名称',
        onSubmitted: (_) => _submit(),
      ),
    ),
    primaryButton: PushButton(
      controlSize: ControlSize.large,
      onPressed: _submit,
      child: Text(widget.confirmLabel),
    ),
    secondaryButton: PushButton(
      controlSize: ControlSize.large,
      secondary: true,
      onPressed: () => Navigator.of(context).pop(),
      child: const Text('取消'),
    ),
  );
}
