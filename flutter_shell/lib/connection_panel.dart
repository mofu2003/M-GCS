import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'fleet/fleet_models.dart';
import 'theme/mg_colors.dart';
import 'theme/mg_typography.dart';
import 'widgets/aligned_field_row.dart';
import 'widgets/adaptive_popup_button.dart';
import 'widgets/mac_panel.dart';
import 'widgets/mg_floating_panel.dart';
import 'widgets/parameter_load_progress.dart';

/// 连接表单的统一网格：标签列等宽、控件行等高、行间距一致。
/// 连接面板在右侧槽位中的最大宽度。
const double kConnectionPanelMaxWidth = 400;

const double _formLabelWidth = 76;
const double _formRowGap = 10;
const double _formControlHeight = 26;

/// 文本框内边距，收紧后与弹出按钮的视觉高度接近。
const EdgeInsets _formFieldPadding = EdgeInsets.symmetric(
  horizontal: 8,
  vertical: 3,
);

class ConnectionFloatingPanel extends StatefulWidget {
  const ConnectionFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<ConnectionFloatingPanel> createState() =>
      _ConnectionFloatingPanelState();
}

class _ConnectionFloatingPanelState extends State<ConnectionFloatingPanel> {
  static const _transports = [
    ConnectionTransport.serial,
    ConnectionTransport.udp,
    ConnectionTransport.tcp,
  ];

  static const _transportLabels = ['串口', 'UDP', 'TCP'];

  /// 与后台配置页同一个标签卡组件。
  late final MacosTabController _tabs = MacosTabController(
    initialIndex: 0,
    length: _transports.length,
  );

  late final TextEditingController _udpHost = TextEditingController(
    text: widget.controller.udpHost,
  );
  late final TextEditingController _udpPort = TextEditingController(
    text: widget.controller.udpPortText,
  );
  late final TextEditingController _tcpHost = TextEditingController(
    text: widget.controller.tcpHost,
  );
  late final TextEditingController _tcpPort = TextEditingController(
    text: widget.controller.tcpPortText,
  );

