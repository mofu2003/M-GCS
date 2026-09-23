import 'package:flutter/foundation.dart';

import '../../../application/connection_models.dart';

/// Localized copy needed to build a frontstage view model.
///
/// The adapter receives this value instead of reading [BuildContext]. This
/// keeps application state independent from Flutter's localization and makes
/// the view model reusable by another shell or another frontend.
@immutable
final class FrontstageText {
  const FrontstageText({
    required this.gps,
    required this.battery,
    required this.flightMode,
    required this.armingState,
    required this.flightController,
    required this.noData,
    required this.currentMode,
    required this.armed,
    required this.disarmed,
    required this.vehicleArmed,
    required this.safe,
    required this.notConnected,
    required this.clickToView,
    required this.vehicleNotConnected,
    required this.notSelected,
    required this.unidentifiedVehicle,
    required this.noMission,
    required this.missionEmptyMessage,
    required this.disconnected,
    required this.waitingForConnection,
    required this.chooseConnection,
    required this.connectedFlightController,
    required this.airframePending,
    required this.firmwarePending,
    required this.versionPending,
    required this.linkMeasuring,
    required this.remoteUnavailable,
    required this.requestingParameters,
    required this.gpsNotLocated,
    required this.serial,
    required this.udp,
    required this.tcp,
  });

  final String gps;
  final String battery;
  final String flightMode;
  final String armingState;
  final String flightController;
  final String noData;
  final String currentMode;
  final String armed;
  final String disarmed;
  final String vehicleArmed;
  final String safe;
  final String notConnected;
  final String clickToView;
  final String vehicleNotConnected;
  final String notSelected;
  final String unidentifiedVehicle;
  final String noMission;
  final String missionEmptyMessage;
  final String disconnected;
  final String waitingForConnection;
  final String chooseConnection;
  final String connectedFlightController;
  final String airframePending;
  final String firmwarePending;
  final String versionPending;
  final String linkMeasuring;
  final String remoteUnavailable;
  final String requestingParameters;
  final String gpsNotLocated;
  final String serial;
  final String udp;
  final String tcp;

  String messagesCount(int count) => '$count条消息';
  String linkPercent(int percent) => '链路 $percent%';
  String remotePercent(int percent) => '遥控器 $percent%';
  String remoteRssi(int rssi) => '遥控器 RSSI $rssi';
  String parametersProgress(int received, int total) => '$received / $total';
  String satelliteCount(int count) => '$count颗';

  static const zh = FrontstageText(
    gps: 'GPS信息',
    battery: '电池信息',
    flightMode: '飞行模式',
    armingState: '解锁状态',
    flightController: '飞控信息',
    noData: '无数据',
    currentMode: '当前模式',
    armed: '已解锁',
    disarmed: '未解锁',
    vehicleArmed: '飞行器已解锁',
    safe: '安全',
    notConnected: '未连接',
    clickToView: '点击查看',
    vehicleNotConnected: '飞控未连接',
    notSelected: '未选择',
    unidentifiedVehicle: '未识别飞控',
    noMission: '当前无任务',
    missionEmptyMessage: '新建任务、导入任务，或从飞控下载现有任务。',
    disconnected: '飞控连接已断开',
    waitingForConnection: '等待连接',
    chooseConnection: '请选择连接方式',
    connectedFlightController: '已连接飞控',
    airframePending: '机架待确认',
    firmwarePending: '固件待确认',
    versionPending: '版本待确认',
    linkMeasuring: '链路 测量中',
    remoteUnavailable: '遥控器 不可用',
    requestingParameters: '正在请求参数列表',
    gpsNotLocated: '未定位',
    serial: 'Serial',
    udp: 'UDP',
    tcp: 'TCP',
  );
}

enum FrontstageStatusTone { neutral, good, warning, danger }

enum FrontstageConnectionTransport { serial, udp, tcp }

enum FrontstageNavigationItem { connection, mission, dashboard, backstage }

final class FrontstageStatusItem {
  const FrontstageStatusItem({
    required this.id,
    required this.label,
    required this.value,
    this.detail,
    this.tone = FrontstageStatusTone.neutral,
  });

  final String id;
  final String label;
  final String value;
  final String? detail;
  final FrontstageStatusTone tone;
}

final class FrontstageStatusModel {
  const FrontstageStatusModel({required this.items});

  final List<FrontstageStatusItem> items;
}

final class FrontstageConnectionModel {
  const FrontstageConnectionModel({
    required this.connected,
    required this.busy,
    required this.transportLabel,
    required this.portLabel,
    required this.baudRateLabel,
    required this.vehicleLabel,
    this.selectedTransport = FrontstageConnectionTransport.serial,
    this.errorMessage,
    this.serialPorts = const [],
  });

  final bool connected;
  final bool busy;
  final String transportLabel;
  final String portLabel;
  final String baudRateLabel;
  final String vehicleLabel;
  final FrontstageConnectionTransport selectedTransport;
  final String? errorMessage;
  final List<ConnectionSerialPort> serialPorts;

  FrontstageConnectionModel copyWith({
    bool? connected,
    bool? busy,
    String? transportLabel,
    String? portLabel,
    String? baudRateLabel,
    String? vehicleLabel,
    FrontstageConnectionTransport? selectedTransport,
    String? errorMessage,
    List<ConnectionSerialPort>? serialPorts,
  }) {
    return FrontstageConnectionModel(
      connected: connected ?? this.connected,
      busy: busy ?? this.busy,
      transportLabel: transportLabel ?? this.transportLabel,
      portLabel: portLabel ?? this.portLabel,
      baudRateLabel: baudRateLabel ?? this.baudRateLabel,
      vehicleLabel: vehicleLabel ?? this.vehicleLabel,
      selectedTransport: selectedTransport ?? this.selectedTransport,
      errorMessage: errorMessage ?? this.errorMessage,
      serialPorts: serialPorts ?? this.serialPorts,
    );
  }
}

