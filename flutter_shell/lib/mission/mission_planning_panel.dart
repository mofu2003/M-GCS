import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/adaptive_popup_button.dart';
import '../widgets/mg_command_confirmation.dart';
import '../widgets/mg_confirm.dart';
import '../widgets/mg_dialog.dart';
import '../widgets/mg_floating_panel.dart';
import '../widgets/mac_panel.dart';
import 'mission_document_controller.dart';
import 'mission_document_models.dart';
import 'mission_transfer_section.dart';
import 'mission_geo.dart';

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
    this.onRequestCommandConfirmation,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  /// 把“开始任务”变成地图悬浮确认层的长按确认指令。
  final ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation;

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
  Timer? _executionTimer;

  // This widget is keyed by session; detach the exact instance registered in
  // initState rather than a getter which may already point to the next vehicle.
  late final MissionDocumentController _mission =
      widget.controller.missionPlanning;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
    _mission.addListener(_handleMissionChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) unawaited(_mission.initialize());
    });
    // 面板只在显示时挂载，所以这里的轮询随面板出现/消失自动开始和停止。
    _executionTimer = Timer.periodic(
      const Duration(seconds: 1),
      (_) => unawaited(_mission.refreshExecutionProgress()),
    );
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
    _executionTimer?.cancel();
    _tabs.removeListener(_handleTabChanged);
    _mission.removeListener(_handleMissionChanged);
    _tabs.dispose();
    super.dispose();
  }

  void _toggleCollapsed() => setState(() => _collapsed = !_collapsed);

  /// 把当前飞控的机型/固件告诉任务文档控制器，新建文档时写进 metadata。
  void _syncVehicleContext() {
    final firmware = widget.controller.firmwareInfo;
    _mission.setVehicleContext(
      vehicleClass: firmware?['vehicle_type'] as String?,
      firmwareProfile: firmware?['firmware'] as String?,
    );
  }

  @override
  Widget build(BuildContext context) {
    _syncVehicleContext();
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
      child: MgFloatingPanel(
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
          MissionTransferSection(
            mission: _mission,
            controller: widget.controller,
            onRequestCommandConfirmation: widget.onRequestCommandConfirmation,
          ),
          const SizedBox(height: 8),
          if (_mission.executionProgress?.active ?? false) ...[
            _MissionExecutionProgressBar(progress: _mission.executionProgress!),
            const SizedBox(height: 8),
          ],
          Expanded(
            child: entities.isEmpty
                ? const _MissionEmptyState()
                : ReorderableListView.builder(
                    buildDefaultDragHandles: false,
                    itemCount: entities.length,
                    onReorderItem: (oldIndex, newIndex) => unawaited(
                      _mutate(() => _mission.reorderEntity(oldIndex, newIndex)),
                    ),
                    itemBuilder: (context, index) {
                      final entity = entities[index];
                      return Padding(
                        key: _entityKeys.putIfAbsent(entity.id, GlobalKey.new),
                        padding: const EdgeInsets.only(bottom: 7),
                        child: GestureDetector(
                          behavior: HitTestBehavior.opaque,
                          onTap: () => _mission.selectEntity(index),
                          child: _MissionEntityCard(
                            position: index + 1,
                            reorderIndex: index,
                            selected: _mission.selectedEntityIndex == index,
                            enabled: enabled,
                            entity: entity,
                            catalog: _catalogForEntity(entity),
                            distanceLabel: _distanceFromPrevious(
                              entities,
                              index,
                            ),
                            onCommandChanged: (command) => _mutate(
                              () => _mission.updateEntity(
                                entity.copyWith(
                                  command: command,
                                  type: _typeForCommand(command),
                                ),
                              ),
                            ),
                            onDelete: () =>
                                _mutate(() => _mission.removeEntity(entity.id)),
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
                              onSave: (updated) =>
                                  _mutate(() => _mission.updateEntity(updated)),
                            ),
                          ),
                        ),
                      );
                    },
                  ),
          ),
        ],
      );
    },
  );

  /// 当前飞控机型下的命令目录；保留 [entity] 自身命令，避免下拉错位。
  List<MissionCommandDescriptor> _catalogForEntity(MissionEntity? entity) =>
      filterCommandCatalogForVehicle(
        _mission.commandCatalog,
        widget.controller.firmwareInfo?['vehicle_type'] as String?,
        keepCommands: [if (entity != null) entity.command],
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
      final enabled = !_mission.busy && !_mission.transferActive;
      return SingleChildScrollView(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const _MissionSectionTitle('Home / 返航基准'),
            const SizedBox(height: 6),
            _HomeCard(
              controller: widget.controller,
              home: document.home,
              enabled: enabled,
              onChanged: _updateHome,
            ),
            const SizedBox(height: 14),
            const _MissionSectionTitle('任务默认值'),
            const SizedBox(height: 6),
            _MissionDefaultsForm(
              defaults: document.defaults,
              enabled: enabled,
              onSubmit: (defaults) async {
                try {
                  await _mission.replaceDocument(
                    document.copyWith(defaults: defaults),
                  );
                } on Object {
                  // 错误走 status。
                }
              },
            ),
            const SizedBox(height: 14),
            const _MissionSectionTitle('围栏 / Rally'),
            const SizedBox(height: 6),
            _FenceRallyCard(
              mission: _mission,
              enabled: enabled && widget.controller.isConnected,
            ),
          ],
        ),
      );
    },
  );

  String? _distanceFromPrevious(List<MissionEntity> entities, int index) {
    if (index <= 0) return null;
    final current = entities[index];
    if (!current.enabled) return null;
    MissionEntity? previous;
    for (var cursor = index - 1; cursor >= 0; cursor--) {
      final candidate = entities[cursor];
      if (candidate.enabled) {
        previous = candidate;
        break;
      }
    }
    if (previous == null) return null;
    final lat1 = previous.latitude;
    final lon1 = previous.longitude;
    final lat2 = current.latitude;
    final lon2 = current.longitude;
    if (lat1 == null ||
        lon1 == null ||
        lat2 == null ||
        lon2 == null ||
        !lat1.isFinite ||
        !lon1.isFinite ||
        !lat2.isFinite ||
        !lon2.isFinite) {
      return null;
    }
    final meters = wgs84DistanceMeters(LatLng(lat1, lon1), LatLng(lat2, lon2));
    return '距上一点 ${formatMissionDistance(meters)}';
  }
}