  int _tabIndex = 0;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
  }

  void _handleTabChanged() {
    final index = _tabs.index;
    if (!mounted || index == _tabIndex) return;
    setState(() => _tabIndex = index);
    widget.controller.selectTransport(_transports[index]);
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    _tabs.dispose();
    _udpHost.dispose();
    _udpPort.dispose();
    _tcpHost.dispose();
    _tcpPort.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        // Use the actual space remaining beside the map controls, not the
        // screen size: this also behaves correctly when the window is resized.
        final panelWidth = constraints.maxWidth.isFinite
            ? math.min(kConnectionPanelMaxWidth, constraints.maxWidth)
            : 400.0;
        final maxHeight = constraints.maxHeight.isFinite
            ? constraints.maxHeight
            : MediaQuery.sizeOf(context).height - 18;
        return AnimatedBuilder(
          // 参数下载进度只通过 parameterChanges 广播，必须一起监听，
          // 否则进度条在下载过程中不会刷新。
          animation: Listenable.merge([
            widget.controller.connectionChanges,
            widget.controller.parameterChanges,
          ]),
          builder: (context, _) {
            final theme = MacosTheme.of(context);
            final colors = MgColors.of(context);
            // 参数下载期间多留一行高度；高度受父级可用空间约束。
            final loadingParameters =
                widget.controller.isConnected &&
                !widget.controller.parametersLoaded;
            final linkRows = widget.controller.fleetLinks.length.clamp(
              0,
              MgMetrics.fleetLinkVisibleRows,
            );
            final requestedHeight =
                (loadingParameters ? 324.0 : 276.0) +
                linkRows * (MgMetrics.fleetLinkRowHeight + 8.0);
            return AnimatedContainer(
              duration: const Duration(milliseconds: 160),
              curve: Curves.easeOutCubic,
              width: panelWidth,
              height: math.min(requestedHeight, maxHeight),
              child: MgFloatingPanel(
                borderRadius: BorderRadius.circular(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Padding(
                      padding: const EdgeInsets.fromLTRB(18, 12, 18, 12),
                      child: Row(
                        children: [
                          Text('连接飞控', style: theme.typography.title3),
                          if (widget.controller.fleetLinks.length > 1) ...[
                            const SizedBox(width: 8),
                            Text(
                              '${widget.controller.fleetLinks.length} 条链路',
                              style: theme.typography.caption1.copyWith(
                                color: colors.textSecondary,
                              ),
                            ),
                          ],
                          const Spacer(),
                          _ConnectionStateLabel(controller: widget.controller),
                          const SizedBox(width: 12),
                          MacosIconButton(
                            icon: const MacosIcon(
                              CupertinoIcons.xmark,
                              size: 14,
                            ),
                            semanticLabel: '关闭连接窗口',
                            onPressed: widget.onClose,
                          ),
                        ],
                      ),
                    ),
                    // 与后台 ToolBar 同款下边框，统一使用全局细线色。
                    Divider(height: 1, thickness: 1, color: colors.divider),
                    Expanded(
                      child: Padding(
                        padding: const EdgeInsets.fromLTRB(18, 6, 18, 14),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            MgFleetLinkList(
                              links: widget.controller.fleetLinks,
                              activeVehicleId: widget.controller.sessionId,
                              activeLinkId: widget.controller.activeLinkId,
                              busy:
                                  widget.controller.busy ||
                                  widget.controller.fleetOperationsActive,
                              onSelect: widget.controller.selectLink,
                              onDisconnect: (link) => unawaited(
                                widget.controller.disconnectFleetLink(link),
                              ),
                            ),
                            if (widget.controller.fleetLinks.isNotEmpty) ...[
                              const SizedBox(height: 8),
                            ],
                            Center(
                              child: MacosSegmentedControl(
                                controller: _tabs,
                                tabs: [
                                  for (final label in _transportLabels)
                                    MacosTab(label: label),
                                ],
                              ),
                            ),
                            // 与后台标签页同一标准：标签卡到内容面板 4pt。
                            const SizedBox(height: kMacTabStripGap),
                            Expanded(
                              child: MacPanel(
                                padding: const EdgeInsets.fromLTRB(
                                  10,
                                  10,
                                  10,
                                  8,
                                ),
                                child: SingleChildScrollView(
                                  child: switch (_tabIndex) {
                                    0 => _buildSerialForm(context),
                                    1 => _buildNetworkForm(
                                      context,
                                      hostController: _udpHost,
                                      portController: _udpPort,
                                      hostPlaceholder: '0.0.0.0',
                                      portPlaceholder: '14550',
                                    ),
                                    _ => _buildNetworkForm(
                                      context,
                                      hostController: _tcpHost,
                                      portController: _tcpPort,
                                      hostPlaceholder: '127.0.0.1',
                                      portPlaceholder: '5760',
                                    ),
                                  },
                                ),
                              ),
                            ),
                            if (widget.controller.message != null ||
                                widget.controller.initializationError !=
                                    null) ...[
                              const SizedBox(height: 6),
                              Text(
                                widget.controller.initializationError ??
                                    widget.controller.message!,
                                maxLines: 2,
                                overflow: TextOverflow.ellipsis,
                                style: theme.typography.caption1.copyWith(
                                  color: widget.controller.isConnected
                                      ? colors.success
                                      : colors.textSecondary,
                                ),
                              ),
                            ],
                            if (widget.controller.isConnected &&
                                !widget.controller.parametersLoaded) ...[
                              const SizedBox(height: 10),
                              _ParameterLoadIndicator(
                                controller: widget.controller,
                              ),
                            ],
                            const SizedBox(height: 10),
                            Row(
                              children: [
                                if (_tabIndex == 0)
                                  PushButton(
                                    controlSize: ControlSize.regular,
                                    secondary: true,
                                    onPressed: widget.controller.busy
                                        ? null
                                        : widget.controller.refreshPorts,
                                    child: const Row(
                                      mainAxisSize: MainAxisSize.min,
                                      children: [
                                        MacosIcon(
                                          CupertinoIcons.refresh,
                                          size: 14,
                                        ),
                                        SizedBox(width: 7),
                                        Text('刷新串口'),
                                      ],
                                    ),
                                  ),
                                const Spacer(),
                                if (widget.controller.busy) ...[
                                  const ProgressCircle(radius: 7),
                                  const SizedBox(width: 10),
                                ],
                                PushButton(
                                  controlSize: ControlSize.regular,
                                  onPressed: widget.controller.canConnect
                                      ? widget.controller.connect
                                      : null,
                                  child: Text(
                                    widget.controller.hasSession
                                        ? '添加链路'
                                        : '连接',
                                  ),
                                ),
                                if (widget.controller.hasSession) ...[
                                  const SizedBox(width: 8),
                                  PushButton(
                                    controlSize: ControlSize.regular,
                                    secondary: true,
                                    onPressed: widget.controller.busy
                                        ? null
                                        : widget.controller.disconnect,
                                    child: const Text('断开当前'),
                                  ),
                                ],
                              ],
                            ),
                          ],
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }

  Widget _buildSerialForm(BuildContext context) {
    final controller = widget.controller;
    final portItems = controller.ports;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        AlignedFieldRow(
          label: '串口设备',
          labelWidth: _formLabelWidth,
          controlHeight: _formControlHeight,
          gap: 12,
          child: portItems.isEmpty
              ? const MacosTextField(
                  enabled: false,
                  placeholder: '未发现串口设备',
                  padding: _formFieldPadding,
                )
              : portItems.length == 1
              ? MacosTextField(
                  enabled: false,
                  placeholder: _portLabel(portItems.single),
                  padding: _formFieldPadding,
                )
              : AdaptivePopupButton<String>(
                  value: controller.selectedPort,
                  options: [for (final port in portItems) port.path],
                  labelOf: (path) => _portLabel(
                    portItems.firstWhere((port) => port.path == path),
                    menu: true,
                  ),
                  onChanged: controller.busy ? null : controller.selectPort,
                ),
        ),
        const SizedBox(height: _formRowGap),
        AlignedFieldRow(
          label: '波特率',
          labelWidth: _formLabelWidth,
          controlHeight: _formControlHeight,
          gap: 12,
          child: AdaptivePopupButton<int>(
            value: controller.baudRate,
            options: controller.baudRates,
            labelOf: (baud) => '$baud',
            onChanged: controller.busy ? null : controller.selectBaudRate,
          ),
        ),
      ],
    );
  }

  /// 串口在菜单里补充飞控标记，帮助判断该选哪一个；选中态只显示名称与路径。
  String _portLabel(SerialPortInfo port, {bool menu = false}) {
    final description = port.description.isEmpty ? port.path : port.description;
    final label = '$description  (${port.path})';
    if (menu && port.likelyFlightController) return '$label  ·  飞控';
    return label;
  }

  Widget _buildNetworkForm(
    BuildContext context, {
    required TextEditingController hostController,
    required TextEditingController portController,
    required String hostPlaceholder,
    required String portPlaceholder,
  }) {
    final disabled = widget.controller.busy;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        AlignedFieldRow(
          label: '本地地址',
          labelWidth: _formLabelWidth,
          controlHeight: _formControlHeight,
          gap: 12,
          child: MacosTextField(
            controller: hostController,
            enabled: !disabled,
            placeholder: hostPlaceholder,
            padding: _formFieldPadding,
            onChanged: (host) => widget.controller.updateNetworkEndpoint(
              host: host,
              port: portController.text,
            ),
          ),
        ),
        const SizedBox(height: _formRowGap),
        AlignedFieldRow(
          label: '端口',
          labelWidth: _formLabelWidth,
          controlHeight: _formControlHeight,
          gap: 12,
          child: MacosTextField(
            controller: portController,
            enabled: !disabled,
            placeholder: portPlaceholder,
            padding: _formFieldPadding,
            keyboardType: TextInputType.number,
            onChanged: (port) => widget.controller.updateNetworkEndpoint(
              host: hostController.text,
              port: port,
            ),
          ),
        ),
        _ConnectionFormError(
          disabled
              ? null
              : _networkValidationError(
                  hostController: hostController,
                  portController: portController,
                ),
        ),
      ],
    );
  }

  /// 输入过程中即时校验，错误直接写在控件下方。
  String? _networkValidationError({
    required TextEditingController hostController,
    required TextEditingController portController,
  }) {
    final host = hostController.text.trim();
    if (host.isEmpty) return '请填写地址';
    if (host.contains(RegExp(r'\s'))) return '地址不能包含空格';
    if (host.contains(':')) return '地址与端口需要分开填写';
    final text = portController.text.trim();
    if (text.isEmpty) return '请填写端口';
    final port = int.tryParse(text);
    if (port == null) return '端口必须是数字';
    if (port < 1 || port > 65535) return '端口范围是 1–65535';
    return null;
  }
}

/// 当前链路列表。每行代表一条物理链路的一个 route；选择只切换 UI 焦点，
/// 断开只关闭指定链路，两者都不会生成飞行命令。
class MgFleetLinkList extends StatelessWidget {
  const MgFleetLinkList({
    super.key,
    required this.links,
    required this.activeVehicleId,
    required this.activeLinkId,
    required this.busy,
    required this.onSelect,
    required this.onDisconnect,
  });

  final List<FleetLinkSummary> links;
  final int? activeVehicleId;
  final int? activeLinkId;
  final bool busy;
  final ValueChanged<int> onSelect;
  final ValueChanged<FleetLinkSummary> onDisconnect;

  String _transportLabel(FleetLinkSummary link) =>
      switch (link.endpoint.split('://').first) {
        'serial' => '串口',
        'tcp' => 'TCP',
        _ => 'UDP',
      };

  String _stateLabel(FleetLinkSummary link) {
    if (link.connected) return '已连接';
    if (link.state == 'duplicate_system_id') return 'SYSID 冲突';
    if (link.state == 'waiting_heartbeat') return '等待心跳';
    return '未连接';
  }

  Color _stateColor(MgColors colors, FleetLinkSummary link) {
    if (link.connected) return colors.success;
    if (link.state == 'duplicate_system_id') return colors.danger;
    if (link.state == 'waiting_heartbeat') return colors.warning;
    return colors.textSecondary;
  }

  String _subtitle(FleetLinkSummary link) {
    final parts = <String>[_transportLabel(link)];
    if (link.systemId > 0) parts.add('sysid ${link.systemId}');
    if (link.vehicleId != null) {
      parts.add('飞机 ${link.vehicleId}');
    } else {
      parts.add('未识别');
    }
    return parts.join(' · ');
  }

  bool _isPrimaryRoute(FleetLinkSummary link) {
    if (!link.primary) return false;
    return true;
  }

  bool _hasPrimaryRoute(FleetLinkSummary link) {
    return links.any(
      (other) => other.vehicleId == link.vehicleId && other.primary,
    );
  }

  @override
  Widget build(BuildContext context) {
    if (links.isEmpty) return const SizedBox.shrink();
    final theme = MacosTheme.of(context);
    final colors = MgColors.of(context);
    final visibleCount = math.min(links.length, MgMetrics.fleetLinkVisibleRows);
    final physicalLinkCount = links.map((link) => link.id).toSet().length;
    final countLabel = links.length == physicalLinkCount
        ? '$physicalLinkCount 条'
        : '$physicalLinkCount 条 · ${links.length} 个飞控';
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Text(
              '当前链路',
              style: theme.typography.caption1.copyWith(
                color: colors.textSecondary,
              ),
            ),
            const Spacer(),
            Text(
              countLabel,
              style: theme.typography.caption1.copyWith(
                color: colors.textSecondary,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        SizedBox(
          height:
              visibleCount * MgMetrics.fleetLinkRowHeight +
              (visibleCount - 1) * 4,
          child: ListView.separated(
            padding: EdgeInsets.zero,
            itemCount: links.length,
            separatorBuilder: (_, index) => const SizedBox(height: 4),
            itemBuilder: (context, index) {
              final link = links[index];
              final active =
                  link.vehicleId != null && activeVehicleId == link.vehicleId;
              final waitingSelected =
                  link.vehicleId == null && activeLinkId == link.id;
              final canSelect = link.vehicleId != null;
              final highlight = active || waitingSelected;
              final showPrimary =
                  active && (_isPrimaryRoute(link) || !_hasPrimaryRoute(link));
              return Material(
                color: highlight ? colors.accentSurface : colors.surfaceMuted,
                borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
                child: SizedBox(
                  height: MgMetrics.fleetLinkRowHeight,
                  child: Row(
                    children: [
                      Expanded(
                        child: InkWell(
                          key: ValueKey('fleet-link-${link.rowKey}'),
                          borderRadius: BorderRadius.circular(
                            MgMetrics.controlRadius,
                          ),
                          onTap: active || busy || !canSelect
                              ? null
                              : () => onSelect(link.id),
                          child: Padding(
                            padding: const EdgeInsets.only(left: 9),
                            child: Row(
                              children: [
                                Icon(
                                  _transportLabel(link) == '串口'
                                      ? Icons.usb
                                      : Icons.lan_outlined,
                                  size: 15,
                                  color: highlight
                                      ? colors.accent
                                      : colors.textSecondary,
                                ),
                                const SizedBox(width: 8),
                                Expanded(
                                  child: Column(
                                    mainAxisAlignment: MainAxisAlignment.center,
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      Text(
                                        link.endpoint,
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                        style: theme.typography.caption1
                                            .copyWith(
                                              color: highlight
                                                  ? colors.textPrimary
                                                  : colors.textSecondary,
                                            ),
                                      ),
                                      const SizedBox(height: 1),
                                      Text(
                                        _subtitle(link),
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                        style: theme.typography.caption2
                                            .copyWith(
                                              color: colors.textTertiary,
                                            ),
                                      ),
                                    ],
                                  ),
                                ),
                                const SizedBox(width: 8),
                                Text(
                                  _stateLabel(link),
                                  style: theme.typography.caption2.copyWith(
                                    color: _stateColor(colors, link),
                                  ),
                                ),
                                if (showPrimary) ...[
                                  const SizedBox(width: 6),
                                  MacosIcon(
                                    CupertinoIcons.checkmark,
                                    size: 13,
                                    color: colors.accent,
                                  ),
                                ],
                              ],
                            ),
                          ),
                        ),
                      ),
                      MacosIconButton(
                        key: ValueKey('fleet-link-disconnect-${link.rowKey}'),
                        icon: MacosIcon(CupertinoIcons.xmark, size: 12),
                        semanticLabel: '断开 ${link.endpoint}',
                        boxConstraints: const BoxConstraints.tightFor(
                          width: 28,
                          height: 28,
                        ),
                        padding: EdgeInsets.zero,
                        backgroundColor: colors.transparent,
                        hoverColor: colors.mapControlHover,
                        onPressed: busy ? null : () => onDisconnect(link),
                      ),
                      const SizedBox(width: 5),
                    ],
                  ),
                ),
              );
            },
          ),
        ),
      ],
    );
  }
}

