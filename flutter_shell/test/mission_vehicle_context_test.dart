import 'dart:ffi';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_ffi_backend.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

void main() {
  final libraryPath = Platform.environment['MGCS_FLEET_LIBRARY'];

  for (final lateContext in [false, true]) {
    test(
      'native VTOL draft uses vehicle context ${lateContext ? 'after' : 'before'} initialization',
      () async {
        final library = DynamicLibrary.open(libraryPath!);
        final create = library
            .lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>(
              'mg_create',
            );
        final destroy = library
            .lookupFunction<
              Void Function(Pointer<Void>),
              void Function(Pointer<Void>)
            >('mg_destroy');
        final core = create();
        addTearDown(() => destroy(core));
        final backend = MissionDocumentFfiBackend(
          library,
          core,
          isConnected: () => false,
        );
        final draft = await backend.createMissionDocument({
          'metadata': {'name': 'VTOL draft', 'vehicle_type': 'copter'},
          'home': {'set': true, 'lat': 32.546, 'lon': 114.736, 'alt_m': 30},
          'entities': [
            const MissionEntity(
              id: 'vtol-takeoff',
              type: 'vtol_takeoff',
              command: 84,
              latitude: 32.546,
              longitude: 114.736,
              altitudeMeters: 50,
            ).toJson(),
          ],
        });
        expect(
          (await backend.validateMissionDocument()).diagnostics.any(
            (d) => d.message.contains('not supported on copter'),
          ),
          isTrue,
        );
        final controller = MissionDocumentController(backend: backend)
          ..bindSession(4);
        addTearDown(controller.dispose);
        if (!lateContext) {
          controller.setVehicleContext(
            vehicleClass: 'vtol',
            firmwareProfile: 'ardupilot',
          );
        }
        await controller.initialize();
        if (lateContext) {
          controller.setVehicleContext(
            vehicleClass: 'vtol',
            firmwareProfile: 'ardupilot',
          );
          await controller.validate();
        }
        expect(
          controller.validation!.diagnostics.where(
            (d) => d.message.contains('not supported on copter'),
          ),
          isEmpty,
        );
        expect(controller.document!.metadata.vehicleType, 'vtol');
        expect(controller.validation!.valid, isTrue);
        expect(controller.document!.metadata.firmwareProfile, 'ardupilot');
        expect(controller.document!.metadata.name, 'VTOL draft');
        expect(controller.document!.home.toJson(), draft.home.toJson());
        expect(
          controller.document!.entities.single.toJson(),
          draft.entities.single.toJson(),
        );
        final revision = controller.document!.revision;
        await controller.validate();
        expect(controller.document!.revision, revision);

        controller.setVehicleContext(
          vehicleClass: 'copter',
          firmwareProfile: 'ardupilot',
        );
        await controller.validate();
        expect(
          controller.validation!.diagnostics.any(
            (d) => d.message.contains('not supported on copter'),
          ),
          isTrue,
        );
      },
      skip: libraryPath == null
          ? 'Set MGCS_FLEET_LIBRARY for native validation'
          : false,
    );
  }
}
