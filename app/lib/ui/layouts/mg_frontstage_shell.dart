import 'package:flutter/material.dart';

import '../../features/frontstage/frontstage_models.dart';
import '../../features/map/mg_map_catalog.dart';
import '../skin/mg_skin_scope.dart';

final class MgFrontstageShell extends StatelessWidget {
  const MgFrontstageShell({
    required this.model,
    required this.onConnectPressed,
    this.onTcpConnectPressed,
    this.onUdpConnectPressed,
    required this.onDisconnectPressed,
    required this.onNavigationPressed,
    required this.onConnectionTransportChanged,
    this.onSerialPortChanged,
    this.onBaudRateChanged,
    this.missionPanel,
    required this.mapStyle,
    required this.onMapStyleChanged,
    super.key,
  });

  final FrontstageModel model;
  final VoidCallback onConnectPressed;
  final ValueChanged<String>? onTcpConnectPressed;
  final ValueChanged<String>? onUdpConnectPressed;
  final VoidCallback onDisconnectPressed;
  final ValueChanged<FrontstageNavigationItem> onNavigationPressed;
  final ValueChanged<FrontstageConnectionTransport>
  onConnectionTransportChanged;
  final ValueChanged<String>? onSerialPortChanged;
  final ValueChanged<int>? onBaudRateChanged;
  final Widget? missionPanel;
  final MgMapStyle mapStyle;
  final ValueChanged<MgMapStyle> onMapStyleChanged;

  @override
  Widget build(BuildContext context) {
    final skin = MgSkinScope.of(context);
    return skin.buildFrontstageShell(
      model: model,
      onConnectPressed: onConnectPressed,
      onTcpConnectPressed: onTcpConnectPressed,
      onUdpConnectPressed: onUdpConnectPressed,
      onDisconnectPressed: onDisconnectPressed,
      onNavigationPressed: onNavigationPressed,
      onConnectionTransportChanged: onConnectionTransportChanged,
      onSerialPortChanged: onSerialPortChanged,
      onBaudRateChanged: onBaudRateChanged,
      missionPanel: missionPanel,
      mapStyle: mapStyle,
      onMapStyleChanged: onMapStyleChanged,
    );
  }
}
