// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class MgLocalizationsEn extends MgLocalizations {
  MgLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'M-GCS';

  @override
  String get navConnection => 'Connection';

  @override
  String get navMission => 'Mission';

  @override
  String get navDashboard => 'Dashboard';

  @override
  String get navBackstage => 'Backstage';

  @override
  String get returnFrontstage => 'Return to frontstage';

  @override
  String get statusGps => 'GPS';

  @override
  String get statusBattery => 'Battery';

  @override
  String get statusFlightMode => 'Flight Mode';

  @override
  String get statusArmed => 'Arming State';

  @override
  String get statusFlightController => 'Flight Controller';

  @override
  String get noData => 'No data';

  @override
  String get currentMode => 'Current mode';

  @override
  String get armed => 'Armed';

  @override
  String get disarmed => 'Disarmed';

  @override
  String get vehicleArmed => 'Vehicle armed';

  @override
  String get safe => 'Safe';

  @override
  String messagesCount(int count) {
    return '$count messages';
  }

  @override
  String get notConnected => 'Not connected';

  @override
  String get clickToView => 'Click to view';

  @override
  String get vehicleNotConnected => 'Vehicle not connected';

  @override
  String get notSelected => 'Not selected';

  @override
  String get unidentifiedVehicle => 'Vehicle not identified';

  @override
  String get noMission => 'No mission';

  @override
  String get missionEmptyMessage =>
      'Create a mission, import one, or download an existing mission from the vehicle.';

  @override
  String get disconnected => 'Flight controller disconnected';

  @override
  String get waitingForConnection => 'Waiting for connection';

  @override
  String get chooseConnection => 'Choose a connection method';

  @override
  String get connectedFlightController => 'Flight controller connected';

  @override
  String get airframePending => 'Airframe pending';

  @override
  String get firmwarePending => 'Firmware pending';

  @override
  String get versionPending => 'Version pending';

  @override
  String get linkMeasuring => 'Link measuring';

  @override
  String linkPercent(int percent) {
    return 'Link $percent%';
  }

  @override
  String get remoteUnavailable => 'Remote control unavailable';

  @override
  String remotePercent(int percent) {
    return 'Remote control $percent%';
  }

  @override
  String remoteRssi(int rssi) {
    return 'Remote control RSSI $rssi';
  }

  @override
  String get requestingParameters => 'Requesting parameter list';

  @override
  String parametersProgress(int received, int total) {
    return '$received / $total';
  }

  @override
  String get gpsNotLocated => 'Not located';

  @override
  String satelliteCount(int count) {
    return '$count satellites';
  }

  @override
  String get connect => 'Connect';

  @override
  String get disconnect => 'Disconnect';

  @override
  String get cancelConnection => 'Cancel connection';

  @override
  String get serial => 'Serial';

  @override
  String get udp => 'UDP';

  @override
  String get tcp => 'TCP';

  @override
  String get serialPort => 'Serial connection';

  @override
  String get port => 'Port';

  @override
  String get baudRate => 'Baud rate';

  @override
  String get listenMode => 'Listen mode';

  @override
  String get remoteAddress => 'Remote address';

  @override
  String get close => 'Close';

  @override
  String get closeConnectionPanel => 'Close connection panel';

  @override
  String get selectSerialPort => 'Select a serial port';

  @override
  String get udpListen => 'UDP listener';

  @override
  String get tcpConnection => 'TCP connection';

  @override
  String get mission => 'Waypoint mission';

  @override
  String get addWaypoint => 'Add waypoint';

  @override
  String get editWaypoint => 'Edit waypoint';

  @override
  String get read => 'Read';

  @override
  String get write => 'Write';

  @override
  String get clear => 'Clear';

  @override
  String get startMission => 'Start mission';

  @override
  String get newMission => 'New mission';

  @override
  String get importMission => 'Import mission';

  @override
  String get downloadFromVehicle => 'Download from vehicle';

  @override
  String get backstage => 'Backstage';

  @override
  String get parameters => 'Parameters';

  @override
  String get airframe => 'Airframe';

  @override
  String get motorOutputs => 'Motor outputs';

  @override
  String get portConfiguration => 'Port configuration';

  @override
  String get remoteControl => 'Remote control setup';

  @override
  String get pidTuning => 'PID tuning';

  @override
  String get calibration => 'Sensor / peripheral setup';

  @override
  String get safetySettings => 'Safety settings';

  @override
  String get flightLogs => 'Flight logs';

  @override
  String parametersDescription(int count) {
    return '$count flight controller parameters';
  }

  @override
  String get airframeDescription =>
      'Configure Copter or Plane/QuadPlane airframes by firmware type';

  @override
  String get motorOutputsDescription =>
      'SERVO1-16 · motor functions, reversing, and PWM range';

  @override
  String portConfigurationDescription(int count) {
    return '$count available serial ports · changes are written immediately';
  }

  @override
  String get remoteControlDescription =>
      'Flight modes, channel functions, and stick calibration';

  @override
  String get pidTuningDescription =>
      'Show controllers for the current airframe · changes are written immediately';

  @override
  String get calibrationDescription => 'IMU, compass, GPS, and airspeed';

  @override
  String get safetySettingsDescription =>
      'Fence, arming checks, and battery protection';

  @override
  String get flightLogsDescription =>
      'Browse files on the flight controller storage and download logs or other files';

  @override
  String parameterWriteFailures(int count) {
    return '$count parameter writes failed; drafts were kept';
  }

  @override
  String parameterFileReadSummary(
    int parsed,
    int staged,
    int unchanged,
    int unknown,
  ) {
    return 'Read $parsed: $staged pending, $unchanged unchanged, $unknown unmatched';
  }

  @override
  String parameterFileReadFailed(String error) {
    return 'Failed to read parameter file: $error';
  }

  @override
  String parametersSaved(int count) {
    return 'Saved $count parameters';
  }

  @override
  String parameterFileSaveFailed(String error) {
    return 'Failed to save parameter file: $error';
  }

  @override
  String get unknown => 'Unknown';

  @override
  String get notLocated => 'Not located';
}
