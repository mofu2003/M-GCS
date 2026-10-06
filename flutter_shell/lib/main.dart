import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'configuration_workspace.dart';
import 'map/amap_map_view.dart';
import 'theme/mg_typography.dart';
import 'widgets/mg_settings_menu.dart';
import 'widgets/mg_top_status_bar.dart';

Future<void> main() async {
  if (!kIsWeb && Platform.isMacOS) {
    WidgetsFlutterBinding.ensureInitialized();
    await WindowManipulator.initialize(enableWindowDelegate: true);
    await WindowManipulator.setMaterial(
      NSVisualEffectViewMaterial.windowBackground,
    );
    await WindowManipulator.disableFullSizeContentView();
    await WindowManipulator.makeTitlebarOpaque();
    await WindowManipulator.showTitle();
  }
  runApp(const MgcsApp());
}

class MgcsApp extends StatelessWidget {
  const MgcsApp({super.key});

  @override
  Widget build(BuildContext context) => MacosApp(
    title: 'M-GCS',
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.system,
    theme: MacosThemeData.light().copyWith(
      typography: MgTypography.macos(Brightness.light),
    ),
    darkTheme: MacosThemeData.dark().copyWith(
      typography: MgTypography.macos(Brightness.dark),
    ),
    home: MapPage(),
  );
}

/// 地面站地图主页面。
class MapPage extends StatefulWidget {
  const MapPage({super.key});

  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  var _showConfiguration = false;
  late final ConnectionController _connectionController =
      ConnectionController();
  final _connectionPanelRequest = ValueNotifier<int>(0);
  final _flightMessagePanelRequest = ValueNotifier<int>(0);
  final _missionPlanningRequest = ValueNotifier<int>(0);

  Future<void> _toggleArm(BuildContext actionContext) async {
    final controller = _connectionController;
    if (!controller.isConnected || controller.vehicleFlying) return;
    final armed = controller.vehicleArmed;
    final confirmed = await showMacosAlertDialog<bool>(
      context: actionContext,
      builder: (dialogContext) => MacosAlertDialog(
        appIcon: Icon(armed ? Icons.lock : Icons.lock_open, size: 48),
        title: Text(armed ? '上锁？' : '解锁？'),
        message: Text(
          armed
              ? '上锁会立即停止电机。飞行中上锁会掉飞机，地面测试请先拆桨。'
              : '解锁后电机可以转动。请确认机体周围无人、已拆桨或已做好安全防护。',
        ),
        primaryButton: PushButton(
          controlSize: ControlSize.large,
          secondary: armed,
          onPressed: () => Navigator.of(dialogContext).pop(true),
          child: Text(armed ? '上锁' : '解锁'),
        ),
        secondaryButton: PushButton(
          controlSize: ControlSize.large,
          secondary: true,
          onPressed: () => Navigator.of(dialogContext).pop(false),
          child: const Text('取消'),
        ),
      ),
    );
    if (confirmed != true || !mounted) return;
    try {
      if (armed) {
        await controller.disarmVehicle();
      } else {
        await controller.armVehicle();
      }
    } on Object catch (error) {
      if (!mounted || !actionContext.mounted) return;
      await showMacosAlertDialog<void>(
        context: actionContext,
        builder: (dialogContext) => MacosAlertDialog(
          appIcon: Icon(Icons.error_outline, size: 48),
          title: Text(armed ? '上锁失败' : '解锁失败'),
          message: Text(error.toString()),
          primaryButton: PushButton(
            controlSize: ControlSize.large,
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text('好'),
          ),
        ),
      );
    }
  }

  Future<void> _openSettings(BuildContext settingsContext) async {
    final connected = _connectionController.isConnected;
    final destination = await showMgSettingsMenu(
      settingsContext,
      connected: connected,
      linkLabel: _connectionController.selectedTransportLabel,
    );
    if (!mounted || destination == null) return;
    switch (destination) {
      case MgSettingsDestination.connection:
        _connectionPanelRequest.value++;
      case MgSettingsDestination.configuration:
        setState(() => _showConfiguration = true);
      case MgSettingsDestination.missionPlanning:
        _missionPlanningRequest.value++;
    }
  }

  Future<void> _openFlightModeMenu(BuildContext context) async {
    if (!_connectionController.isConnected) return;
    await _connectionController.refreshFlightModes();
    if (!mounted) return;
    final options = _connectionController.flightModeOptions;
    if (options.isEmpty) return;
    final modeContext = context;
    if (!modeContext.mounted) return;
    await showMgFlightModeMenu(
      modeContext,
      controller: _connectionController,
      options: options,
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_showConfiguration) {
      return ConfigurationWorkspace(
        connectionController: _connectionController,
        onReturnToMap: () => setState(() => _showConfiguration = false),
      );
    }

    return MacosWindow(
      child: Column(
        children: [
          AnimatedBuilder(
            animation: Listenable.merge([
              _connectionController.connectionChanges,
              _connectionController.telemetryChanges,
              _connectionController.profileChanges,
            ]),
            builder: (context, _) => MgTopStatusBar(
              data: MgTopStatusData.fromController(_connectionController),
              onOpenFlightMessages: () => _flightMessagePanelRequest.value++,
              onToggleArm: () => unawaited(_toggleArm(context)),
              onOpenFlightMode: () => unawaited(_openFlightModeMenu(context)),
              onOpenSettings: () => unawaited(_openSettings(context)),
            ),
          ),
          Expanded(
            child: MacosScaffold(
              children: [
                ContentArea(
                  builder: (context, _) => AMapMapView(
                    connectionController: _connectionController,
                    connectionPanelRequest: _connectionPanelRequest,
                    flightMessagePanelRequest: _flightMessagePanelRequest,
                    missionPlanningRequest: _missionPlanningRequest,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _connectionPanelRequest.dispose();
    _flightMessagePanelRequest.dispose();
    _missionPlanningRequest.dispose();
    _connectionController.dispose();
    super.dispose();
  }
}