/// 任务执行进度条：来源是飞控的 MISSION_CURRENT + MISSION_ITEM_REACHED。
class _MissionExecutionProgressBar extends StatelessWidget {
  const _MissionExecutionProgressBar({required this.progress});

  final MissionExecutionProgress progress;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    final total = progress.total;
    final reached = progress.reachedCount.clamp(0, total);
    final ratio = total == 0 ? 0.0 : reached / total;
    final done = progress.finished;
    // 三档文案：航段飞完（还在返航/降落）与真正任务完成分开，避免提前报"完成"。
    final label = done
        ? '任务完成 · 共 $total 项'
        : progress.aborted
        ? '任务已中止 · 共 $total 项'
        : progress.reachedEnd
        ? '航段已完成 · 共 $total 项，等待返航/降落'
        : '任务进度 第 ${(progress.currentIndex + 1).clamp(1, total)}/$total 项 · '
              '已到达 $reached 项';
    return MissionCardFrame(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MacosIcon(
                done
                    ? CupertinoIcons.checkmark_circle
                    : progress.aborted
                    ? CupertinoIcons.exclamationmark_triangle
                    : CupertinoIcons.location_north,
                size: 13,
                color: done
                    ? colors.success
                    : progress.aborted
                    ? colors.danger
                    : progress.reachedEnd
                    ? colors.warning
                    : colors.accent,
              ),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  label,
                  style: typography.caption1.copyWith(
                    color: colors.textPrimary,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ],
          ),
          const SizedBox(height: 6),
          ProgressBar(
            value: ratio * 100,
            height: MgMetrics.motorBarHeight,
            trackColor: done
                ? colors.success
                : progress.aborted
                ? colors.danger
                : progress.reachedEnd
                ? colors.warning
                : colors.accent,
            backgroundColor: colors.surfaceMuted,
            semanticLabel: label,
          ),
        ],
      ),
    );
  }
}

