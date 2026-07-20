import 'package:flutter/foundation.dart';

import '../../application/connection_models.dart';

enum FrontstageStatusTone { neutral, good, warning, danger }

enum FrontstageConnectionTransport { serial, udp, tcp, bluetooth }

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
