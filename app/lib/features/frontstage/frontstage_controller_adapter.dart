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
    FrontstageText text = FrontstageText.zh,
    FrontstageConnectionTransport selectedTransport =
        FrontstageConnectionTransport.serial,
  }) {
    final snapshot = controller.snapshot;
    final dashboard = snapshot?.dashboard;
    final connected = controller.status == ConnectionStatus.connected;
    final error = controller.errorMessage;
    final displayError =
        controller.status == ConnectionStatus.idle && error != null
        ? text.disconnected
        : error;

    return FrontstageModel(
      title: '',
      subtitle: '',
      status: FrontstageStatusModel(
        items: [
          FrontstageStatusItem(
            id: 'gps',
            label: text.gps,
            value: dashboard?.satellitesVisible == null
                ? text.noData
                : text.satelliteCount(dashboard!.satellitesVisible!),
            detail: _gpsDetail(dashboard, text),
            tone: (dashboard?.satellitesVisible ?? 0) > 0
                ? FrontstageStatusTone.good
                : FrontstageStatusTone.warning,
          ),
          FrontstageStatusItem(
            id: 'battery',
            label: text.battery,
            value: _formatDouble(dashboard?.batteryVoltage, 'V'),
            detail: _formatPercent(dashboard?.batteryRemainingPercent),
            tone: dashboard?.batteryVoltage == null
                ? FrontstageStatusTone.warning
                : FrontstageStatusTone.good,
          ),
          FrontstageStatusItem(
            id: 'mode',
            label: text.flightMode,
            value: dashboard?.mode ?? 'UNKNOWN',
            detail: text.currentMode,
          ),
          FrontstageStatusItem(
            id: 'armed',
            label: text.armingState,
            value: dashboard?.armed == true ? text.armed : text.disarmed,
            detail: dashboard?.armed == true ? text.vehicleArmed : text.safe,
            tone: dashboard?.armed == true
                ? FrontstageStatusTone.danger
                : FrontstageStatusTone.good,
          ),
          FrontstageStatusItem(
            id: 'output',
            label: text.flightController,
            value: connected
                ? text.messagesCount(controller.messages.length)
                : text.notConnected,
            detail: connected
                ? controller.messages.isEmpty
                      ? text.clickToView
                      : controller.messages.last.text
                : displayError ?? text.vehicleNotConnected,
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
            : _frontstageTransportLabel(selectedTransport, text),
        portLabel: controller.request.port?.isEmpty == false
            ? controller.request.port!
            : text.notSelected,
        baudRateLabel: '${controller.request.baudRate ?? 115200}',
        vehicleLabel: snapshot?.connection.label ?? text.unidentifiedVehicle,
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
      missionEmptyState: FrontstageMissionEmptyStateModel(
        visible: false,
        title: text.noMission,
        message: text.missionEmptyMessage,
      ),
      connectionDrawerVisible: connectionDrawerVisible,
      missionPanelVisible: missionPanelVisible,
      dashboardVisible: dashboardVisible,
      messages: controller.messages,
      bootstrap: _bootstrapModel(controller, text),
    );
  }

  static FrontstageConnectionBootstrapModel _bootstrapModel(
    ConnectionController controller,
    FrontstageText text,
  ) {
    final state = controller.bootstrapState;
    if (state == ConnectionBootstrapState.idle) {
      return FrontstageConnectionBootstrapModel(
        state: ConnectionBootstrapState.idle,
        name: text.notConnected,
        airframe: text.waitingForConnection,
        firmwareLabel: text.chooseConnection,
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
        ? text.remotePercent(remoteControl!.percent!)
        : remoteControl?.rawRssi != null
        ? text.remoteRssi(remoteControl!.rawRssi!)
        : text.remoteUnavailable;

    return FrontstageConnectionBootstrapModel(
      state: state,
      progressPercent: progress?.percent,
      parameterLabel: progress?.hasTotal == true
          ? '${progress!.received} / ${progress.total}'
          : text.requestingParameters,
      name: identity?.name ?? text.connectedFlightController,
      airframe: identity?.airframe ?? text.airframePending,
      firmwareLabel:
          '${identity?.firmwareType ?? text.firmwarePending} ${identity?.versionLabel ?? text.versionPending}',
      linkLabel: link?.measuring == false && link?.percent != null
          ? text.linkPercent(link!.percent!)
          : text.linkMeasuring,
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
    FrontstageText text,
  ) {
    return switch (transport) {
      FrontstageConnectionTransport.serial => text.serial,
      FrontstageConnectionTransport.udp => text.udp,
      FrontstageConnectionTransport.tcp => text.tcp,
    };
  }

  static String _formatDouble(double? value, String unit) {
    if (value == null) {
      return '--';
    }
    return '${value.toStringAsFixed(1)}$unit';
  }

  static String _formatPercent(int? value) => value == null ? '--%' : '$value%';

  static String _gpsDetail(
    FlightDashboardData? dashboard,
    FrontstageText text,
  ) {
    if (dashboard?.latitude == null || dashboard?.longitude == null) {
      return text.gpsNotLocated;
    }
    return '${dashboard!.latitude!.toStringAsFixed(4)}, '
        '${dashboard.longitude!.toStringAsFixed(4)}';
  }
}
