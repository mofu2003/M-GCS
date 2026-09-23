import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/presentation/controllers/parameter_controller.dart';
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

  test('recovers from an exception in single-write fallback', () async {
    final backend = _ThrowingParameterBackend();
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refresh();
    controller.stageValue('BATT_MONITOR', 4);
    final result = await controller.writeStaged();

    expect(result.failed['BATT_MONITOR'], contains('transport failed'));
    expect(controller.writing, isFalse);
    expect(controller.canEdit, isTrue);
  });

  test('parameter file import stages only known changed values', () async {
    final backend = _ParameterBackend(
      importedEntries: const [
        ParameterFileEntry(name: 'BATT_MONITOR', value: 4),
        ParameterFileEntry(name: 'BATT_ENABLE', value: 1),
        ParameterFileEntry(name: 'UNKNOWN_PARAM', value: 3),
      ],
    );
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refresh();
    controller.stageValue('BATT_ENABLE', 0);
    final summary = await controller.importParameterFile('ignored by fake');

    expect(summary.parsed, 3);
    expect(summary.staged, 1);
    expect(summary.unchanged, 1);
    expect(summary.unknown, 1);
    expect(controller.stagedChanges.single.parameter.name, 'BATT_MONITOR');
    expect(controller.stagedChanges.single.stagedValue, 4);
    expect(backend.writeCalls, isEmpty);
  });

  test(
    'parameter file save exports current values rather than staged values',
    () async {
      final backend = _ParameterBackend();
      final controller = ParameterController(
        backend: backend,
        isSessionReady: () => true,
      );
      addTearDown(controller.dispose);

      await controller.refresh();
      controller.stageValue('BATT_MONITOR', 4);
      expect(await controller.exportParameterFile(), 'fake.param');

      expect(backend.exportedParameters['BATT_MONITOR'], 0);
      expect(backend.exportedParameters['BATT_ENABLE'], 1);
    },
  );

  test('explicit refresh requests a new vehicle parameter download', () async {
    final backend = _ParameterBackend();
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refreshFromVehicle();

    expect(backend.refreshCalls, 1);
  });

  test('uses the change-set contract when the backend provides it', () async {
    final backend = _ChangeSetParameterBackend();
    final controller = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    addTearDown(controller.dispose);

    await controller.refresh();
    final import = await controller.importParameterFile('ignored by fake');
    expect(import.staged, 1);
    expect(import.unchanged, 1);
    expect(import.unknown, 1);
    expect(controller.stagedChanges.single.parameter.name, 'BATT_MONITOR');

    final result = await controller.writeStaged();
    expect(result.succeeded, ['BATT_MONITOR']);
    expect(backend.commitCalls, 1);
    expect(backend.singleWrites, isEmpty);
  });

  test(
    'discards results that complete after the parameter session changes',
    () async {
      final backend = _DelayedParameterBackend();
      var epoch = 1;
      final controller = ParameterController(
        backend: backend,
        isSessionReady: () => true,
        sessionEpoch: () => epoch,
      );
      addTearDown(controller.dispose);

      final refresh = controller.refresh();
      await backend.loadStarted.future;
      epoch = 2;
      controller.syncSession();
      backend.release.complete(const [
        FlightControllerParameter(
          name: 'STALE_PARAM',
          value: 1,
          mavlinkType: 9,
          index: 0,
          count: 1,
        ),
      ]);
      await refresh;

      expect(controller.parameters, isEmpty);
      expect(controller.stagedChanges, isEmpty);
    },
  );
}

class _ParameterBackend
    implements
        ParameterSessionBackend,
        ParameterFileSessionBackend,
        ParameterRefreshSessionBackend {
  _ParameterBackend({
    this.failNames = const {},
    this.importedEntries = const [],
  });

  final Set<String> failNames;
  final List<ParameterFileEntry> importedEntries;
  final List<String> writeCalls = [];
  final Map<String, double> exportedParameters = {};
  int refreshCalls = 0;

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
  Future<List<FlightControllerParameter>> refreshParameters() async {
    refreshCalls++;
    return loadParameters();
  }

  @override
  Future<List<ParameterFileEntry>> parseParameterFile(String content) async =>
      importedEntries;

  @override
  Future<String> createParameterFile(
    List<FlightControllerParameter> parameters,
  ) async {
    exportedParameters.addEntries(
      parameters.map((parameter) => MapEntry(parameter.name, parameter.value)),
    );
    return 'fake.param';
  }

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

class _DelayedParameterBackend extends _ParameterBackend {
  final loadStarted = Completer<void>();
  final release = Completer<List<FlightControllerParameter>>();

  @override
  Future<List<FlightControllerParameter>> loadParameters() {
    if (!loadStarted.isCompleted) loadStarted.complete();
    return release.future;
  }
}

final class _ThrowingParameterBackend extends _ParameterBackend {
  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    throw StateError('transport failed');
  }
}

final class _ChangeSetParameterBackend extends _ParameterBackend
    implements ParameterChangeSetSessionBackend {
  _ChangeSetParameterBackend()
    : super(
        importedEntries: const [
          ParameterFileEntry(name: 'BATT_MONITOR', value: 4),
          ParameterFileEntry(name: 'BATT_ENABLE', value: 1),
          ParameterFileEntry(name: 'UNKNOWN_PARAM', value: 3),
        ],
      );

  int commitCalls = 0;
  List<String> get singleWrites => writeCalls;

  @override
  Future<ParameterChangeSetPreview> prepareParameterChangeSet(
    List<ParameterFileEntry> entries,
  ) async => const ParameterChangeSetPreview(
    ready: 1,
    unchanged: 1,
    unknown: 1,
    invalid: 0,
    items: [
      ParameterChangeSetItem(
        name: 'BATT_MONITOR',
        value: 4,
        mavlinkType: 9,
        disposition: 'ready',
      ),
      ParameterChangeSetItem(
        name: 'BATT_ENABLE',
        value: 1,
        mavlinkType: 9,
        disposition: 'unchanged',
      ),
      ParameterChangeSetItem(
        name: 'UNKNOWN_PARAM',
        value: 3,
        mavlinkType: 9,
        disposition: 'unknown',
        reason: 'parameter not reported by flight controller',
      ),
    ],
  );

  @override
  Future<ParameterWriteSummary> commitParameterChangeSet(
    List<StagedParameterChange> changes,
  ) async {
    commitCalls++;
    return const ParameterWriteSummary(succeeded: ['BATT_MONITOR'], failed: {});
  }
}
