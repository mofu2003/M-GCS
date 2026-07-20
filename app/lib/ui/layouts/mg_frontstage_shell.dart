import 'package:flutter/material.dart';

import '../../features/frontstage/frontstage_models.dart';
import '../../features/map/mg_map_catalog.dart';
import '../skin/mg_skin_scope.dart';

final class MgFrontstageShell extends StatelessWidget {
  const MgFrontstageShell({
    required this.model,
    required this.onConnectPressed,
    required this.onDisconnectPressed,
    required this.onNavigationPressed,
    required this.onConnectionTransportChanged,
    this.onSerialPortChanged,
    this.onBaudRateChanged,
    required this.mapStyle,
    required this.onMapStyleChanged,
    super.key,
  });

  final FrontstageModel model;
  final VoidCallback onConnectPressed;
  final VoidCallback onDisconnectPressed;
  final ValueChanged<FrontstageNavigationItem> onNavigationPressed;
  final ValueChanged<FrontstageConnectionTransport>
  onConnectionTransportChanged;
  final ValueChanged<String>? onSerialPortChanged;
  final ValueChanged<int>? onBaudRateChanged;
  final MgMapStyle mapStyle;
  final ValueChanged<MgMapStyle> onMapStyleChanged;

  @override
  Widget build(BuildContext context) {
    final skin = MgSkinScope.of(context);
    return skin.buildFrontstageShell(
      model: model,
      onConnectPressed: onConnectPressed,
      onDisconnectPressed: onDisconnectPressed,
      onNavigationPressed: onNavigationPressed,
      onConnectionTransportChanged: onConnectionTransportChanged,
      onSerialPortChanged: onSerialPortChanged,
      onBaudRateChanged: onBaudRateChanged,
      mapStyle: mapStyle,
      onMapStyleChanged: onMapStyleChanged,
    );
  }
}
