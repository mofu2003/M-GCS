import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'configuration/configuration_workspace.dart';
import 'fleet/fleet_models.dart';
import 'gcs_configuration_workspace.dart';
import 'map/amap_map_view.dart';
import 'logging/flight_data_page.dart';
import 'theme/mg_typography.dart';
import 'theme/mg_material_theme.dart';
import 'widgets/mg_command_confirmation.dart';
import 'widgets/mg_dialog.dart';
import 'widgets/mg_settings_menu.dart';
import 'widgets/mg_top_status_bar.dart';
import 'widgets/parameter_load_progress.dart';

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
    builder: (context, child) =>
        MgMaterialTheme(child: child ?? const SizedBox.shrink()),
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
  /// 地图样式的唯一真源：设置页写，地图页读并持久化。
  final _mapStyle = ValueNotifier<MapStyle>(MapStyle.amap);

  /// 自定义瓦片源地址（KD-37）。地图页读完偏好后会把持久化的值写回来。
  final _customTileUrl = ValueNotifier<String>('');
  late final ConnectionController _connectionController =
      ConnectionController();
  final _connectionPanelRequest = ValueNotifier<int>(0);
  final _flightMessagePanelRequest = ValueNotifier<int>(0);
  final _missionPlanningRequest = ValueNotifier<int>(0);
  final _geofenceRequest = ValueNotifier<int>(0);
  final _fleetPanelRequest = ValueNotifier<int>(0);

  /// 设置菜单直接跳到配置工作区的某个页面（如「机载日志」）。
  final _configurationPageRequest = ValueNotifier<String?>(null);
  final _pendingCommand = ValueNotifier<MgCommandConfirmation?>(null);

  FleetTarget? _pendingTarget;
  int? _confirmationFocusRevision;

  @override
  void initState() {
    super.initState();
    if (_connectionController.initializationError == null) {
      _confirmationFocusRevision = _connectionController.fleet.focusRevision;
      _connectionController.fleet.addListener(_handleFleetFocusChange);
    }
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) _connectionController.preloadConfigurationResources();
    });
  }

  void _handleFleetFocusChange() {
    final revision = _connectionController.fleet.focusRevision;
    if (_confirmationFocusRevision == revision) return;
    _confirmationFocusRevision = revision;
    _cancelCommandConfirmation();
  }

  void _requestCommandConfirmation(MgCommandConfirmation command) {
    try {
      _pendingTarget = _connectionController.fleet.captureTarget(
        expectedVehicleId: _connectionController.sessionId,
      );
      _pendingCommand.value = command;
    } on Object catch (error) {
      _cancelCommandConfirmation();
      // Reuse the standard failure presentation; never execute an unbound command.
      unawaited(
        _executeCommandConfirmation(
          MgCommandConfirmation(
            label: command.label,
            execute: () async => throw error,
          ),
        ),
      );
    }
  }

  void _cancelCommandConfirmation() {
    _pendingTarget = null;
    _pendingCommand.value = null;
  }

  void _confirmCommandConfirmation() {
    final command = _pendingCommand.value;
    if (command == null) return;
    final target = _pendingTarget;
    _cancelCommandConfirmation();
    final execute = target == null
        ? command.execute
        : () =>
              _connectionController.fleet.runForTarget(target, command.execute);
    unawaited(
      _executeCommandConfirmation(
        MgCommandConfirmation(label: command.label, execute: execute),
      ),
    );
  }

  void _requestFleetBatchAction(FleetBatchTarget target) {
    _cancelCommandConfirmation();
    _pendingCommand.value = MgCommandConfirmation(
      label: target.action.label,
      detail: target.detail,
      execute: () async {
        final result = await _connectionController.fleet.runBatch(target);
        if (!result.allSucceeded && mounted) {
          await _showFleetBatchResult(result);
        }
      },
    );
  }

  Future<void> _showFleetBatchResult(FleetBatchResult result) {
    final failed = result.results.where((item) => !item.success).toList();
    return showMgAlertDialog<void>(
      context: context,
      builder: (dialogContext) => MacosAlertDialog(
        appIcon: const Icon(Icons.warning_amber_rounded, size: 48),
        title: Text('${result.action.label}结果'),
        message: SizedBox(
          width: 360,
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  '已接受 ${result.successCount} 架，失败 ${result.failureCount} 架，'
                  '未派发 ${result.notSentCount} 架',
                ),
                const SizedBox(height: 8),
                for (final item in failed) ...[
                  Text(
                    '飞机 ${item.systemId} · ${item.status.label} · ${item.message}',
                  ),
                  const SizedBox(height: 4),
                ],
              ],
            ),
          ),
        ),
        primaryButton: PushButton(
          controlSize: ControlSize.large,
          onPressed: () => Navigator.of(dialogContext).pop(),
          child: const Text('知道了'),
        ),
      ),
    );
  }

  Future<void> _executeCommandConfirmation(
    MgCommandConfirmation command,
  ) async {
    try {
      await command.execute();
    } on Object catch (error) {
      if (!mounted) return;
      await showMgAlertDialog<void>(
        context: context,
        builder: (dialogContext) => MacosAlertDialog(
          appIcon: const Icon(Icons.error_outline, size: 48),
          title: Text('${command.label}失败'),
          message: Text(error.toString().replaceFirst('Bad state: ', '')),
          primaryButton: PushButton(
            controlSize: ControlSize.large,
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text('知道了'),
          ),
        ),
      );
    }
  }

  /// 解锁/上锁只进入悬浮长按确认，成功不弹窗；失败由统一命令错误处理弹窗。
  void _requestArmConfirmation() {
    final controller = _connectionController;
    // S5：动作名（解锁/上锁）取决于当前 armed，过期状态不能用来决定动作。
    if (!controller.isConnected || !controller.vehicleStateFresh) return;
    if (controller.vehicleFlying) return;
    final armed = controller.vehicleArmed;
    _requestCommandConfirmation(
      MgCommandConfirmation(
        label: armed ? '上锁' : '解锁',
        execute: () => _executeArm(armed),
      ),
    );
  }

  /// 急停不走 [_requestArmConfirmation]：常规上锁在飞行中被禁用，急停必须仍可触达。
  void _requestEmergencyStopConfirmation() {
    final controller = _connectionController;
    // 急停只在确认为“已解锁”时出现；过期帧可能停在解锁前的状态（S5）。
    // 命令入口对新鲜度还有一道校验，这里只是不给出基于过期状态的入口。
    if (!controller.isConnected ||
        !controller.vehicleStateFresh ||
        !controller.vehicleArmed) {
      return;
    }
    _requestCommandConfirmation(
      MgCommandConfirmation(
        label: '急停',
        detail: '跳过解锁前检查并强制上锁，立即停止电机。飞行中执行会导致坠机。',
        execute: controller.emergencyDisarmVehicle,
      ),
    );
  }

  Future<void> _executeArm(bool armed) async {
    final changed = armed
        ? await _connectionController.disarmVehicle()
        : await _connectionController.armVehicle();
    // 飞控已经处于目标状态时没有下发任何命令；这不是错误，但必须让操作者
    // 知道“什么都没发生”，不能按了长按确认却毫无反馈（S1）。
    if (!changed && mounted) {
      await _showAlreadyInState(armed ? '上锁' : '解锁');
    }
  }

  Future<void> _showAlreadyInState(String action) {
    return showMgAlertDialog<void>(
      context: context,
      builder: (dialogContext) => MacosAlertDialog(
        appIcon: const Icon(Icons.info_outline, size: 48),
        title: Text('无需$action'),
        message: Text(
          action == '解锁' ? '飞控当前已经处于解锁状态，未重复下发命令。' : '飞控当前已经处于上锁状态，未重复下发命令。',
        ),
        primaryButton: PushButton(
          controlSize: ControlSize.large,
          onPressed: () => Navigator.of(dialogContext).pop(),
          child: const Text('知道了'),
        ),
      ),
    );
  }

  Future<void> _openSettings(BuildContext settingsContext) async {
    final connected = _connectionController.isConnected;
    final destination = await showMgSettingsMenu(
      settingsContext,
      topInset: mgTopStatusBarHeight(
        MediaQuery.sizeOf(settingsContext).width,
        multipleVehicles:
            _connectionController.fleetSnapshot.vehicles.length > 1,
      ),
      connected: connected,
      linkLabel: _connectionController.selectedTransportLabel,
    );
    if (!mounted || destination == null) return;
    switch (destination) {
      case MgSettingsDestination.connection:
        _cancelCommandConfirmation();
        _connectionPanelRequest.value++;
      case MgSettingsDestination.configuration:
        _cancelCommandConfirmation();
        return _openConfiguration();
      case MgSettingsDestination.missionPlanning:
        _cancelCommandConfirmation();
        _missionPlanningRequest.value++;
      case MgSettingsDestination.geofence:
        _cancelCommandConfirmation();
        _geofenceRequest.value++;
      case MgSettingsDestination.flightData:
        _cancelCommandConfirmation();
        return _openFlightData(FlightDataView.onboardLogs);
      case MgSettingsDestination.gcs:
        _cancelCommandConfirmation();
        return _openGcsConfiguration();
    }
  }

  Route<void> _workspaceRoute(WidgetBuilder builder) => PageRouteBuilder<void>(
    transitionDuration: const Duration(milliseconds: 120),
    reverseTransitionDuration: const Duration(milliseconds: 100),
    pageBuilder: (context, animation, secondaryAnimation) => builder(context),
    transitionsBuilder: (context, animation, secondaryAnimation, child) =>
        FadeTransition(
          opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
          child: child,
        ),
  );

  Future<void> _openFlightData(FlightDataView view) =>
      Navigator.of(context).push<void>(
        _workspaceRoute(
          (routeContext) => FlightDataPage(
            view: view,
            controller: _connectionController,
            onReturnToFlight: () => Navigator.of(routeContext).maybePop(),
          ),
        ),
      );

  Future<void> _openConfiguration({String? page}) async {
    _configurationPageRequest.value = page;
    await Navigator.of(context).push(
      _workspaceRoute(
        (routeContext) => ConfigurationWorkspace(
          connectionController: _connectionController,
          requestedPage: _configurationPageRequest,
          onReturnToMap: () => Navigator.of(routeContext).maybePop(),
        ),
      ),
    );
  }

  Future<void> _openGcsConfiguration() async {
    await Navigator.of(context).push(
      _workspaceRoute(
        (routeContext) => GcsConfigurationWorkspace(
          mapStyle: _mapStyle,
          customTileUrl: _customTileUrl,
          connectionController: _connectionController,
          onReturnToMap: () => Navigator.of(routeContext).maybePop(),
        ),
      ),
    );
  }

  void _selectFleetVehicle(int vehicleId) {
    try {
      _connectionController.selectVehicle(vehicleId);
    } on Object catch (error) {
      unawaited(
        showMgAlertDialog<void>(
          context: context,
          builder: (dialogContext) => MacosAlertDialog(
            appIcon: const Icon(Icons.error_outline, size: 48),
            title: const Text('无法切换飞机'),
            message: Text(error.toString().replaceFirst('Bad state: ', '')),
            primaryButton: PushButton(
              controlSize: ControlSize.large,
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('知道了'),
            ),
          ),
        ),
      );
    }
  }

  Future<void> _openFleetFlightMode(int vehicleId) =>
      _executeCommandConfirmation(
        MgCommandConfirmation(
          label: '打开飞行模式',
          execute: () => _openFlightModeMenu(context, vehicleId: vehicleId),
        ),
      );

  Future<void> _openFlightModeMenu(
    BuildContext context, {
    int? vehicleId,
  }) async {
    if (vehicleId != null) _connectionController.selectVehicle(vehicleId);
    if (!_connectionController.isConnected) {
      if (vehicleId != null) throw StateError('这架飞机尚未连接');
      return;
    }
    final target = vehicleId == null
        ? null
        : _connectionController.fleet.captureTarget(
            expectedVehicleId: vehicleId,
          );
    await _connectionController.refreshFlightModes();
    if (!mounted) return;
    // 列表属于打开时的飞机。异步加载期间焦点或会话变化后不能沿用。
    if (target != null) _connectionController.fleet.validateTarget(target);
    final options = _connectionController.flightModeOptions;
    if (options.isEmpty) {
      if (vehicleId != null) throw StateError('尚未获取到这架飞机的飞行模式');
      return;
    }
    final modeContext = context;
    if (!modeContext.mounted) return;
    await showMgFlightModeMenu(
      modeContext,
      controller: _connectionController,
      options: options,
      vehicleLabel: vehicleId == null
          ? null
          : (_connectionController.fleet.snapshot
                    .vehicle(vehicleId)
                    ?.displayName ??
                '待识别飞机'),
      alignment: vehicleId == null ? Alignment.topLeft : Alignment.topRight,
    );
  }

  @override
  Widget build(BuildContext context) {
    return MacosWindow(
      child: Column(
        children: [
          Stack(
            children: [
              AnimatedBuilder(
                animation: Listenable.merge([
                  _connectionController.connectionChanges,
                  _connectionController.gpsChanges,
                  _connectionController.batteryChanges,
                  _connectionController.flightStateChanges,
                  _connectionController.stateChanges,
                  _connectionController.profileChanges,
                ]),
                builder: (context, _) => MgTopStatusBar(
                  data: MgTopStatusData.fromController(_connectionController),
                  vehicles: _connectionController.fleetSnapshot.vehicles,
                  activeVehicleId:
                      _connectionController.fleetSnapshot.activeVehicleId,
                  selectedVehicleCount:
                      _connectionController.fleet.selectedVehicleIds.length,
                  selectedVehicleIds:
                      _connectionController.fleet.selectedVehicleIds,
                  onSelectVehicle: _selectFleetVehicle,
                  onOpenFleetBatch: () => _fleetPanelRequest.value++,
                  onOpenFlightMessages: () =>
                      _flightMessagePanelRequest.value++,
                  onToggleArm: _requestArmConfirmation,
                  onEmergencyStop: _requestEmergencyStopConfirmation,
                  onOpenFlightMode: () =>
                      unawaited(_openFlightModeMenu(context)),
                  onOpenSettings: () => unawaited(_openSettings(context)),
                ),
              ),
              Positioned(
                left: 0,
                right: 0,
                bottom: 0,
                height: 3,
                child: ToolbarParameterProgress(
                  controller: _connectionController,
                ),
              ),
            ],
          ),
          Expanded(
            child: ValueListenableBuilder<MgCommandConfirmation?>(
              valueListenable: _pendingCommand,
              builder: (context, pendingCommand, _) => Stack(
                children: [
                  MacosScaffold(
                    children: [
                      ContentArea(
                        builder: (context, _) => AMapMapView(
                          connectionController: _connectionController,
                          mapStyle: _mapStyle,
                          customTileUrl: _customTileUrl,
                          connectionPanelRequest: _connectionPanelRequest,
                          flightMessagePanelRequest: _flightMessagePanelRequest,
                          missionPlanningRequest: _missionPlanningRequest,
                          geofenceRequest: _geofenceRequest,
                          fleetPanelRequest: _fleetPanelRequest,
                          onSelectFleetVehicle: (vehicleId) async =>
                              _selectFleetVehicle(vehicleId),
                          onRequestFleetBatchAction: _requestFleetBatchAction,
                          onOpenFleetFlightMode: (vehicleId) =>
                              unawaited(_openFleetFlightMode(vehicleId)),
                          onRequestCommandConfirmation:
                              _requestCommandConfirmation,
                        ),
                      ),
                    ],
                  ),
                  if (pendingCommand != null)
                    MgCommandConfirmationOverlay(
                      command: pendingCommand,
                      onConfirmed: _confirmCommandConfirmation,
                      onCancelled: _cancelCommandConfirmation,
                    ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    if (_connectionController.initializationError == null) {
      _connectionController.fleet.removeListener(_handleFleetFocusChange);
    }
    _mapStyle.dispose();
    _customTileUrl.dispose();
    _connectionPanelRequest.dispose();
    _flightMessagePanelRequest.dispose();
    _missionPlanningRequest.dispose();
    _geofenceRequest.dispose();
    _fleetPanelRequest.dispose();
    _configurationPageRequest.dispose();
    _pendingCommand.dispose();
    _connectionController.dispose();
    super.dispose();
  }
}
