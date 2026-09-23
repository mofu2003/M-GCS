import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';

import '../../../features/frontstage/frontstage_models.dart';
import '../../../features/map/mg_map_catalog.dart';
import '../../../features/map/mg_map_surface.dart';
import '../../shell/mg_frontstage_shell_contract.dart';
import 'mac_glass_attitude_panel.dart';
import 'mac_glass_connection_drawer.dart';
import 'mac_glass_map_style_switcher.dart';
import 'mac_glass_map_toolbar.dart';
import 'mac_glass_mission_empty_state.dart';
import 'mac_glass_navigation_dock.dart';
import 'mac_glass_output_panel.dart';
import 'mac_glass_status_bar.dart';
import 'mac_glass_tokens.dart';

final class MacGlassFrontstageShell extends StatefulWidget {
  const MacGlassFrontstageShell({
    required this.model,
    required this.actions,
    super.key,
  });

  final MgFrontstageShellModel model;
  final MgFrontstageShellActions actions;

  @override
  State<MacGlassFrontstageShell> createState() =>
      _MacGlassFrontstageShellState();
}

final class _MacGlassFrontstageShellState
    extends State<MacGlassFrontstageShell> {
  late final MapController _mapController;
  late Widget _mapSurface;
  bool _outputPanelOpen = false;

  @override
  void initState() {
    super.initState();
    _mapController = MapController();
    _mapSurface = _buildMapSurface(widget.model.mapStyle);
  }

  @override
  void didUpdateWidget(covariant MacGlassFrontstageShell oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.model.mapStyle != widget.model.mapStyle ||
        oldWidget.model.missionPoints != widget.model.missionPoints) {
      _mapSurface = _buildMapSurface(widget.model.mapStyle);
    }
  }

  @override
  void dispose() {
    _mapController.dispose();
    super.dispose();
  }

  void _zoomBy(double delta) {
    final camera = _mapController.camera;
    _mapController.move(camera.center, camera.zoom + delta);
  }

  void _resetMap() {
    _mapController.moveAndRotate(mgDefaultMapCenter, mgDefaultMapZoom, 0);
  }

  Widget _buildMapSurface(MgMapStyle style) {
    return RepaintBoundary(
      key: const ValueKey('mg-map-repaint-boundary'),
      child: MgMapSurface(
        style: style,
        controller: _mapController,
        missionPoints: widget.model.missionPoints,
        onMapSecondaryTap: widget.actions.onMapSecondaryTap,
        onMissionPointTap: widget.actions.onMissionPointTap,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Stack(
        key: const ValueKey('mg-frontstage-shell'),
        children: [
          Positioned.fill(child: _mapSurface),
          Positioned(
            top: MacGlassTokens.pageInset,
            left: MacGlassTokens.pageInset,
            right: MacGlassTokens.pageInset,
            child: MacGlassStatusBar(
              model: widget.model.frontstage,
              bootstrap: widget.model.frontstage.bootstrap,
              outputPanelOpen: _outputPanelOpen,
              onOutputPressed: () {
                setState(() {
                  _outputPanelOpen = !_outputPanelOpen;
                });
              },
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop,
            left: MacGlassTokens.pageInset,
            bottom: MacGlassTokens.pageInset,
            width: MacGlassTokens.dockWidth,
            child: MacGlassNavigationDock(
              connectionDrawerVisible:
                  widget.model.frontstage.connectionDrawerVisible,
              missionPanelVisible: widget.model.frontstage.missionPanelVisible,
              dashboardVisible: widget.model.frontstage.dashboardVisible,
              onNavigationPressed: widget.actions.onNavigation,
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop,
            right: MacGlassTokens.contentRight,
            width: 360,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-flight-control-transition'),
              visible:
                  widget.model.flightControlPanel != null &&
                  widget.model.frontstage.connection.connected,
              offset: const Offset(0.035, -0.018),
              alignment: Alignment.topRight,
              child: widget.model.flightControlPanel ?? const SizedBox.shrink(),
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop,
            right: MacGlassTokens.pageInset,
            child: MacGlassMapToolbar(
              onZoomIn: () => _zoomBy(1),
              onZoomOut: () => _zoomBy(-1),
              onReset: _resetMap,
              onNorth: () => _mapController.rotate(0),
              onLayers: () {},
            ),
          ),
          Positioned(
            right: MacGlassTokens.pageInset,
            bottom: MacGlassTokens.bottomControlInset,
            child: MacGlassMapStyleSwitcher(
              value: widget.model.mapStyle,
              onChanged: widget.actions.onMapStyleChanged,
            ),
          ),
          Center(
            child: Padding(
              padding: const EdgeInsets.only(left: 84),
              child: MacGlassMissionEmptyState(
                model: widget.model.frontstage.missionEmptyState,
              ),
            ),
          ),
          Positioned(
            left: MacGlassTokens.contentLeft,
            bottom: MacGlassTokens.pageInset,
            width: 450,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-dashboard-transition'),
              visible: widget.model.frontstage.dashboardVisible,
              offset: const Offset(0, 0.045),
              alignment: Alignment.bottomLeft,
              child: MacGlassAttitudePanel(
                model: widget.model.frontstage.attitude,
              ),
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop,
            left: MacGlassTokens.contentLeft,
            bottom: MacGlassTokens.pageInset,
            width: 720,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-mission-panel-transition'),
              visible: widget.model.frontstage.missionPanelVisible,
              offset: const Offset(-0.035, 0.018),
              alignment: Alignment.bottomLeft,
              expand: true,
              child: widget.model.missionPanel ?? const SizedBox.shrink(),
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop,
            left: MacGlassTokens.contentLeft,
            bottom:
                MacGlassTokens.pageInset +
                MacGlassTokens.dashboardHeight +
                MacGlassTokens.overlayGap,
            width: 330,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-connection-transition'),
              visible: widget.model.frontstage.connectionDrawerVisible,
              offset: const Offset(-0.055, 0.018),
              alignment: Alignment.bottomLeft,
              expand: true,
              child: MacGlassConnectionDrawer(
                model: widget.model.frontstage.connection,
                onConnectPressed: widget.actions.onConnect,
                onTcpConnectPressed: widget.actions.onTcpConnect,
                onUdpConnectPressed: widget.actions.onUdpConnect,
                onDisconnectPressed: widget.actions.onDisconnect,
                onClose: () => widget.actions.onNavigation(
                  FrontstageNavigationItem.connection,
                ),
                onTransportChanged: widget.actions.onTransportChanged,
                onSerialPortChanged: widget.actions.onSerialPortChanged,
                onBaudRateChanged: widget.actions.onBaudRateChanged,
                fleetPanel: widget.model.fleetPanel,
              ),
            ),
          ),
          Positioned(
            top: MacGlassTokens.contentTop - MacGlassTokens.overlayGap / 2,
            left: MacGlassTokens.contentLeft,
            right: MacGlassTokens.contentRight,
            child: Align(
              alignment: Alignment.topRight,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 400),
                child: _MacGlassOverlayTransition(
                  key: const ValueKey('mg-flight-info-transition'),
                  visible: _outputPanelOpen,
                  offset: const Offset(0.025, -0.035),
                  alignment: Alignment.topRight,
                  child: MacGlassOutputPanel(
                    messages: widget.model.frontstage.messages,
                    connected: widget.model.frontstage.connection.connected,
                    disconnectMessage:
                        widget.model.frontstage.connection.errorMessage,
                    onClose: () {
                      setState(() {
                        _outputPanelOpen = false;
                      });
                    },
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

final class _MacGlassOverlayTransition extends StatelessWidget {
  const _MacGlassOverlayTransition({
    required this.visible,
    required this.offset,
    required this.alignment,
    required this.child,
    this.expand = false,
    super.key,
  });

  final bool visible;
  final Offset offset;
  final Alignment alignment;
  final Widget child;
  final bool expand;

  @override
  Widget build(BuildContext context) {
    return AnimatedSwitcher(
      duration: const Duration(milliseconds: 220),
      reverseDuration: const Duration(milliseconds: 160),
      switchInCurve: Curves.easeOutCubic,
      switchOutCurve: Curves.easeInCubic,
      layoutBuilder: (currentChild, previousChildren) => Stack(
        alignment: alignment,
        fit: expand ? StackFit.expand : StackFit.loose,
        children: [...previousChildren, ?currentChild],
      ),
      transitionBuilder: (transitionChild, animation) {
        final scale = Tween<double>(begin: 0.985, end: 1).animate(animation);
        final slide = Tween<Offset>(
          begin: offset,
          end: Offset.zero,
        ).animate(animation);
        return FadeTransition(
          opacity: animation,
          child: SlideTransition(
            position: slide,
            child: ScaleTransition(
              alignment: alignment,
              scale: scale,
              child: transitionChild,
            ),
          ),
        );
      },
      child: visible
          ? KeyedSubtree(key: const ValueKey('visible'), child: child)
          : const SizedBox.shrink(key: ValueKey('hidden')),
    );
  }
}
