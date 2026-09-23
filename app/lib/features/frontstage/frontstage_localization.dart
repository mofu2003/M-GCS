import '../../l10n/generated/app_localizations.dart';
import 'frontstage_models.dart';

FrontstageText frontstageTextFrom(MgLocalizations l10n) {
  return FrontstageText(
    gps: l10n.statusGps,
    battery: l10n.statusBattery,
    flightMode: l10n.statusFlightMode,
    armingState: l10n.statusArmed,
    flightController: l10n.statusFlightController,
    noData: l10n.noData,
    currentMode: l10n.currentMode,
    armed: l10n.armed,
    disarmed: l10n.disarmed,
    vehicleArmed: l10n.vehicleArmed,
    safe: l10n.safe,
    notConnected: l10n.notConnected,
    clickToView: l10n.clickToView,
    vehicleNotConnected: l10n.vehicleNotConnected,
    notSelected: l10n.notSelected,
    unidentifiedVehicle: l10n.unidentifiedVehicle,
    noMission: l10n.noMission,
    missionEmptyMessage: l10n.missionEmptyMessage,
    disconnected: l10n.disconnected,
    waitingForConnection: l10n.waitingForConnection,
    chooseConnection: l10n.chooseConnection,
    connectedFlightController: l10n.connectedFlightController,
    airframePending: l10n.airframePending,
    firmwarePending: l10n.firmwarePending,
    versionPending: l10n.versionPending,
    linkMeasuring: l10n.linkMeasuring,
    remoteUnavailable: l10n.remoteUnavailable,
    requestingParameters: l10n.requestingParameters,
    gpsNotLocated: l10n.gpsNotLocated,
    serial: l10n.serial,
    udp: l10n.udp,
    tcp: l10n.tcp,
  );
}
