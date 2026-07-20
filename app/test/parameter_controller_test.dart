import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/application/parameter_controller.dart';
import 'package:mgcs/application/parameter_models.dart';

void main() {
  test('stages changes and writes enable parameters last', () async {
    final backend = _ParameterBackend();
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refresh();
    controller.stageValue('BATT_ENABLE', 0);
    controller.stageValue('BATT_MONITOR', 4);
    final result = await controller.writeStaged();

    expect(result.failed, isEmpty);
    expect(backend.writeCalls, ['BATT_MONITOR', 'BATT_ENABLE']);
    expect(controller.stagedChanges, isEmpty);
  });

  test('keeps only failed staged values after a batch write', () async {
    final backend = _ParameterBackend(failNames: {'BATT_ENABLE'});
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refresh();
    controller.stageValue('BATT_MONITOR', 4);
    controller.stageValue('BATT_ENABLE', 0);
    final result = await controller.writeStaged();

    expect(result.succeeded, ['BATT_MONITOR']);
    expect(result.failed, {'BATT_ENABLE': 'write failed'});
    expect(controller.stagedChanges.single.parameter.name, 'BATT_ENABLE');
    expect(controller.stagedChanges.single.errorMessage, 'write failed');
  });
}

final class _ParameterBackend implements ParameterSessionBackend {
  _ParameterBackend({this.failNames = const {}});

  final Set<String> failNames;
  final List<String> writeCalls = [];

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => const [
    FlightControllerParameter(
      name: 'BATT_MONITOR',
      value: 0,
      mavlinkType: 9,
      index: 0,
      count: 2,
    ),
    FlightControllerParameter(
      name: 'BATT_ENABLE',
      value: 1,
      mavlinkType: 9,
      index: 1,
      count: 2,
    ),
  ];

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async {
    return ParameterDescriptor(
      name: name,
      displayName: name,
      description: '',
      group: '',
      groupDisplayName: '',
      type: '',
      found: false,
    );
  }

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    writeCalls.add(parameter.name);
    if (failNames.contains(parameter.name)) {
      return const ConnectionOperationResult(
        success: false,
        message: 'write failed',
      );
    }
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      const [];
}