final class FrontstageConnectionBootstrapModel {
  const FrontstageConnectionBootstrapModel({
    this.state = ConnectionBootstrapState.idle,
    this.progressPercent,
    this.parameterLabel = '',
    this.name = '已连接飞控',
    this.airframe = '机架待确认',
    this.firmwareLabel = '固件待确认',
    this.linkLabel = '链路 测量中',
    this.remoteControlLabel = '遥控器 不可用',
    this.errorMessage,
  });

  final ConnectionBootstrapState state;
  final int? progressPercent;
  final String parameterLabel;
  final String name;
  final String airframe;
  final String firmwareLabel;
  final String linkLabel;
  final String remoteControlLabel;
  final String? errorMessage;

  bool get visible => true;
}

final class FrontstageAttitudeModel {
  const FrontstageAttitudeModel({
    required this.pitchDegrees,
    required this.rollDegrees,
    required this.headingDegrees,
    required this.altitudeLabel,
    required this.airspeedLabel,
    required this.groundSpeedLabel,
    required this.verticalSpeedLabel,
    required this.batteryLabel,
  });

  final double pitchDegrees;
  final double rollDegrees;
  final double headingDegrees;
  final String altitudeLabel;
  final String airspeedLabel;
  final String groundSpeedLabel;
  final String verticalSpeedLabel;
  final String batteryLabel;
}

final class FrontstageMissionEmptyStateModel {
  const FrontstageMissionEmptyStateModel({
    required this.visible,
    required this.title,
    required this.message,
  });

  final bool visible;
  final String title;
  final String message;
}

@immutable
final class FrontstageModel {
  const FrontstageModel({
    required this.title,
    required this.subtitle,
    required this.status,
    required this.connection,
    required this.attitude,
    required this.missionEmptyState,
    required this.connectionDrawerVisible,
    required this.missionPanelVisible,
    required this.dashboardVisible,
    this.messages = const [],
    this.bootstrap = const FrontstageConnectionBootstrapModel(),
  });

  final String title;
  final String subtitle;
  final FrontstageStatusModel status;
  final FrontstageConnectionModel connection;
  final FrontstageAttitudeModel attitude;
  final FrontstageMissionEmptyStateModel missionEmptyState;
  final bool connectionDrawerVisible;
  final bool missionPanelVisible;
  final bool dashboardVisible;
  final List<FlightControllerMessage> messages;
  final FrontstageConnectionBootstrapModel bootstrap;

  factory FrontstageModel.demoDisconnected() {
    return const FrontstageModel(
      title: '',
      subtitle: '',
      status: FrontstageStatusModel(
        items: [
          FrontstageStatusItem(
            id: 'gps',
            label: 'GPS信息',
            value: '无数据',
            detail: '0 sats',
            tone: FrontstageStatusTone.warning,
          ),
          FrontstageStatusItem(
            id: 'battery',
            label: '电池信息',
            value: '--',
            detail: '--%',
            tone: FrontstageStatusTone.warning,
          ),
          FrontstageStatusItem(
            id: 'mode',
            label: '飞行模式',
            value: 'UNKNOWN',
            detail: '当前模式',
          ),
          FrontstageStatusItem(
            id: 'armed',
            label: '解锁状态',
            value: '未解锁',
            detail: '安全',
            tone: FrontstageStatusTone.good,
          ),
          FrontstageStatusItem(
            id: 'output',
            label: '飞控信息',
            value: '0条消息',
            detail: '点击查看',
          ),
        ],
      ),
      connection: FrontstageConnectionModel(
        connected: false,
        busy: false,
        transportLabel: 'Serial',
        portLabel: '未选择',
        baudRateLabel: '115200',
        vehicleLabel: '未识别飞控',
      ),
      attitude: FrontstageAttitudeModel(
        pitchDegrees: 0,
        rollDegrees: 0,
        headingDegrees: 0,
        altitudeLabel: '--',
        airspeedLabel: '--',
        groundSpeedLabel: '--',
        verticalSpeedLabel: '--',
        batteryLabel: '--',
      ),
      missionEmptyState: FrontstageMissionEmptyStateModel(
        visible: false,
        title: '当前无任务',
        message: '新建任务、导入任务，或从飞控下载现有任务。',
      ),
      connectionDrawerVisible: false,
      missionPanelVisible: false,
      dashboardVisible: true,
      messages: [],
      bootstrap: FrontstageConnectionBootstrapModel(),
    );
  }

  FrontstageModel copyWith({
    String? title,
    String? subtitle,
    FrontstageStatusModel? status,
    FrontstageConnectionModel? connection,
    FrontstageAttitudeModel? attitude,
    FrontstageMissionEmptyStateModel? missionEmptyState,
    bool? connectionDrawerVisible,
    bool? missionPanelVisible,
    bool? dashboardVisible,
    List<FlightControllerMessage>? messages,
    FrontstageConnectionBootstrapModel? bootstrap,
  }) {
    return FrontstageModel(
      title: title ?? this.title,
      subtitle: subtitle ?? this.subtitle,
      status: status ?? this.status,
      connection: connection ?? this.connection,
      attitude: attitude ?? this.attitude,
      missionEmptyState: missionEmptyState ?? this.missionEmptyState,
      connectionDrawerVisible:
          connectionDrawerVisible ?? this.connectionDrawerVisible,
      missionPanelVisible: missionPanelVisible ?? this.missionPanelVisible,
      dashboardVisible: dashboardVisible ?? this.dashboardVisible,
      messages: messages ?? this.messages,
      bootstrap: bootstrap ?? this.bootstrap,
    );
  }
}
