import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';

import '../../../features/frontstage/frontstage_models.dart';
import '../../../features/map/mg_map_catalog.dart';
import '../../../features/map/mg_map_surface.dart';
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
    required this.onConnectPressed,
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
    _mapSurface = _buildMapSurface(widget.mapStyle);
  }

  @override
  void didUpdateWidget(covariant MacGlassFrontstageShell oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.mapStyle != widget.mapStyle) {
      _mapSurface = _buildMapSurface(widget.mapStyle);
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
      child: MgMapSurface(style: style, controller: _mapController),
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
              model: widget.model,
              bootstrap: widget.model.bootstrap,
              outputPanelOpen: _outputPanelOpen,
              onOutputPressed: () {
                setState(() {
                  _outputPanelOpen = !_outputPanelOpen;
                });
              },
            ),
          ),
          Positioned(
            top: 102,
            left: MacGlassTokens.pageInset,
            bottom: MacGlassTokens.pageInset,
            width: MacGlassTokens.dockWidth,
            child: MacGlassNavigationDock(
              connectionDrawerVisible: widget.model.connectionDrawerVisible,
              missionPanelVisible: widget.model.missionPanelVisible,
              dashboardVisible: widget.model.dashboardVisible,
              onNavigationPressed: widget.onNavigationPressed,
            ),
          ),
          Positioned(
            top: 102,
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
            bottom: 50,
            child: MacGlassMapStyleSwitcher(
              value: widget.mapStyle,
              onChanged: widget.onMapStyleChanged,
            ),
          ),
          Center(
            child: Padding(
              padding: const EdgeInsets.only(left: 84),
              child: MacGlassMissionEmptyState(
                model: widget.model.missionEmptyState,
              ),
            ),
          ),
          Positioned(
            left: 106,
            bottom: MacGlassTokens.pageInset,
            width: 450,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-dashboard-transition'),
              visible: widget.model.dashboardVisible,
              offset: const Offset(0, 0.045),
              alignment: Alignment.bottomLeft,
              child: MacGlassAttitudePanel(model: widget.model.attitude),
            ),
          ),
          Positioned(
            top: 102,
            left: 106,
            bottom: MacGlassTokens.pageInset,
            width: 720,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-mission-panel-transition'),
              visible: widget.model.missionPanelVisible,
              offset: const Offset(-0.035, 0.018),
              alignment: Alignment.bottomLeft,
              expand: true,
              child: widget.missionPanel ?? const SizedBox.shrink(),
            ),
          ),
          Positioned(
            top: 102,
            left: 106,
            bottom: MacGlassTokens.pageInset + 216 + 12,
            width: 330,
            child: _MacGlassOverlayTransition(
              key: const ValueKey('mg-connection-transition'),
              visible: widget.model.connectionDrawerVisible,
              offset: const Offset(-0.055, 0.018),
              alignment: Alignment.bottomLeft,
              expand: true,
              child: MacGlassConnectionDrawer(
                model: widget.model.connection,
                onConnectPressed: widget.onConnectPressed,
                onDisconnectPressed: widget.onDisconnectPressed,
                onClose: () => widget.onNavigationPressed(
                  FrontstageNavigationItem.connection,
                ),
                onTransportChanged: widget.onConnectionTransportChanged,
                onSerialPortChanged: widget.onSerialPortChanged,
                onBaudRateChanged: widget.onBaudRateChanged,
              ),
            ),
          ),
          Positioned(
            top: 96,
            left: 106,
            right: 82,
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
                    messages: widget.model.messages,
                    connected: widget.model.connection.connected,
                    disconnectMessage: widget.model.connection.errorMessage,
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
