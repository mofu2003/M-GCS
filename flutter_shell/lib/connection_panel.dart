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

  /// 与参数编辑页面同一个标签卡组件。
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
          animation: widget.controller.connectionChanges,
          builder: (context, _) {
            final theme = MacosTheme.of(context);
            final colors = MgColors.of(context);
            final formError = widget.controller.busy || _tabIndex == 0
                ? null
                : _networkValidationError(
                    hostController: _tabIndex == 1 ? _udpHost : _tcpHost,
                    portController: _tabIndex == 1 ? _udpPort : _tcpPort,
                  );
            final notice =
                widget.controller.initializationError ??
                formError ??
                widget.controller.message;
            return ConstrainedBox(
              constraints: BoxConstraints(maxHeight: maxHeight),
              child: SizedBox(
                width: panelWidth,
                child: MgFloatingPanel(
                  borderRadius: BorderRadius.circular(12),
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
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
                            _ConnectionStateLabel(
                              controller: widget.controller,
                            ),
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
                      // 与参数编辑页面工具栏 同款下边框，统一使用全局细线色。
                      Divider(height: 1, thickness: 1, color: colors.divider),
                      Flexible(
                        child: SingleChildScrollView(
                          child: Padding(
                            padding: const EdgeInsets.fromLTRB(18, 6, 18, 14),
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
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
                                if (widget
                                    .controller
                                    .fleetLinks
                                    .isNotEmpty) ...[
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
                                // 与参数编辑页面标签页同一标准：标签卡到内容面板 4pt。
                                const SizedBox(height: kMacTabStripGap),
                                SizedBox(
                                  height: 96,
                                  child: MacPanel(
                                    padding: const EdgeInsets.all(10),
                                    child: Center(
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
                                const SizedBox(height: 6),
                                SizedBox(
                                  height: 32,
                                  child: notice == null
                                      ? null
                                      : Tooltip(
                                          message: notice,
                                          child: Align(
                                            alignment: Alignment.centerLeft,
                                            child: Text(
                                              notice,
                                              maxLines: 2,
                                              overflow: TextOverflow.ellipsis,
                                              style: theme.typography.caption1
                                                  .copyWith(
                                                    color:
                                                        widget
                                                                    .controller
                                                                    .initializationError !=
                                                                null ||
                                                            formError != null
                                                        ? colors.danger
                                                        : colors.textSecondary,
                                                  ),
                                            ),
                                          ),
                                        ),
                                ),
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
                      ),
                    ],
                  ),
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
      mainAxisSize: MainAxisSize.min,
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
      mainAxisSize: MainAxisSize.min,
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
      ],
    );
  }

  /// 输入过程中即时校验，错误显示在固定提示区。
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
    if (link.vehicleId != null) {
      parts.add(link.systemId > 0 ? '飞机 ${link.systemId}' : '待识别飞机');
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