/// 围栏 / Rally 操作组。
///
/// 两者是独立的 MAVLink mission type，不随任务上传；下发流程自带回读核对。
class _FenceRallyCard extends StatelessWidget {
  const _FenceRallyCard({required this.mission, required this.enabled});

  final MissionDocumentController mission;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MissionCardFrame(
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

  /// 围栏 / Rally 下发同样会覆盖飞控上的同类数据，先确认（KD-08）。
  Future<Object?> _confirmUpload(
    BuildContext context,
    String label,
    Future<Object?> Function() upload,
  ) async {
    final confirmed = await confirmMissionAction(
      context,
      title: '下发$label到飞控？',
      message: '这会把本地$label写入飞控，覆盖飞控上的同类数据，下发后会自动回读核对。',
      confirmLabel: '下发$label',
    );
    if (!confirmed) return null;
    return upload();
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

  Future<void> _useVehiclePosition() async {
    final fix = widget.controller.vehiclePosition;
    if (!widget.controller.vehiclePositionValid || fix == null) return;
    final currentAltitude = double.tryParse(_altitude.text) ?? 0;
    _latitude.text = fix.latitude.toStringAsFixed(7);
    _longitude.text = fix.longitude.toStringAsFixed(7);
    _altitude.text = (fix.altitudeMeters ?? currentAltitude).toStringAsFixed(1);
    await _save();
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
              if (widget.controller.vehiclePositionValid)
                Tooltip(
                  message: '使用飞机当前位置',
                  child: MacosIconButton(
                    icon: const MacosIcon(CupertinoIcons.location, size: 13),
                    semanticLabel: '使用飞机当前位置',
                    onPressed: widget.enabled
                        ? () => unawaited(_useVehiclePosition())
                        : null,
                  ),
                ),
              PushButton(
                controlSize: ControlSize.regular,
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
              controlSize: ControlSize.regular,
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
class _MissionSectionTitle extends StatelessWidget {
  const _MissionSectionTitle(this.text);

  final String text;

  @override
  Widget build(BuildContext context) => Text(
    text,
    style: MacosTheme.of(
      context,
    ).typography.body.copyWith(color: context.mgColors.textSecondary),
  );
}

class _MissionEmptyState extends StatelessWidget {
  const _MissionEmptyState();

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            MacosIcon(
              CupertinoIcons.map_pin_ellipse,
              size: 30,
              color: colors.textTertiary,
            ),
            const SizedBox(height: 8),
            Text('还没有任务项', style: MacosTheme.of(context).typography.body),
            const SizedBox(height: 4),
            Text(
              '保持规划面板打开，单击地图即可添加航点',
              textAlign: TextAlign.center,
              style: MacosTheme.of(
                context,
              ).typography.caption1.copyWith(color: colors.textSecondary),
            ),
          ],
        ),
      ),
    );
  }
}

class _MissionEntityCard extends StatelessWidget {
  const _MissionEntityCard({
    required this.position,
    required this.reorderIndex,
    required this.selected,
    required this.entity,
    required this.catalog,
    required this.distanceLabel,
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
  final String? distanceLabel;
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
          const SizedBox(height: 6),
          Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              SizedBox(
                width: 74,
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
              const SizedBox(width: 10),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      distanceLabel ?? '起点',
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MacosTheme.of(context).typography.caption1
                          .copyWith(color: colors.textTertiary),
                    ),
                    const SizedBox(height: 2),
                    Text(
                      _positionLabel(),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MacosTheme.of(context).typography.caption1
                          .copyWith(color: colors.textSecondary),
                    ),
                  ],
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
    return '${latitude.toStringAsFixed(7)}, ${longitude.toStringAsFixed(7)}';
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
  late int _frame = widget.defaults.frame;

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
        frame: _frame,
        altitudeMeters: altitude,
        speedMetersPerSecond: speed,
      ),
    );
  }

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      _LabeledField(
        label: '默认高度基准',
        child: AdaptivePopupButton<int>(
          value: _frame,
          options: [for (final choice in kMissionFrameChoices) choice.$1],
          labelOf: missionFrameLabel,
          onChanged: widget.enabled
              ? (frame) => setState(() => _frame = frame)
              : null,
        ),
      ),
      const SizedBox(height: 10),
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
  );
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

