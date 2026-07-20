import 'connection_models.dart';
import 'connection_session_backend.dart';

final class UnavailableConnectionSessionBackend
    implements ConnectionSessionBackend, ConnectionBackendAvailability {
  const UnavailableConnectionSessionBackend(this.reason);

  final String reason;

  @override
  String get unavailableReason => reason;

  @override
  Future<List<ConnectionSerialPort>> listSerialPorts() async => const [];

  @override
  Future<ConnectionOperationResult> connect(ConnectionRequest request) async {
    return ConnectionOperationResult(success: false, message: reason);
  }

  @override
  Future<ConnectionOperationResult> disconnect() async {
    return const ConnectionOperationResult(success: true, message: '已断开');
  }

  @override
  Future<ConnectionOperationResult> startParameterSync() async {
    return ConnectionOperationResult(success: false, message: reason);
  }

  @override
  Future<ConnectionSessionSnapshot> loadSessionSnapshot(
    ConnectionRequest request,
  ) async {
    return ConnectionSessionSnapshot(
      connection: AppConnectionSummary(
        status: ConnectionStatus.error,
        label: 'MgCore 不可用',
        detail: reason,
      ),
      dashboard: const FlightDashboardData(
        valid: false,
        mode: 'UNKNOWN',
        armed: false,
      ),
    );
  }

  @override
  Future<FlightDashboardData> loadTelemetrySnapshot() async {
    return const FlightDashboardData(
      valid: false,
      mode: 'UNKNOWN',
      armed: false,
    );
  }

  @override
  Future<ConnectionBootstrapSnapshot> loadBootstrapSnapshot() async {
    return const ConnectionBootstrapSnapshot();
  }

  @override
  void dispose() {}
}
