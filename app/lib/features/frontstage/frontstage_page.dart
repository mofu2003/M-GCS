import 'dart:async';

import 'package:flutter/material.dart';
import 'package:latlong2/latlong.dart';

import '../../l10n/generated/app_localizations.dart';
import '../../application/connection_controller.dart';
import '../../application/connection_models.dart';
import '../../features/map/mg_map_catalog.dart';
import '../../features/map/mg_map_surface.dart';
import '../map/mg_coordinate_transform.dart';
import '../mission/mission_editor_controller.dart';
import '../mission/mission_waypoint_dialog.dart';
import '../../ui/skin/mg_skin.dart';
import '../../ui/skin/mg_skin_scope.dart';
import '../../ui/shell/mg_frontstage_shell_contract.dart';
import '../../ui/skins/mac_glass/mac_glass_skin.dart';
import 'frontstage_controller_adapter.dart';
import 'frontstage_localization.dart';
import 'frontstage_models.dart';
import 'mission_transfer_panel.dart';
import 'flight_control_panel.dart';
import '../../application/flight_control_controller.dart';
import '../../application/fleet_controller.dart';
import 'fleet_vehicle_panel.dart';

final class FrontstagePage extends StatefulWidget {
  const FrontstagePage({
    required this.controller,
    required this.onOpenBackstage,
    this.skin = const MacGlassSkin(),
    super.key,
  });

  final ConnectionController controller;
  final VoidCallback onOpenBackstage;
  final MgSkin skin;

  @override
  State<FrontstagePage> createState() => _FrontstagePageState();
}

final class _FrontstagePageState extends State<FrontstagePage> {
  late final ConnectionController _controller;
  late final MissionEditorController _missionController;
  late final FlightControlController _flightControlController;
  late final FleetController _fleetController;
  Timer? _serialPortRefreshTimer;
  bool _connectionDrawerVisible = false;
  bool _missionPanelVisible = false;
  bool _dashboardVisible = true;
  FrontstageConnectionTransport _selectedTransport =
      FrontstageConnectionTransport.serial;
  MgMapStyle _mapStyle = MgMapStyle.standard;
  List<MgMapMissionPoint> _missionPoints = const [];
  int? _flightControlInitializedEpoch;
  int? _fleetInitializedEpoch;