/// 任务面板的确认弹窗，委托给全应用统一的 [showMgConfirmDialog]。
Future<bool> confirmMissionAction(
  BuildContext context, {
  required String title,
  required String message,
  required String confirmLabel,
}) => showMgConfirmDialog(
  context,
  title: title,
  message: message,
  confirmLabel: confirmLabel,
);

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
  final fullCatalog = mission.commandCatalog.isEmpty
      ? catalog
      : mission.commandCatalog;
  // 命令目录按当前飞控机型过滤；已有实体自身的命令始终保留，避免编辑一个
  // 当前机型不支持的项时下拉落到别的命令上。
  final vehicleCatalog = filterCommandCatalogForVehicle(
    fullCatalog,
    controller.firmwareInfo?['vehicle_type'] as String?,
    keepCommands: [if (entity != null) entity.command],
  );
  if (vehicleCatalog.isEmpty) return;
  await _showFadeDialog<void>(
    context: context,
    builder: (dialogContext) => _MissionEntitySheet(
      catalog: vehicleCatalog,
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
}) => showMgSheet<T>(context: context, builder: builder);

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

  /// 高度基准：沿用既有实体的帧，新建时取任务默认帧。
  late int _frame = widget.entity?.frame ?? widget.defaults.frame;
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
      frame: _descriptor.requiresAltitude ? _frame : base.frame,
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
    final isVtolTakeoff = _descriptor.command == kMavCmdNavVtolTakeoff;
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
              mainAxisSize: MainAxisSize.min,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        widget.entity == null
                            ? '添加${_descriptor.label}'
                            : '编辑${_descriptor.label}',
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
                            label: isVtolTakeoff ? '参考纬度' : '纬度',
                            child: MacosTextField(
                              controller: _latitude,
                              padding: _fieldPadding,
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: _LabeledField(
                            label: isVtolTakeoff ? '参考经度' : '经度',
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
                            label: _frame == kMissionFrameTerrainAlt
                                ? '高度 (m, AGL)'
                                : '高度 (m)',
                            child: MacosTextField(
                              controller: _altitude,
                              padding: _fieldPadding,
                            ),
                          ),
                        ),
                    ],
                  ),
                ],
                if (_descriptor.requiresAltitude) ...[
                  const SizedBox(height: 8),
                  _LabeledField(
                    label: '高度基准',
                    child: AdaptivePopupButton<int>(
                      value: _frame,
                      options: [
                        for (final choice in kMissionFrameChoices) choice.$1,
                      ],
                      labelOf: missionFrameLabel,
                      onChanged: (frame) => setState(() => _frame = frame),
                    ),
                  ),
                  if (_frame == kMissionFrameTerrainAlt) ...[
                    const SizedBox(height: 6),
                    Text(
                      '地形跟随由飞控按地形数据执行，需要飞控载有该区域的地形数据；'
                      '否则该航点不能按 AGL 高度执行。',
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                  ],
                ],
                if (isVtolTakeoff) ...[
                  const SizedBox(height: 8),
                  DecoratedBox(
                    decoration: BoxDecoration(
                      color: colors.surfaceMuted,
                      borderRadius: BorderRadius.circular(
                        MgMetrics.controlRadius,
                      ),
                      border: Border.all(color: colors.divider),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 9,
                        vertical: 7,
                      ),
                      child: Text(
                        'ArduPilot：先在当前水平位置垂直爬升到设定高度，再转固定翼并飞向下一航点。'
                        '此处经纬度是 MAVLink 兼容参考点，不是要求实际飞抵的航点。',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                    ),
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

/// “改变高度”对话框。
///
/// 这是**独立于引导操作菜单**的入口（挂在左侧飞行动作条里），只负责收一个目标
/// 高度（相对 Home），再调用 [ConnectionController.guidedChangeAltitude]。
/// 默认填当前相对高度，和 QGC 的高度滑块从当前值起步一致。
Future<void> showChangeAltitudeDialog(
  BuildContext context, {
  required ConnectionController controller,
  ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation,
}) async {
  final current = controller.relativeAltitudeMeters;
  if (!controller.vehiclePositionValid || current == null) return;
  await _showFadeDialog<void>(
    context: context,
    builder: (dialogContext) => _ChangeAltitudeSheet(
      controller: controller,
      initialAltitudeMeters: current,
      onRequestCommandConfirmation: onRequestCommandConfirmation,
    ),
  );
}

class _ChangeAltitudeSheet extends StatefulWidget {
  const _ChangeAltitudeSheet({
    required this.controller,
    required this.initialAltitudeMeters,
    this.onRequestCommandConfirmation,
  });

  final ConnectionController controller;
  final double initialAltitudeMeters;
  final ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation;

  @override
  State<_ChangeAltitudeSheet> createState() => _ChangeAltitudeSheetState();
}

class _ChangeAltitudeSheetState extends State<_ChangeAltitudeSheet> {
  late final TextEditingController _altitude = TextEditingController(
    text: widget.initialAltitudeMeters.toStringAsFixed(1),
  );
  String? _error;
  late final int _inputFocusRevision;

  @override
  void initState() {
    super.initState();
    _inputFocusRevision = widget.controller.fleet.focusRevision;
  }

  bool _validateInputTarget() {
    widget.controller.fleet.refresh();
    if (widget.controller.fleet.focusRevision == _inputFocusRevision) {
      return true;
    }
    setState(() => _error = '飞机已切换或断开，请关闭窗口后重新操作');
    return false;
  }

  @override
  void dispose() {
    _altitude.dispose();
    super.dispose();
  }

  Future<void> _execute() async {
    if (!_validateInputTarget()) return;
    final target = double.tryParse(_altitude.text.trim());
    if (target == null || !target.isFinite) {
      setState(() => _error = '请输入有效的高度');
      return;
    }
    final onRequest = widget.onRequestCommandConfirmation;
    if (onRequest != null) {
      final command = MgCommandConfirmation(
        label: '改变高度',
        execute: () => widget.controller.guidedChangeAltitude(target),
      );
      Navigator.of(context).pop();
      onRequest(command);
      return;
    }

    final confirmed = await confirmMissionAction(
      context,
      title: '改变高度',
      message: '飞机会在当前位置升/降到 ${target.toStringAsFixed(1)} m（相对 Home）并悬停。',
      confirmLabel: '执行',
    );
    if (!confirmed || !mounted || !_validateInputTarget()) return;
    try {
      await widget.controller.guidedChangeAltitude(target);
      if (mounted) Navigator.of(context).pop();
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString());
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
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
                Text('改变高度', style: MacosTheme.of(context).typography.title3),
                const SizedBox(height: 2),
                Text(
                  '当前 ${widget.initialAltitudeMeters.toStringAsFixed(1)} m（相对 Home）',
                  style: MacosTheme.of(
                    context,
                  ).typography.caption1.copyWith(color: colors.textSecondary),
                ),
                const SizedBox(height: 10),
                _LabeledField(
                  label: '高度 (m)',
                  child: MacosTextField(
                    controller: _altitude,
                    padding: _fieldPadding,
                  ),
                ),
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
                      onPressed: _execute,
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

/// 引导操作：门控条件照抄 QGC `GuidedActionsController.qml` 的 `show*` 属性。
Future<void> showGuidedActionsDialog(
  BuildContext context, {
  required ConnectionController controller,
  required LatLng position,
  String? initialActionLabel,
  ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation,
}) async {
  await _showFadeDialog<void>(
    context: context,
    builder: (dialogContext) => _GuidedActionsSheet(
      controller: controller,
      position: position,
      initialActionLabel: initialActionLabel,
      onRequestCommandConfirmation: onRequestCommandConfirmation,
    ),
  );
}

class _GuidedAction {
  const _GuidedAction({
    required this.label,
    required this.disabledReason,
    required this.confirmMessage,
    required this.run,
    this.needsAltitude = false,
  });

  final String label;
  final String? disabledReason;
  final String confirmMessage;
  final bool needsAltitude;
  bool get enabled => disabledReason == null;
  final Future<void> Function({
    required double latitude,
    required double longitude,
    required double altitude,
  })
  run;
}

class _GuidedActionsSheet extends StatefulWidget {
  const _GuidedActionsSheet({
    required this.controller,
    required this.position,
    this.initialActionLabel,
    this.onRequestCommandConfirmation,
  });

  final ConnectionController controller;
  final LatLng position;
  final String? initialActionLabel;

  /// 起飞仍需先在该弹窗填写高度；点击执行后改由顶部悬浮按钮长按确认。
  final ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation;

  @override
  State<_GuidedActionsSheet> createState() => _GuidedActionsSheetState();
}

class _GuidedActionsSheetState extends State<_GuidedActionsSheet> {
  late final TextEditingController _altitude = TextEditingController(
    text: _defaultAltitude().toStringAsFixed(1),
  );
  late String? _selectedActionLabel = widget.initialActionLabel;
  String? _error;
  late final int _inputFocusRevision;

  @override
  void initState() {
    super.initState();
    _inputFocusRevision = widget.controller.fleet.focusRevision;
  }

  bool _validateInputTarget() {
    widget.controller.fleet.refresh();
    if (widget.controller.fleet.focusRevision == _inputFocusRevision) {
      return true;
    }
    setState(() => _error = '飞机已切换或断开，请关闭窗口后重新操作');
    return false;
  }

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
    final stateFresh = controller.vehicleStateFresh;
    final armed = stateFresh && controller.vehicleArmed;
    final flying = stateFresh && controller.vehicleFlying;
    // MAV_TYPE_FIXED_WING = 1：QGC 不给固定翼显示“降落”。
    final fixedWing = controller.vehicleType == 1;
    final mode = controller.vehicleFlightMode.toUpperCase();
    final inAuto = mode.contains('AUTO');
    final inRtl = mode.contains('RTL');
    final inLand = mode.contains('LAND');
    final guidedSupported = controller.supportsGuidedMode;
    final pauseSupported = controller.supportsPause;
    final homeReady =
        controller.homePositionValid &&
        controller.homePosition?.altitudeMeters != null;

    String? firstDisabled(List<String?> reasons) {
      for (final reason in reasons) {
        if (reason != null) return reason;
      }
      return null;
    }

    return [
      _GuidedAction(
        label: '移动到此处',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!guidedSupported) '当前飞控未声明支持引导模式',
          if (!flying) '飞机尚未起飞',
          // QGC `guidedModeGotoLocation` 在高度未知时直接拒发；这里同样门控。
          if (controller.absoluteAltitudeMeters == null ||
              controller.absoluteAltitudeMeters == 0)
            '还没有有效的海拔高度',
        ]),
        // QGC 的 Go to location 不给高度输入框：目标高度固定取飞机**当前 AMSL**，
        // 也就是"保持现在的高度飞过去"。
        confirmMessage: inAuto
            ? '当前为 AUTO。执行后会切换到 GUIDED，并离开当前任务。'
            : '飞机会保持当前高度飞往该坐标。',
        run: ({required latitude, required longitude, required altitude}) {
          final current = controller.absoluteAltitudeMeters;
          if (current == null || current == 0) {
            throw StateError('移动到此处失败：没有有效的海拔高度');
          }
          return controller.guidedGoto(latitude, longitude, current);
        },
      ),
      _GuidedAction(
        label: '返航 (RTL)',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!guidedSupported) '当前飞控未声明支持引导模式',
          if (!armed) '飞控未解锁',
          if (!flying) '飞机尚未起飞',
          if (inRtl) '飞机已在返航模式',
        ]),
        confirmMessage: '飞机将返回起飞点（Home）并自动降落。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedReturnToLaunch(),
      ),
      _GuidedAction(
        label: '降落',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!guidedSupported) '当前飞控未声明支持引导模式',
          if (!armed) '飞控未解锁',
          // QGC `GuidedActionsController.qml`：Land 只在飞行中出现，地面不给。
          if (!flying) '飞机尚未起飞',
          if (fixedWing) '固定翼不支持原地引导降落',
          if (inLand) '飞机已在降落模式',
        ]),
        confirmMessage: '飞机将在当前位置直接降落。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedLand(),
      ),
      _GuidedAction(
        label: '起飞',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!controller.supportsGuidedTakeoff) '当前机型不支持引导起飞',
          if (flying) '飞机已经在空中',
        ]),
        needsAltitude: true,
        confirmMessage: '飞控将进入 GUIDED，解锁后以该高度起飞并悬停。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedTakeoff(altitude),
      ),
      _GuidedAction(
        label: '暂停/继续',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!guidedSupported) '当前飞控未声明支持引导模式',
          if (!pauseSupported) '当前飞控不支持暂停',
          if (!armed) '飞控未解锁',
          if (!flying) '飞机尚未起飞',
        ]),
        confirmMessage: '飞控会暂停当前动作并原地悬停。',
        run: ({required latitude, required longitude, required altitude}) =>
            controller.guidedPause(),
      ),
      _GuidedAction(
        label: '环绕飞行',
        disabledReason: firstDisabled([
          if (!connected) '未连接飞控',
          if (connected && !stateFresh) '飞控状态已过期',
          if (!guidedSupported) '当前飞控未声明支持引导模式',
          if (!flying) '飞机尚未起飞',
          if (!homeReady) '需要有效的 Home 点和高度',
        ]),
        needsAltitude: true,
        confirmMessage: '飞控会绕该点盘旋（半径 50 m）。',
        // QGC `GuidedActionsController` 传的是 `homePosition.altitude + 滑块值`
        // （AMSL），所以这里把用户填的相对高度换算成 AMSL 再下发。
        run: ({required latitude, required longitude, required altitude}) {
          final homeAltitude = controller.homePosition?.altitudeMeters;
          if (homeAltitude == null) {
            throw StateError('环绕飞行失败：Home 高度未知');
          }
          return controller.guidedOrbit(
            latitude: latitude,
            longitude: longitude,
            altitudeMeters: homeAltitude + altitude,
          );
        },
      ),
    ];
  }

  Future<void> _execute(_GuidedAction action) async {
    if (!_validateInputTarget()) return;
    final altitude = double.tryParse(_altitude.text.trim()) ?? 0;
    final onRequest = widget.onRequestCommandConfirmation;
    if (action.label == '起飞' && onRequest != null) {
      final command = MgCommandConfirmation(
        label: action.label,
        execute: () => action.run(
          latitude: widget.position.latitude,
          longitude: widget.position.longitude,
          altitude: altitude,
        ),
      );
      Navigator.of(context).pop();
      onRequest(command);
      return;
    }

    final confirmed = await confirmMissionAction(
      context,
      title: action.label,
      message: action.confirmMessage,
      confirmLabel: '执行',
    );
    if (!confirmed || !mounted || !_validateInputTarget()) return;
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
    selectedAction ??= actions.isEmpty
        ? null
        : actions.firstWhere(
            (action) => action.enabled,
            orElse: () => actions.first,
          );

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
                        : (label) => setState(() {
                            _selectedActionLabel = label;
                            _error = null;
                          }),
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
                ] else if (selectedAction?.disabledReason != null) ...[
                  const SizedBox(height: 6),
                  Text(
                    selectedAction!.disabledReason!,
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