/// 仅在输入非法时显示的校验提示；正常状态下完全不占位。
class _ConnectionFormError extends StatelessWidget {
  const _ConnectionFormError(this.message);

  final String? message;

  @override
  Widget build(BuildContext context) {
    final error = message;
    if (error == null) return const SizedBox.shrink();
    return Padding(
      padding: const EdgeInsets.only(left: _formLabelWidth + 12, top: 6),
      child: Text(
        error,
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(color: MgColors.of(context).danger),
      ),
    );
  }
}

/// 飞控连接后参数下载进度；参数读完自动消失。
class _ParameterLoadIndicator extends StatelessWidget {
  const _ParameterLoadIndicator({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    return ParameterLoadProgress(
      received: controller.parameterReceived,
      total: controller.parameterTotal,
      leading: '正在加载参数',
    );
  }
}

class _ConnectionStateLabel extends StatelessWidget {
  const _ConnectionStateLabel({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    final (label, color) = switch (controller.phase) {
      SerialConnectionPhase.connected => ('已连接', colors.success),
      SerialConnectionPhase.waitingForHeartbeat => ('等待心跳', colors.warning),
      SerialConnectionPhase.disconnected => ('未连接', colors.textSecondary),
    };
    return Text(
      label,
      style: MacosTheme.of(context).typography.callout.copyWith(color: color),
    );
  }
}
