import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/adaptive_popup_button.dart';
import '../widgets/mac_panel.dart';
import 'mission_document_controller.dart';
import 'mission_document_models.dart';

/// 面板宽度：贴地图右边缘悬浮，折叠后只剩一个箭头按钮。
const double kMissionPanelWidth = 420;

const EdgeInsets _fieldPadding = EdgeInsets.symmetric(
  horizontal: 8,
  vertical: 3,
);

/// 航线规划面板。
///
/// 任务页 = 本地可编辑的任务文档（Home + 航点卡片 + 从飞控读取 / 上传到飞控）；
/// 设置页 = 整份任务的默认高度与速度。数据流照 Mission Planner 的 Read/Write 与
/// QGC 的 Download/Upload：**不自动拉取**，覆盖本地改动前先确认。
class MissionPlanningFloatingPanel extends StatefulWidget {
  const MissionPlanningFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<MissionPlanningFloatingPanel> createState() =>
      _MissionPlanningFloatingPanelState();
}

class _MissionPlanningFloatingPanelState
    extends State<MissionPlanningFloatingPanel> {
  static const _tabLabels = ['任务', '设置'];

  final MacosTabController _tabs = MacosTabController(
    initialIndex: 0,
    length: _tabLabels.length,
  );

  bool _collapsed = false;
  String? _lastSelectedEntityId;
  final Map<String, GlobalKey> _entityKeys = {};

  MissionDocumentController get _mission => widget.controller.missionPlanning;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
    _mission.addListener(_handleMissionChanged);
    unawaited(_mission.initialize());
  }

  void _handleTabChanged() {
    if (mounted) setState(() {});
  }

  void _handleMissionChanged() {
    final selectedIndex = _mission.selectedEntityIndex;
    final entities = _mission.document?.entities ?? const <MissionEntity>[];
    if (selectedIndex == null ||
        selectedIndex < 0 ||
        selectedIndex >= entities.length) {
      _lastSelectedEntityId = null;
      return;
    }
    final selectedId = entities[selectedIndex].id;
    if (selectedId == _lastSelectedEntityId) return;
    _lastSelectedEntityId = selectedId;
    final entityKey = _entityKeys.putIfAbsent(selectedId, GlobalKey.new);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final target = entityKey.currentContext;
      if (target != null) {
        Scrollable.ensureVisible(
          target,
          duration: const Duration(milliseconds: 180),
          alignment: 0.1,
        );
      }
    });
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    _mission.removeListener(_handleMissionChanged);
    _tabs.dispose();
    super.dispose();
  }

  void _toggleCollapsed() => setState(() => _collapsed = !_collapsed);

  @override
  Widget build(BuildContext context) {
    if (_collapsed) {
      // 折叠态：整块面板收起成一个箭头按钮，仍然贴在同一个边缘。
      return Align(
        alignment: Alignment.topRight,
        child: Tooltip(
          message: '展开航线规划',
          child: MacosIconButton(
            icon: const MacosIcon(CupertinoIcons.chevron_left, size: 14),
            semanticLabel: '展开航线规划',
            onPressed: _toggleCollapsed,
          ),
        ),
      );
    }

    final colors = context.mgColors;
    return SizedBox(
      width: kMissionPanelWidth,
      child: Container(
        decoration: BoxDecoration(
          color: colors.surfaceMuted,
          borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
          border: Border.all(color: colors.divider),
          boxShadow: [
            BoxShadow(
              color: colors.shadow,
              blurRadius: 24,
              offset: const Offset(0, 8),
            ),
          ],
        ),
        clipBehavior: Clip.antiAlias,
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(18, 12, 12, 12),
              child: Row(
                children: [
                  Text('航线规划', style: MacosTheme.of(context).typography.title3),
                  const Spacer(),
                  Tooltip(
                    message: '折叠航线规划',
                    child: MacosIconButton(
                      icon: const MacosIcon(
                        CupertinoIcons.chevron_right,
                        size: 14,
                      ),
                      semanticLabel: '折叠航线规划',
                      onPressed: _toggleCollapsed,
                    ),
                  ),
                  Tooltip(
                    message: '关闭航线规划',
                    child: MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 14),
                      semanticLabel: '关闭航线规划',
                      onPressed: widget.onClose,
                    ),
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: colors.divider),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(14, 8, 14, 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Center(
                      child: MacosSegmentedControl(
                        controller: _tabs,
                        tabs: [
                          for (final label in _tabLabels)
                            MacosTab(label: label),
                        ],
                      ),
                    ),
                    const SizedBox(height: kMacTabStripGap),
                    Expanded(
                      child: _tabs.index == 0
                          ? _buildTasksTab(context)
                          : _buildSettingsTab(context),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  // ================================================================
  // 任务页
  // ================================================================

  Widget _buildTasksTab(BuildContext context) => ListenableBuilder(
    listenable: _mission,
    builder: (context, _) {
      final document = _mission.document;
      final entities = document?.entities ?? const <MissionEntity>[];
      final enabled = !_mission.busy && !_mission.transferActive;
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _buildMissionActions(context, document),
          const SizedBox(height: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _HomeCard(
                  controller: widget.controller,
                  home: document?.home,
                  enabled: enabled,
                  onChanged: _updateHome,
                ),
                const SizedBox(height: 8),
                Expanded(
                  child: entities.isEmpty
                      ? Center(child: _buildEmptyHint(context))
                      : ReorderableListView.builder(
                          buildDefaultDragHandles: false,
                          itemCount: entities.length,
                          onReorderItem: (oldIndex, newIndex) => unawaited(
                            _mutate(
                              () => _mission.reorderEntity(oldIndex, newIndex),
                            ),
                          ),
                          itemBuilder: (context, index) {
                            final entity = entities[index];
                            return Padding(
                              key: _entityKeys.putIfAbsent(
                                entity.id,
                                GlobalKey.new,
                              ),
                              padding: const EdgeInsets.only(bottom: 8),
                              child: GestureDetector(
                                behavior: HitTestBehavior.opaque,
                                onTap: () => _mission.selectEntity(index),
                                child: _MissionEntityCard(
                                  position: index + 1,
                                  reorderIndex: index,
                                  selected:
                                      _mission.selectedEntityIndex == index,
                                  enabled: enabled,
                                  entity: entity,
                                  catalog: _mission.commandCatalog,
                                  onCommandChanged: (command) => _mutate(
                                    () => _mission.updateEntity(
                                      entity.copyWith(
                                        command: command,
                                        type: _typeForCommand(command),
                                      ),
                                    ),
                                  ),
                                  onDelete: () => _mutate(
                                    () => _mission.removeEntity(entity.id),
                                  ),
                                  onAltitudeChanged: (altitude) => _mutate(
                                    () => _mission.updateEntity(
                                      entity.copyWith(altitudeMeters: altitude),
                                    ),
                                  ),
                                  onEdit: () => showMissionEntityDialog(
                                    context,
                                    controller: widget.controller,
                                    catalog: _mission.commandCatalog,
                                    entity: entity,
                                    onSave: (updated) => _mutate(
                                      () => _mission.updateEntity(updated),
                                    ),
                                  ),
                                ),
                              ),
                            );
                          },
                        ),
                ),
              ],
            ),
          ),
        ],
      );
    },
  );

  Widget _buildMissionActions(BuildContext context, MissionDocument? document) {
    final colors = context.mgColors;
    final busy = _mission.busy;
    final transfer = _mission.transfer;
    final transferring = transfer?.active ?? false;
    final status = _mission.status ?? _mission.error;
    final diagnostics = (_mission.validation?.diagnostics ?? const [])
        .where(
          (diagnostic) => diagnostic.severity != MissionDiagnosticSeverity.info,
        )
        .take(3)
        .toList(growable: false);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed: busy || transferring ? null : _downloadFromVehicle,
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  MacosIcon(CupertinoIcons.arrow_down_to_line, size: 13),
                  SizedBox(width: 6),
                  Text('从飞控读取'),
                ],
              ),
            ),
            const SizedBox(width: 8),
            PushButton(
              controlSize: ControlSize.regular,
              secondary: true,
              onPressed: busy || transferring || document == null
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
            const Spacer(),
            if (_mission.dirty)
              Padding(
                padding: const EdgeInsets.only(right: 8),
                child: Text(
                  '未上传',
                  style: MacosTheme.of(
                    context,
                  ).typography.caption1.copyWith(color: colors.warning),
                ),
              ),
            if (transferring)
              PushButton(
                controlSize: ControlSize.small,
                secondary: true,
                onPressed: _mission.cancelTransfer,
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
        if (status != null) ...[
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
        if (diagnostics.isNotEmpty) ...[
          const SizedBox(height: 6),
          for (final diagnostic in diagnostics)
            _MissionDiagnosticRow(
              diagnostic: diagnostic,
              onTap: diagnostic.entityId.isEmpty
                  ? null
                  : () => _mission.selectEntityById(diagnostic.entityId),
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
    return switch (transfer.state) {
      MissionTransferState.downloading =>
        '正在读取 ${transfer.received}/${transfer.total}',
      MissionTransferState.uploading =>
        '正在上传 ${transfer.sent}/${transfer.total}',
      MissionTransferState.waitingAck => '正在等待飞控确认',
      MissionTransferState.idle => '传输完成 ${transfer.progressPercent}%',
    };
  }

  String _estimateSummary() {
    final estimate = _mission.estimate;
    final count = _mission.document?.entities.length ?? 0;
    if (estimate == null) return '$count 个任务项';
    final distanceKm = estimate.totalDistanceMeters / 1000.0;
    final minutes = estimate.estimatedTimeSeconds / 60.0;
    return '$count 个任务项 · ${distanceKm.toStringAsFixed(2)} km · '
        '${minutes.toStringAsFixed(1)} min';
  }

  Widget _buildEmptyHint(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 14),
    child: Text(
      '点击地图添加航点',
      textAlign: TextAlign.center,
      style: MacosTheme.of(
        context,
      ).typography.caption1.copyWith(color: context.mgColors.textSecondary),
    ),
  );

  String _typeForCommand(int command) {
    for (final descriptor in _mission.commandCatalog) {
      if (descriptor.command == command) return descriptor.entityType;
    }
    return 'waypoint';
  }

  Future<void> _mutate(Future<void> Function() operation) async {
    try {
      await operation();
    } on Object {
      // 失败信息由 controller 的 status / error 呈现。
    }
  }

  Future<void> _updateHome(MissionHome home) async {
    final document = _mission.document;
    if (document == null) return;
    try {
      await _mission.replaceDocument(document.copyWith(home: home));
    } on Object {
      // 同上，错误走 status。
    }
  }

  /// 从飞控读取：本地已有任务项时先确认（照抄 MP 的
  /// “This will clear your existing points, Continue?” 与 QGC 的下载前提示）。
  Future<void> _downloadFromVehicle() async {
    final localCount = _mission.document?.entities.length ?? 0;
    if (_mission.dirty) {
      final confirmed = await confirmMissionAction(
        context,
        title: '从飞控读取任务',
        message: '本地有未上传的修改（$localCount 个任务项），读取会覆盖它们。是否继续？',
        confirmLabel: '继续读取',
      );
      if (!confirmed) return;
    }
    await _mutate(_mission.downloadFromVehicle);
  }

  Future<void> _uploadToVehicle() async {
    await _mutate(() async {
      await _mission.upload();
    });
  }

  // ================================================================
  // 设置页
  // ================================================================

  Widget _buildSettingsTab(BuildContext context) => ListenableBuilder(
    listenable: _mission,
    builder: (context, _) {
      final document = _mission.document;
      if (document == null) {
        return Text(
          '任务文档尚未初始化',
          style: MacosTheme.of(
            context,
          ).typography.callout.copyWith(color: context.mgColors.textSecondary),
        );
      }
      return _MissionDefaultsForm(
        defaults: document.defaults,
        enabled: !_mission.busy && !_mission.transferActive,
        onSubmit: (defaults) async {
          try {
            await _mission.replaceDocument(
              document.copyWith(defaults: defaults),
            );
          } on Object {
            // 错误走 status。
          }
        },
      );
    },
  );
}

/// Home 卡片：编辑文档 home，并可把它设为飞控 home（影响 RTL 基准）。
class _HomeCard extends StatefulWidget {
  const _HomeCard({
    required this.controller,
    required this.home,
    required this.onChanged,
    this.enabled = true,
  });

  final ConnectionController controller;
  final MissionHome? home;
  final bool enabled;
  final Future<void> Function(MissionHome home) onChanged;

  @override
  State<_HomeCard> createState() => _HomeCardState();
}

class _HomeCardState extends State<_HomeCard> {
  late final TextEditingController _latitude = TextEditingController(
    text: _format(widget.home?.latitude),
  );
  late final TextEditingController _longitude = TextEditingController(
    text: _format(widget.home?.longitude),
  );
  late final TextEditingController _altitude = TextEditingController(
    text: (widget.home?.altitudeMeters ?? 0).toStringAsFixed(1),
  );

  static String _format(double? value) =>
      value == null ? '' : value.toStringAsFixed(7);

  @override
  void dispose() {
    _latitude.dispose();
    _longitude.dispose();
    _altitude.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    final latitude = double.tryParse(_latitude.text.trim());
    final longitude = double.tryParse(_longitude.text.trim());
    final altitude = double.tryParse(_altitude.text.trim());
    if (latitude == null || longitude == null || altitude == null) return;
    await widget.onChanged(
      MissionHome(
        set: true,
        latitude: latitude,
        longitude: longitude,
        altitudeMeters: altitude,
      ),
    );
  }

  /// 设为飞控 home：QGC 会先弹确认，因为这会改返航基准。
  Future<void> _setVehicleHome() async {
    final latitude = double.tryParse(_latitude.text.trim());
    final longitude = double.tryParse(_longitude.text.trim());
    final altitude = double.tryParse(_altitude.text.trim());
    if (latitude == null || longitude == null || altitude == null) return;

    final confirmed = await confirmMissionAction(
      context,
      title: '设置 Home',
      message: '把 Home 设为该坐标会改变飞控的返航基准（RTL 落点）。是否继续？',
      confirmLabel: '设为 Home',
    );
    if (!confirmed) return;
    try {
      await widget.controller.setVehicleHome(latitude, longitude, altitude);
    } on Object {
      // 失败信息由后端返回，界面保持原值。
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MissionCardFrame(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MissionBadge(label: 'H', color: colors.warning),
              const SizedBox(width: 10),
              Text('设置 Home', style: MacosTheme.of(context).typography.body),
              const Spacer(),
              PushButton(
                controlSize: ControlSize.small,
                secondary: true,
                onPressed: widget.controller.isConnected && widget.enabled
                    ? _setVehicleHome
                    : null,
                child: const Text('设为飞控 Home'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: _LabeledField(
                  label: '纬度',
                  child: MacosTextField(
                    controller: _latitude,
                    enabled: widget.enabled,
                    padding: _fieldPadding,
                    onSubmitted: (_) => _save(),
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: _LabeledField(
                  label: '经度',
                  child: MacosTextField(
                    controller: _longitude,
                    enabled: widget.enabled,
                    padding: _fieldPadding,
                    onSubmitted: (_) => _save(),
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: _LabeledField(
                  label: 'H(m)',
                  child: MacosTextField(
                    controller: _altitude,
                    enabled: widget.enabled,
                    padding: _fieldPadding,
                    onSubmitted: (_) => _save(),
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Align(
            alignment: Alignment.centerRight,
            child: PushButton(
              controlSize: ControlSize.small,
              secondary: true,
              onPressed: widget.enabled ? _save : null,
              child: const Text('保存'),
            ),
          ),
        ],
      ),
    );
  }
}

/// 任务项卡片：编号徽标 + 命令下拉 + 编辑 / 排序 / 删除 + 高度与位置。
class _MissionEntityCard extends StatelessWidget {
  const _MissionEntityCard({
    required this.position,
    required this.reorderIndex,
    required this.selected,
    required this.entity,
    required this.catalog,
    required this.onCommandChanged,
    required this.onAltitudeChanged,
    required this.onEdit,
    this.enabled = true,
    this.onDelete,
  });

  final int position;
  final int reorderIndex;
  final bool selected;
  final bool enabled;
  final MissionEntity entity;
  final List<MissionCommandDescriptor> catalog;
  final ValueChanged<int> onCommandChanged;
  final ValueChanged<double> onAltitudeChanged;
  final VoidCallback onEdit;
  final VoidCallback? onDelete;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final commands = catalog.map((descriptor) => descriptor.command).toList();
    final selectedCommand = commands.contains(entity.command)
        ? entity.command
        : (commands.isEmpty ? null : commands.first);
    return MissionCardFrame(
      selected: selected,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MissionBadge(label: '$position', color: colors.accent),
              const SizedBox(width: 10),
              Expanded(
                child: AdaptivePopupButton<int>(
                  value: selectedCommand,
                  options: commands,
                  labelOf: _labelFor,
                  onChanged: enabled ? onCommandChanged : null,
                ),
              ),
              Tooltip(
                message: '拖动排序',
                child: ReorderableDragStartListener(
                  index: reorderIndex,
                  enabled: enabled,
                  child: const MacosIcon(CupertinoIcons.bars, size: 13),
                ),
              ),
              Tooltip(
                message: '编辑航点',
                child: MacosIconButton(
                  icon: const MacosIcon(CupertinoIcons.pencil, size: 13),
                  semanticLabel: '编辑航点',
                  onPressed: enabled ? onEdit : null,
                ),
              ),
              Tooltip(
                message: '删除',
                child: MacosIconButton(
                  icon: MacosIcon(
                    CupertinoIcons.trash,
                    size: 13,
                    color: !enabled || onDelete == null
                        ? colors.textTertiary
                        : colors.danger,
                  ),
                  semanticLabel: '删除',
                  onPressed: enabled ? onDelete : null,
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                flex: 2,
                child: _LabeledField(
                  label: 'H(m)',
                  child: _NumberField(
                    key: ValueKey('entity-alt-${entity.id}'),
                    initial: entity.altitudeMeters,
                    enabled: enabled,
                    onSubmitted: onAltitudeChanged,
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                flex: 3,
                child: _LabeledField(
                  label: '位置',
                  child: MacosTextField(
                    enabled: false,
                    placeholder: _positionLabel(),
                    padding: _fieldPadding,
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  String _labelFor(int command) {
    for (final descriptor in catalog) {
      if (descriptor.command == command) {
        return '${descriptor.label} (${descriptor.name})';
      }
    }
    return '命令 $command';
  }

  String _positionLabel() {
    final latitude = entity.latitude;
    final longitude = entity.longitude;
    if (latitude == null || longitude == null) return '无位置';
    return '${latitude.toStringAsFixed(6)}, ${longitude.toStringAsFixed(6)}';
  }
}

/// 任务默认参数（整份任务共用）。
class _MissionDefaultsForm extends StatefulWidget {
  const _MissionDefaultsForm({
    required this.defaults,
    required this.onSubmit,
    this.enabled = true,
  });

  final MissionDefaults defaults;
  final Future<void> Function(MissionDefaults defaults) onSubmit;
  final bool enabled;

  @override
  State<_MissionDefaultsForm> createState() => _MissionDefaultsFormState();
}

class _MissionDefaultsFormState extends State<_MissionDefaultsForm> {
  late final TextEditingController _altitude = TextEditingController(
    text: widget.defaults.altitudeMeters.toStringAsFixed(1),
  );
  late final TextEditingController _speed = TextEditingController(
    text: widget.defaults.speedMetersPerSecond.toStringAsFixed(1),
  );

  @override
  void dispose() {
    _altitude.dispose();
    _speed.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final altitude = double.tryParse(_altitude.text.trim());
    final speed = double.tryParse(_speed.text.trim());
    if (altitude == null || speed == null) return;
    await widget.onSubmit(
      MissionDefaults(
        frame: widget.defaults.frame,
        altitudeMeters: altitude,
        speedMetersPerSecond: speed,
      ),
    );
  }

  @override
  Widget build(BuildContext context) => SingleChildScrollView(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _LabeledField(
          label: '默认高度 (m)',
          child: MacosTextField(
            controller: _altitude,
            enabled: widget.enabled,
            padding: _fieldPadding,
            onSubmitted: (_) => _submit(),
          ),
        ),
        const SizedBox(height: 10),
        _LabeledField(
          label: '默认速度 (m/s)',
          child: MacosTextField(
            controller: _speed,
            enabled: widget.enabled,
            padding: _fieldPadding,
            onSubmitted: (_) => _submit(),
          ),
        ),
        const SizedBox(height: 12),
        Align(
          alignment: Alignment.centerRight,
          child: PushButton(
            controlSize: ControlSize.regular,
            onPressed: widget.enabled ? _submit : null,
            child: const Text('应用'),
          ),
        ),
      ],
    ),
  );
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
                diagnostic.message,
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
}

/// 面板内的卡片外观：只包裹「重复的任务项」，避免卡片套卡片。
class MissionCardFrame extends StatelessWidget {
  const MissionCardFrame({
    super.key,
    required this.child,
    this.selected = false,
  });

  final Widget child;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 10, 12, 10),
      decoration: BoxDecoration(
        color: selected ? colors.surfaceMuted : colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(
          color: selected ? colors.accent : colors.divider,
          width: selected ? 1.5 : 1,
        ),
      ),
      child: child,
    );
  }
}

class MissionBadge extends StatelessWidget {
  const MissionBadge({super.key, required this.label, required this.color});

  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
    width: 22,
    height: 22,
    alignment: Alignment.center,
    decoration: BoxDecoration(color: color, shape: BoxShape.circle),
    child: Text(
      label,
      style: MacosTheme.of(
        context,
      ).typography.caption1.copyWith(color: context.mgColors.onAccent),
    ),
  );
}

class _LabeledField extends StatelessWidget {
  const _LabeledField({required this.label, required this.child});

  final String label;
  final Widget child;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      Text(
        label,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(color: context.mgColors.textSecondary),
      ),
      const SizedBox(height: 4),
      child,
    ],
  );
}

/// 只接受数字的紧凑输入框；提交时回调有效数值。
class _NumberField extends StatefulWidget {
  const _NumberField({
    super.key,
    required this.initial,
    required this.onSubmitted,
    this.enabled = true,
  });

  final double? initial;
  final ValueChanged<double> onSubmitted;
  final bool enabled;

  @override
  State<_NumberField> createState() => _NumberFieldState();
}

class _NumberFieldState extends State<_NumberField> {
  late final TextEditingController _controller = TextEditingController(
    text: widget.initial?.toStringAsFixed(1) ?? '',
  );

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    final value = double.tryParse(_controller.text.trim());
    if (value == null) return;
    widget.onSubmitted(value);
  }

  @override
  Widget build(BuildContext context) => MacosTextField(
    controller: _controller,
    enabled: widget.enabled,
    padding: _fieldPadding,
    onSubmitted: (_) => _submit(),
  );
}

/// 统一的确认弹窗，返回用户是否点了确认。
Future<bool> confirmMissionAction(
  BuildContext context, {
  required String title,
  required String message,
  required String confirmLabel,
}) async {
  final result = await showMacosAlertDialog<bool>(
    context: context,
    builder: (dialogContext) => MacosAlertDialog(
      appIcon: MacosIcon(
        CupertinoIcons.exclamationmark_triangle,
        size: 44,
        color: dialogContext.mgColors.warning,
      ),
      title: Text(title),
      message: Text(message),
      primaryButton: PushButton(
        controlSize: ControlSize.large,
        onPressed: () => Navigator.of(dialogContext).pop(true),
        child: Text(confirmLabel),
      ),
      secondaryButton: PushButton(
        controlSize: ControlSize.large,
        secondary: true,
        onPressed: () => Navigator.of(dialogContext).pop(false),
        child: const Text('取消'),
      ),
    ),
  );
  return result ?? false;
}

/// 编辑 / 新增任务项：命令类型、位置、高度，以及命令目录定义的参数。
Future<void> showMissionEntityDialog(
  BuildContext context, {
  required ConnectionController controller,
  required List<MissionCommandDescriptor> catalog,
  required Future<void> Function(MissionEntity entity) onSave,
  MissionEntity? entity,
  LatLng? initialPosition,
}) async {
  if (catalog.isEmpty) return;
  final mission = controller.missionPlanning;
  await mission.initialize();
  if (!context.mounted) return;
  await _showFadeDialog<void>(
    context: context,
    builder: (dialogContext) => _MissionEntitySheet(
      catalog: mission.commandCatalog.isEmpty
          ? catalog
          : mission.commandCatalog,
      defaults: mission.document?.defaults ?? const MissionDefaults(),
      entity: entity,
      initialPosition: initialPosition,
      onSave: onSave,
    ),
  );
}

/// 无弹跳的淡入对话框。
///
/// macos_ui 的 `showMacosSheet` 入场使用弹簧缩放曲线（`_SubtleBounceCurve`），
/// 这里改为纯淡入，保留 `MacosSheet` 的外观但去掉弹跳。
Future<T?> _showFadeDialog<T>({
  required BuildContext context,
  required WidgetBuilder builder,
}) {
  return showGeneralDialog<T>(
    context: context,
    barrierDismissible: false,
    barrierLabel: MaterialLocalizations.of(context).modalBarrierDismissLabel,
    barrierColor: MgColors.of(context).shadow,
    transitionDuration: const Duration(milliseconds: 150),
    pageBuilder: (dialogContext, animation, secondaryAnimation) =>
        builder(dialogContext),
    transitionBuilder: (dialogContext, animation, secondaryAnimation, child) =>
        FadeTransition(
          opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
          child: child,
        ),
  );
}

class _MissionEntitySheet extends StatefulWidget {
  const _MissionEntitySheet({
    required this.catalog,
    required this.defaults,
    required this.onSave,
    this.entity,
    this.initialPosition,
  });

  final List<MissionCommandDescriptor> catalog;
  final MissionDefaults defaults;
  final Future<void> Function(MissionEntity entity) onSave;

  /// 为空表示新增。
  final MissionEntity? entity;
  final LatLng? initialPosition;

  @override
  State<_MissionEntitySheet> createState() => _MissionEntitySheetState();
}

class _MissionEntitySheetState extends State<_MissionEntitySheet> {
  late MissionCommandDescriptor _descriptor = _descriptorFor(
    widget.entity?.command ?? widget.catalog.first.command,
  );
  late final TextEditingController _latitude = TextEditingController(
    text: _initialLatitude(),
  );
  late final TextEditingController _longitude = TextEditingController(
    text: _initialLongitude(),
  );
  late final TextEditingController _altitude = TextEditingController(
    text: _initialAltitude(),
  );
  final Map<int, TextEditingController> _parameterFields = {};
  String? _error;

  MissionCommandDescriptor _descriptorFor(int command) =>
      widget.catalog.firstWhere(
        (descriptor) => descriptor.command == command,
        orElse: () => widget.catalog.first,
      );

  String _initialLatitude() {
    final entity = widget.entity;
    if (entity?.latitude != null) return entity!.latitude!.toStringAsFixed(7);
    final position = widget.initialPosition;
    return position == null ? '' : position.latitude.toStringAsFixed(7);
  }

  String _initialLongitude() {
    final entity = widget.entity;
    if (entity?.longitude != null) return entity!.longitude!.toStringAsFixed(7);
    final position = widget.initialPosition;
    return position == null ? '' : position.longitude.toStringAsFixed(7);
  }

  String _initialAltitude() {
    final altitude = widget.entity?.altitudeMeters;
    return (altitude ?? widget.defaults.altitudeMeters).toStringAsFixed(1);
  }

  TextEditingController _parameterController(
    MissionCommandParameter parameter,
  ) {
    return _parameterFields.putIfAbsent(parameter.index, () {
      final entity = widget.entity;
      final existing = entity != null && entity.params.length >= parameter.index
          ? entity.params[parameter.index - 1]
          : parameter.defaultValue;
      return TextEditingController(text: formatMissionParameter(existing));
    });
  }

  @override
  void dispose() {
    _latitude.dispose();
    _longitude.dispose();
    _altitude.dispose();
    for (final controller in _parameterFields.values) {
      controller.dispose();
    }
    super.dispose();
  }

  Future<void> _save() async {
    double? latitude;
    double? longitude;
    if (_descriptor.requiresPosition) {
      latitude = double.tryParse(_latitude.text.trim());
      longitude = double.tryParse(_longitude.text.trim());
      if (latitude == null || longitude == null) {
        setState(() => _error = '纬度和经度必须是数字');
        return;
      }
    }

    final altitudeText = _altitude.text.trim();
    final altitude = altitudeText.isEmpty
        ? null
        : double.tryParse(altitudeText);
    if (altitudeText.isNotEmpty && altitude == null) {
      setState(() => _error = '高度必须是数字');
      return;
    }

    final params = <double>[0, 0, 0, 0];
    for (final parameter in _descriptor.parameters) {
      final text = _parameterController(parameter).text.trim();
      final value = text.isEmpty
          ? parameter.defaultValue
          : double.tryParse(text);
      if (value == null) {
        setState(() => _error = '${parameter.label} 必须是数字');
        return;
      }
      if (parameter.index >= 1 && parameter.index <= 4) {
        params[parameter.index - 1] = value;
      }
    }

    final base = widget.entity ?? const MissionEntity();
    final entity = base.copyWith(
      type: _descriptor.entityType,
      command: _descriptor.command,
      autocontinue: 1,
      latitude: latitude,
      longitude: longitude,
      altitudeMeters: altitude,
      params: params,
    );

    await widget.onSave(entity);
    if (mounted) Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final commands = widget.catalog
        .map((descriptor) => descriptor.command)
        .toList();
    return Center(
      child: MacosSheet(
        insetPadding: EdgeInsets.zero,
        child: SizedBox(
          width: 360,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 10, 14, 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        widget.entity == null ? '添加航点' : '编辑航点',
                        style: MacosTheme.of(context).typography.title3,
                      ),
                    ),
                    MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 13),
                      semanticLabel: '关闭',
                      onPressed: () => Navigator.of(context).pop(),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                _LabeledField(
                  label: '命令类型',
                  child: AdaptivePopupButton<int>(
                    value: _descriptor.command,
                    options: commands,
                    labelOf: (command) {
                      final descriptor = _descriptorFor(command);
                      return '${descriptor.label} (${descriptor.name})';
                    },
                    onChanged: (command) =>
                        setState(() => _descriptor = _descriptorFor(command)),
                  ),
                ),
                if (_descriptor.requiresPosition ||
                    _descriptor.requiresAltitude) ...[
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      if (_descriptor.requiresPosition) ...[
                        Expanded(
                          child: _LabeledField(
                            label: '纬度',
                            child: MacosTextField(
                              controller: _latitude,
                              padding: _fieldPadding,
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: _LabeledField(
                            label: '经度',
                            child: MacosTextField(
                              controller: _longitude,
                              padding: _fieldPadding,
                            ),
                          ),
                        ),
                      ],
                      if (_descriptor.requiresPosition &&
                          _descriptor.requiresAltitude)
                        const SizedBox(width: 8),
                      if (_descriptor.requiresAltitude)
                        Expanded(
                          child: _LabeledField(
                            label: '高度 (m)',
                            child: MacosTextField(
                              controller: _altitude,
                              padding: _fieldPadding,
                            ),
                          ),
                        ),
                    ],
                  ),
                ],
                if (_descriptor.parameters.isNotEmpty) ...[
                  const SizedBox(height: 10),
                  Text(
                    '参数',
                    style: MacosTheme.of(
                      context,
                    ).typography.caption1.copyWith(color: colors.textSecondary),
                  ),
                  const SizedBox(height: 4),
                  for (
                    var index = 0;
                    index < _descriptor.parameters.length;
                    index += 2
                  )
                    Padding(
                      padding: const EdgeInsets.only(bottom: 7),
                      child: Row(
                        children: [
                          Expanded(
                            child: _buildParameterField(
                              _descriptor.parameters[index],
                            ),
                          ),
                          const SizedBox(width: 8),
                          if (index + 1 < _descriptor.parameters.length)
                            Expanded(
                              child: _buildParameterField(
                                _descriptor.parameters[index + 1],
                              ),
                            )
                          else
                            const Spacer(),
                        ],
                      ),
                    ),
                ],
                if (_error != null) ...[
                  const SizedBox(height: 4),
                  Text(
                    _error!,
                    style: MacosTheme.of(
                      context,
                    ).typography.caption1.copyWith(color: colors.danger),
                  ),
                ],
                const SizedBox(height: 8),
                Align(
                  alignment: Alignment.centerRight,
                  child: PushButton(
                    controlSize: ControlSize.regular,
                    onPressed: _save,
                    child: const Text('保存'),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildParameterField(MissionCommandParameter parameter) {
    return _LabeledField(
      label: parameter.unit.isEmpty
          ? parameter.label
          : '${parameter.label} (${parameter.unit})',
      child:
          parameter.type == MissionParameterType.enumeration &&
              parameter.options.isNotEmpty
          ? AdaptivePopupButton<double>(
              value: _parameterValue(parameter),
              options: [for (final option in parameter.options) option.value],
              labelOf: (value) => _optionLabel(parameter, value),
              onChanged: (value) => setState(
                () => _parameterController(parameter).text =
                    formatMissionParameter(value),
              ),
            )
          : MacosTextField(
              controller: _parameterController(parameter),
              padding: _fieldPadding,
            ),
    );
  }

  double _parameterValue(MissionCommandParameter parameter) {
    final text = _parameterController(parameter).text.trim();
    return double.tryParse(text) ?? parameter.defaultValue;
  }

  String _optionLabel(MissionCommandParameter parameter, double value) {
    for (final option in parameter.options) {
      if (option.value == value) return option.label;
    }
    return value.toStringAsFixed(0);
  }
}

/// 引导操作：门控条件照抄 QGC `GuidedActionsController.qml` 的 `show*` 属性。
Future<void> showGuidedActionsDialog(
  BuildContext context, {
  required ConnectionController controller,
  required LatLng position,
}) async {
  await _showFadeDialog<void>(
    context: context,
    builder: (dialogContext) =>
        _GuidedActionsSheet(controller: controller, position: position),
  );
}

class _GuidedAction {
  const _GuidedAction({
    required this.label,
    required this.enabled,
    required this.confirmMessage,
    required this.run,
    this.needsAltitude = false,
  });

  final String label;
  final bool enabled;
  final String confirmMessage;
  final bool needsAltitude;
  final Future<void> Function({
    required double latitude,
    required double longitude,
    required double altitude,
  })
  run;
}

class _GuidedActionsSheet extends StatefulWidget {
  const _GuidedActionsSheet({required this.controller, required this.position});

  final ConnectionController controller;
  final LatLng position;

  @override
  State<_GuidedActionsSheet> createState() => _GuidedActionsSheetState();
}

class _GuidedActionsSheetState extends State<_GuidedActionsSheet> {
  late final TextEditingController _altitude = TextEditingController(
    text: _defaultAltitude().toStringAsFixed(1),
  );
  String? _selectedActionLabel;
  String? _error;

  double _defaultAltitude() {
    final altitude = widget.controller.vehiclePosition?.altitudeMeters;
    if (altitude != null && altitude > 0) return altitude;
    return 30;
  }

  @override
  void dispose() {
    _altitude.dispose();
    super.dispose();
  }

  List<_GuidedAction> _actions() {
    final controller = widget.controller;
    final connected = controller.isConnected;
    final armed = controller.vehicleArmed;
    final flying = controller.vehicleFlying;
    // MAV_TYPE_FIXED_WING = 1：QGC 不给固定翼显示“降落”。
    final fixedWing = controller.vehicleType == 1;
    final mode = controller.vehicleFlightMode.toUpperCase();

    return [
      _GuidedAction(
        label: '移动到此处',
        enabled: connected && flying,
        needsAltitude: true,
        confirmMessage: '飞机会飞往该坐标并保持高度。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedGoto(latitude, longitude, altitude),
      ),
      _GuidedAction(
        label: '返航 (RTL)',
        enabled: connected && armed && flying && !mode.contains('RTL'),
        confirmMessage: '飞机将返回起飞点（Home）并自动降落。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedReturnToLaunch(),
      ),
      _GuidedAction(
        label: '降落',
        enabled: connected && armed && !fixedWing && !mode.contains('LAND'),
        confirmMessage: '飞机将在当前位置直接降落。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedLand(),
      ),
      _GuidedAction(
        label: '起飞',
        enabled: connected && !flying,
        needsAltitude: true,
        confirmMessage: '飞控将以该高度起飞并悬停。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedTakeoff(altitude),
      ),
      _GuidedAction(
        label: '暂停/继续',
        enabled: connected && armed && flying,
        confirmMessage: '飞控会暂停当前动作并原地悬停。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedPause(),
      ),
      _GuidedAction(
        label: '环绕飞行',
        enabled: connected && flying,
        needsAltitude: true,
        confirmMessage: '飞控会绕该点盘旋（半径 50 m）。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedOrbit(
              latitude: latitude,
              longitude: longitude,
              altitudeMeters: altitude,
            ),
      ),
    ];
  }

  Future<void> _execute(_GuidedAction action) async {
    final altitude = double.tryParse(_altitude.text.trim()) ?? 0;
    final confirmed = await confirmMissionAction(
      context,
      title: action.label,
      message: action.confirmMessage,
      confirmLabel: '执行',
    );
    if (!confirmed) return;
    try {
      await action.run(
        latitude: widget.position.latitude,
        longitude: widget.position.longitude,
        altitude: altitude,
      );
      if (mounted) Navigator.of(context).pop();
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString());
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final actions = _actions();
    _GuidedAction? selectedAction;
    for (final action in actions) {
      if (action.label == _selectedActionLabel) {
        selectedAction = action;
        break;
      }
    }
    selectedAction ??= actions.isEmpty ? null : actions.first;

    return Center(
      child: MacosSheet(
        insetPadding: EdgeInsets.zero,
        child: SizedBox(
          width: 280,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(12, 10, 12, 10),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text('引导模式选项', style: MacosTheme.of(context).typography.title3),
                const SizedBox(height: 2),
                Text(
                  '${widget.position.latitude.toStringAsFixed(5)}, '
                  '${widget.position.longitude.toStringAsFixed(5)}',
                  style: MacosTheme.of(
                    context,
                  ).typography.caption1.copyWith(color: colors.textSecondary),
                ),
                const SizedBox(height: 10),
                _LabeledField(
                  label: '操作',
                  child: AdaptivePopupButton<String>(
                    value: selectedAction?.label,
                    options: [for (final action in actions) action.label],
                    labelOf: (label) => label,
                    onChanged: actions.isEmpty
                        ? null
                        : (label) =>
                              setState(() => _selectedActionLabel = label),
                  ),
                ),
                if (actions.isEmpty) ...[
                  const SizedBox(height: 8),
                  Text(
                    '当前飞行状态没有可用的引导操作。',
                    style: MacosTheme.of(
                      context,
                    ).typography.caption1.copyWith(color: colors.textSecondary),
                  ),
                ] else if (selectedAction?.enabled != true) ...[
                  const SizedBox(height: 6),
                  Text(
                    '当前飞行状态下不能执行该操作。',
                    style: MacosTheme.of(
                      context,
                    ).typography.caption1.copyWith(color: colors.warning),
                  ),
                ],
                if (selectedAction?.needsAltitude == true) ...[
                  const SizedBox(height: 8),
                  _LabeledField(
                    label: '高度 (m)',
                    child: MacosTextField(
                      controller: _altitude,
                      padding: _fieldPadding,
                    ),
                  ),
                ],
                if (_error != null) ...[
                  const SizedBox(height: 8),
                  Text(
                    _error!,
                    style: MacosTheme.of(
                      context,
                    ).typography.caption1.copyWith(color: colors.danger),
                  ),
                ],
                const SizedBox(height: 12),
                Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    PushButton(
                      controlSize: ControlSize.regular,
                      secondary: true,
                      onPressed: () => Navigator.of(context).pop(),
                      child: const Text('取消'),
                    ),
                    const SizedBox(width: 8),
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: selectedAction?.enabled != true
                          ? null
                          : () => _execute(selectedAction!),
                      child: const Text('执行'),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// 参数数值的显示格式：整数不带小数点，其余保留三位。
String formatMissionParameter(double value) => value == value.roundToDouble()
    ? value.toStringAsFixed(0)
    : value.toStringAsFixed(3);
