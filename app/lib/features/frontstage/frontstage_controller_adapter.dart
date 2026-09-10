import '../../application/connection_controller.dart';
import '../../application/connection_models.dart';
import 'frontstage_models.dart';

final class FrontstageControllerAdapter {
  const FrontstageControllerAdapter._();

  static FrontstageModel buildModel({
    required ConnectionController controller,
    required bool connectionDrawerVisible,
    required bool missionPanelVisible,
    required bool dashboardVisible,
    FrontstageConnectionTransport selectedTransport =
        FrontstageConnectionTransport.serial,
  }) {
    final snapshot = controller.snapshot;
    final dashboard = snapshot?.dashboard;
    final connected = controller.status == ConnectionStatus.connected;
    final error = controller.errorMessage;
    final displayError =
        controller.status == ConnectionStatus.idle && error != null
        ? '飞控连接已断开'
        : error;

    return FrontstageModel(
      title: '',
      subtitle: '',
      status: FrontstageStatusModel(
        items: [
          FrontstageStatusItem(
            id: 'gps',
            label: 'GPS信息',
            value: _satelliteCount(dashboard?.satellitesVisible),
            detail: _gpsDetail(dashboard),
            tone: (dashboard?.satellitesVisible ?? 0) > 0
                ? FrontstageStatusTone.good
                : FrontstageStatusTone.warning,
          ),
          FrontstageStatusItem(
            id: 'battery',
            label: '电池信息',
            value: _formatDouble(dashboard?.batteryVoltage, 'V'),
            detail: _formatPercent(dashboard?.batteryRemainingPercent),
            tone: dashboard?.batteryVoltage == null
                ? FrontstageStatusTone.warning
                : FrontstageStatusTone.good,
          ),
          FrontstageStatusItem(
            id: 'mode',
            label: '飞行模式',
            value: dashboard?.mode ?? 'UNKNOWN',
            detail: '当前模式',
          ),
          FrontstageStatusItem(
            id: 'armed',
            label: '解锁状态',
            value: dashboard?.armed == true ? '已解锁' : '未解锁',
            detail: dashboard?.armed == true ? '飞行器已解锁' : '安全',
            tone: dashboard?.armed == true
                ? FrontstageStatusTone.danger
                : FrontstageStatusTone.good,
          ),
          FrontstageStatusItem(
            id: 'output',
            label: '飞控信息',
            value: connected ? '${controller.messages.length}条消息' : '未连接',
            detail: connected
                ? controller.messages.isEmpty
                      ? '点击查看'
                      : controller.messages.last.text
                : displayError ?? '飞控未连接',
            tone: connected
                ? FrontstageStatusTone.neutral
                : FrontstageStatusTone.warning,
          ),
        ],
      ),
      connection: FrontstageConnectionModel(
        connected: connected,
        busy: controller.isBusy,
        transportLabel: connected
            ? _transportLabel(controller.request.transport)
            : _frontstageTransportLabel(selectedTransport),
        portLabel: controller.request.port?.isEmpty == false
            ? controller.request.port!
            : '未选择',
        baudRateLabel: '${controller.request.baudRate ?? 115200}',
        vehicleLabel: snapshot?.connection.label ?? '未识别飞控',
        selectedTransport: selectedTransport,
        errorMessage: displayError,
        serialPorts: controller.ports,
      ),
      attitude: FrontstageAttitudeModel(
        pitchDegrees: dashboard?.pitchDegrees ?? 0,
        rollDegrees: dashboard?.rollDegrees ?? 0,
        headingDegrees: dashboard?.headingDegrees ?? 0,
        altitudeLabel: _formatDouble(dashboard?.altitudeMeters, 'm'),
        airspeedLabel: _formatDouble(dashboard?.airspeedMetersPerSecond, 'm/s'),
        groundSpeedLabel: _formatDouble(
          dashboard?.groundSpeedMetersPerSecond,
          'm/s',
        ),
        verticalSpeedLabel: _formatDouble(
          dashboard?.climbMetersPerSecond,
          'm/s',
        ),
        batteryLabel: _formatDouble(dashboard?.batteryVoltage, 'V'),
      ),
      missionEmptyState: const FrontstageMissionEmptyStateModel(
        visible: false,
        title: '当前无任务',
        message: '新建任务、导入任务，或从飞控下载现有任务。',
      ),
      connectionDrawerVisible: connectionDrawerVisible,
      missionPanelVisible: missionPanelVisible,
      dashboardVisible: dashboardVisible,
      messages: controller.messages,
      bootstrap: _bootstrapModel(controller),
    );
  }

  static FrontstageConnectionBootstrapModel _bootstrapModel(
    ConnectionController controller,
  ) {
    final state = controller.bootstrapState;
    if (state == ConnectionBootstrapState.idle) {
      return const FrontstageConnectionBootstrapModel(
        state: ConnectionBootstrapState.idle,
        name: '未连接飞控',
        airframe: '等待连接',
        firmwareLabel: '请选择连接方式',
        linkLabel: '',
        remoteControlLabel: '',
      );
    }
    final bootstrap = controller.snapshot?.bootstrap;
    final progress = bootstrap?.parameterProgress;
    final identity = bootstrap?.identity;
    final link = bootstrap?.linkQuality;
    final remoteControl = bootstrap?.remoteControlSignal;
    final remoteControlLabel = remoteControl?.percent != null
        ? '遥控器 ${remoteControl!.percent}%'
        : remoteControl?.rawRssi != null
        ? '遥控器 RSSI ${remoteControl!.rawRssi}'
        : '遥控器 不可用';

    return FrontstageConnectionBootstrapModel(
      state: state,
      progressPercent: progress?.percent,
      parameterLabel: progress?.hasTotal == true
          ? '${progress!.received} / ${progress.total}'
          : '正在请求参数列表',
      name: identity?.name ?? '已连接飞控',
      airframe: identity?.airframe ?? '机架待确认',
      firmwareLabel:
          '${identity?.firmwareType ?? '固件待确认'} ${identity?.versionLabel ?? '版本待确认'}',
      linkLabel: link?.measuring == false && link?.percent != null
          ? '链路 ${link!.percent}%'
          : '链路 测量中',
      remoteControlLabel: remoteControlLabel,
      errorMessage: controller.bootstrapError,
    );
  }

  static String _transportLabel(ConnectionTransport transport) {
    return switch (transport) {
      ConnectionTransport.serial => 'Serial',
      ConnectionTransport.tcp => 'TCP',
      ConnectionTransport.udp => 'UDP',
    };
  }

  static String _frontstageTransportLabel(
    FrontstageConnectionTransport transport,
  ) {
    return switch (transport) {
      FrontstageConnectionTransport.serial => 'Serial',
      FrontstageConnectionTransport.udp => 'UDP',
      FrontstageConnectionTransport.tcp => 'TCP',
      FrontstageConnectionTransport.bluetooth => '蓝牙',
    };
  }

  static String _formatDouble(double? value, String unit) {
    if (value == null) {
      return '--';
    }
    return '${value.toStringAsFixed(1)}$unit';
  }

  static String _formatPercent(int? value) => value == null ? '--%' : '$value%';

  static String _satelliteCount(int? count) =>
      count == null ? '--颗' : '$count颗';

  static String _gpsDetail(FlightDashboardData? dashboard) {
    if (dashboard?.latitude == null || dashboard?.longitude == null) {
      return '未定位';
    }
    return '${dashboard!.latitude!.toStringAsFixed(4)}, '
        '${dashboard.longitude!.toStringAsFixed(4)}';
  }
}
