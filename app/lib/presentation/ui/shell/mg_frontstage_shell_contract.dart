import 'package:flutter/widgets.dart';
import 'package:latlong2/latlong.dart';

import '../../features/frontstage/frontstage_models.dart';
import '../../features/map/mg_map_catalog.dart';
import '../../features/map/mg_map_surface.dart';

/// Stable data boundary between the frontstage feature and a visual shell.
///
/// A new shell can consume this model without knowing how the controller is
/// implemented, and the existing feature page does not need to change when a
/// different visual skin is introduced.
@immutable
final class MgFrontstageShellModel {
  const MgFrontstageShellModel({
    required this.frontstage,
    required this.mapStyle,
    this.missionPanel,
    this.flightControlPanel,
    this.fleetPanel,
    this.missionPoints = const [],
  });

  final FrontstageModel frontstage;
  final Widget? missionPanel;
  final Widget? flightControlPanel;
  final Widget? fleetPanel;
  final List<MgMapMissionPoint> missionPoints;
  final MgMapStyle mapStyle;
}

@immutable
final class MgFrontstageShellActions {
  const MgFrontstageShellActions({
    required this.onConnect,
    required this.onDisconnect,
    required this.onNavigation,
    required this.onTransportChanged,
    required this.onMapStyleChanged,
    this.onTcpConnect,
    this.onUdpConnect,
    this.onSerialPortChanged,
    this.onBaudRateChanged,
    this.onMapSecondaryTap,
    this.onMissionPointTap,
  });

  final VoidCallback onConnect;
  final VoidCallback onDisconnect;
  final ValueChanged<FrontstageNavigationItem> onNavigation;
  final ValueChanged<FrontstageConnectionTransport> onTransportChanged;
  final ValueChanged<MgMapStyle> onMapStyleChanged;
  final ValueChanged<String>? onTcpConnect;
  final ValueChanged<String>? onUdpConnect;
  final ValueChanged<String>? onSerialPortChanged;
  final ValueChanged<int>? onBaudRateChanged;
  final ValueChanged<LatLng>? onMapSecondaryTap;
  final ValueChanged<int>? onMissionPointTap;
}