  @override
  void initState() {
    super.initState();
    _controller = widget.controller;
    _missionController = MissionEditorController(
      backend: _controller.capabilities.mission,
    )..addListener(_syncMissionPointsFromController);
    _syncMissionPointsFromController();
    _flightControlController = FlightControlController(
      backend: _controller.capabilities.flightControl,
    )..initialize();
    _fleetController = FleetController(
      fleet: _controller.capabilities.fleet,
      flightControl: _controller.capabilities.flightControl,
    );
    _controller.addListener(_closeConnectionDrawerWhenSyncStarts);
    _serialPortRefreshTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      _controller.refreshSerialPorts();
    });
  }

  void _closeConnectionDrawerWhenSyncStarts() {
    if (_controller.status == ConnectionStatus.connected &&
        _flightControlInitializedEpoch != _controller.sessionEpoch) {
      _flightControlInitializedEpoch = _controller.sessionEpoch;
      unawaited(_flightControlController.initialize());
    }
    if (_controller.status == ConnectionStatus.connected &&
        _fleetInitializedEpoch != _controller.sessionEpoch) {
      _fleetInitializedEpoch = _controller.sessionEpoch;
      unawaited(_fleetController.initialize());
    }
    if (!mounted ||
        !_connectionDrawerVisible ||
        _controller.bootstrapState != ConnectionBootstrapState.downloading) {
      return;
    }
    setState(() {
      _connectionDrawerVisible = false;
    });
  }

  void _connectNetwork(
    FrontstageConnectionTransport transport,
    String endpoint,
  ) {
    final appTransport = switch (transport) {
      FrontstageConnectionTransport.udp => ConnectionTransport.udp,
      FrontstageConnectionTransport.tcp => ConnectionTransport.tcp,
      FrontstageConnectionTransport.serial => null,
    };
    if (appTransport == null) {
      return;
    }
    _controller.setNetworkEndpoint(appTransport, endpoint);
    _controller.connect();
  }

  @override
  void dispose() {
    _serialPortRefreshTimer?.cancel();
    _controller.removeListener(_closeConnectionDrawerWhenSyncStarts);
    _missionController
      ..removeListener(_syncMissionPointsFromController)
      ..dispose();
    _flightControlController.dispose();
    _fleetController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MgSkinScope(
      skin: widget.skin,
      child: ListenableBuilder(
        listenable: _controller,
        builder: (context, _) {
          final localizedText = frontstageTextFrom(MgLocalizations.of(context));
          final model = FrontstageControllerAdapter.buildModel(
            controller: _controller,
            connectionDrawerVisible: _connectionDrawerVisible,
            missionPanelVisible: _missionPanelVisible,
            dashboardVisible: _dashboardVisible,
            selectedTransport: _selectedTransport,
            text: localizedText,
          );
          final missionPanel = MissionTransferPanel(
            backend: _controller.capabilities.mission,
            controller: _missionController,
          );
          return widget.skin.buildFrontstageShell(
            model: MgFrontstageShellModel(
              frontstage: model,
              missionPanel: missionPanel,
              flightControlPanel: FlightControlPanel(
                controller: _flightControlController,
              ),
              fleetPanel: FleetVehiclePanel(controller: _fleetController),
              missionPoints: _missionPoints,
              mapStyle: _mapStyle,
            ),
            actions: MgFrontstageShellActions(
              onConnect: _controller.connect,
              onTcpConnect: (endpoint) {
                _connectNetwork(FrontstageConnectionTransport.tcp, endpoint);
              },
              onUdpConnect: (endpoint) {
                _connectNetwork(FrontstageConnectionTransport.udp, endpoint);
              },
              onDisconnect: _controller.disconnect,
              onNavigation: (item) {
                setState(() {
                  switch (item) {
                    case FrontstageNavigationItem.connection:
                      _connectionDrawerVisible = !_connectionDrawerVisible;
                      if (_connectionDrawerVisible) {
                        unawaited(_controller.refreshSerialPorts());
                        unawaited(_fleetController.refresh());
                      }
                    case FrontstageNavigationItem.mission:
                      _missionPanelVisible = !_missionPanelVisible;
                      if (_missionPanelVisible) {
                        _connectionDrawerVisible = false;
                      }
                    case FrontstageNavigationItem.dashboard:
                      _dashboardVisible = !_dashboardVisible;
                    case FrontstageNavigationItem.backstage:
                      widget.onOpenBackstage();
                  }
                });
              },
              onTransportChanged: (transport) {
                setState(() {
                  _selectedTransport = transport;
                });
              },
              onSerialPortChanged: _controller.setSerialPort,
              onBaudRateChanged: _controller.setBaudRate,
              onMapSecondaryTap: _handleMapSecondaryTap,
              onMissionPointTap: _handleMissionPointTap,
              onMapStyleChanged: (style) {
                setState(() {
                  _mapStyle = style;
                });
              },
            ),
          );
        },
      ),
    );
  }

  void _syncMissionPointsFromController() {
    if (!mounted) return;
    final points = <MgMapMissionPoint>[];
    for (final item in _missionController.items) {
      final latitude = item.latitude;
      final longitude = item.longitude;
      if (latitude == null || longitude == null) continue;
      points.add(
        MgMapMissionPoint(
          latitude: latitude,
          longitude: longitude,
          sequence: item.sequence,
          current: item.sequence == _missionController.currentIndex,
        ),
      );
    }
    setState(() => _missionPoints = List.unmodifiable(points));
  }

  Future<void> _handleMapSecondaryTap(LatLng mapPoint) async {
    final source = MgMapCatalog.sourceFor(_mapStyle);
    final domainPoint = mgCoordinateFromMap(mapPoint, source.coordinateSystem);
    final draft = await showMissionWaypointDialog(
      context,
      latitude: domainPoint.latitude,
      longitude: domainPoint.longitude,
    );
    if (!mounted || draft == null) return;
    _missionController.addWaypointAt(
      latitude: draft.latitude,
      longitude: draft.longitude,
      altitude: draft.altitude,
      speedMetersPerSecond: draft.speedMetersPerSecond,
    );
  }

  Future<void> _handleMissionPointTap(int sequence) async {
    if (sequence < 0 || sequence >= _missionController.items.length) return;
    final item = _missionController.items[sequence];
    final latitude = item.latitude;
    final longitude = item.longitude;
    if (latitude == null || longitude == null) return;
    final draft = await showMissionWaypointDialog(
      context,
      latitude: latitude,
      longitude: longitude,
      altitude: item.altitudeMeters ?? 50,
      speedMetersPerSecond: item.speedMetersPerSecond,
      editing: true,
    );
    if (!mounted || draft == null) return;
    _missionController.updateWaypoint(
      sequence,
      latitude: draft.latitude,
      longitude: draft.longitude,
      altitude: draft.altitude,
      speedMetersPerSecond: draft.speedMetersPerSecond,
    );
  }
}
