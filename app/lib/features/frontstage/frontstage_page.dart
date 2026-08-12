import 'dart:async';

import 'package:flutter/material.dart';

import '../../application/connection_controller.dart';
import '../../application/connection_models.dart';
import '../../application/connection_session_backend.dart';
import '../../features/map/mg_map_catalog.dart';
import '../../ui/skin/mg_skin_factory.dart';
import '../../ui/skin/mg_skin_scope.dart';
import 'frontstage_controller_adapter.dart';
import 'frontstage_models.dart';
import 'mission_transfer_panel.dart';

final class FrontstagePage extends StatefulWidget {
  const FrontstagePage({
    required this.controller,
    required this.onOpenBackstage,
    super.key,
  });

  final ConnectionController controller;
  final VoidCallback onOpenBackstage;

  @override
  State<FrontstagePage> createState() => _FrontstagePageState();
}

final class _FrontstagePageState extends State<FrontstagePage> {
  late final ConnectionController _controller;
  Timer? _serialPortRefreshTimer;
  bool _connectionDrawerVisible = false;
  bool _missionPanelVisible = false;
  bool _dashboardVisible = true;
  FrontstageConnectionTransport _selectedTransport =
      FrontstageConnectionTransport.serial;
  MgMapStyle _mapStyle = MgMapStyle.standard;

  @override
  void initState() {
    super.initState();
    _controller = widget.controller;
    _controller.addListener(_closeConnectionDrawerWhenSyncStarts);
    _serialPortRefreshTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      _controller.refreshSerialPorts();
    });
  }

  void _closeConnectionDrawerWhenSyncStarts() {
    if (!mounted ||
        !_connectionDrawerVisible ||
        _controller.bootstrapState != ConnectionBootstrapState.downloading) {
      return;
    }
    setState(() {
      _connectionDrawerVisible = false;
    });
  }

  @override
  void dispose() {
    _serialPortRefreshTimer?.cancel();
    _controller.removeListener(_closeConnectionDrawerWhenSyncStarts);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final skin = createDefaultSkin();
    return MgSkinScope(
      skin: skin,
      child: ListenableBuilder(
        listenable: _controller,
        builder: (context, _) {
          final model = FrontstageControllerAdapter.buildModel(
            controller: _controller,
            connectionDrawerVisible: _connectionDrawerVisible,
            missionPanelVisible: _missionPanelVisible,
            dashboardVisible: _dashboardVisible,
            selectedTransport: _selectedTransport,
          );
          final missionPanel = MissionTransferPanel(
            backend: _controller.backend is MissionSessionBackend
                ? _controller.backend as MissionSessionBackend
                : null,
          );
          return skin.buildFrontstageShell(
            model: model,
            onConnectPressed: () {
              _controller.connect();
            },
            onDisconnectPressed: () {
              _controller.disconnect();
            },
            onNavigationPressed: (item) {
              setState(() {
                switch (item) {
                  case FrontstageNavigationItem.connection:
                    _connectionDrawerVisible = !_connectionDrawerVisible;
                    if (_connectionDrawerVisible) {
                      unawaited(_controller.refreshSerialPorts());
                    }
                  case FrontstageNavigationItem.mission:
                    _missionPanelVisible = !_missionPanelVisible;
                    if (_missionPanelVisible) _connectionDrawerVisible = false;
                  case FrontstageNavigationItem.dashboard:
                    _dashboardVisible = !_dashboardVisible;
                  case FrontstageNavigationItem.backstage:
                    widget.onOpenBackstage();
                }
              });
            },
            onConnectionTransportChanged: (transport) {
              setState(() {
                _selectedTransport = transport;
              });
            },
            onSerialPortChanged: _controller.setSerialPort,
            onBaudRateChanged: _controller.setBaudRate,
            missionPanel: missionPanel,
            mapStyle: _mapStyle,
            onMapStyleChanged: (style) {
              setState(() {
                _mapStyle = style;
              });
            },
          );
        },
      ),
    );
  }
}
