import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/mission/mission_transfer_section.dart';
import 'package:mgcs_frontend/widgets/mg_command_confirmation.dart';

import 'support/mission_backend_fake.dart';

void main() {
  for (final interrupted in [false, true]) {
    for (final overlay in [false, true]) {
      testWidgets(
        'mission action and confirmation agree: interrupted=$interrupted overlay=$overlay',
        (tester) async {
          final mission = MissionDocumentController(
            backend: FakeMissionBackend(),
          )..bindSession(1);
          await mission.initialize();
          await mission.addEntity(
            const MissionEntity(latitude: 31.2, longitude: 121.4),
          );
          mission.dirty = false;
          mission.executionProgress = MissionExecutionProgress(
            valid: true,
            total: 3,
            currentIndex: 1,
            aborted: interrupted,
          );
          final connection = ConnectionController()
            ..sessionId = 1
            ..phase = SerialConnectionPhase.connected;
          MgCommandConfirmation? confirmation;
          await tester.pumpWidget(
            MacosApp(
              home: Scaffold(
                body: SizedBox(
                  width: 420,
                  child: MissionTransferSection(
                    mission: mission,
                    controller: connection,
                    onRequestCommandConfirmation: overlay
                        ? (value) => confirmation = value
                        : null,
                  ),
                ),
              ),
            ),
          );
          await tester.pump();
          final label = interrupted ? '继续任务' : '开始任务';
          expect(find.text(label), findsOneWidget);
          await tester.tap(find.text(label));
          await tester.pumpAndSettle();
          if (overlay) {
            expect(confirmation?.label, label);
            expect(find.byType(MacosAlertDialog), findsNothing);
          } else {
            final dialog = tester.widget<MacosAlertDialog>(
              find.byType(MacosAlertDialog),
            );
            expect((dialog.title as Text).data, label);
            expect(
                (dialog.primaryButton.child as Text).data,
              label,
            );
            if (interrupted) {
              expect((dialog.message as Text).data, contains('继续执行'));
            }
            await tester.tap(find.text('取消'));
            await tester.pumpAndSettle();
          }
          expect(tester.takeException(), isNull);
          await tester.pumpWidget(const SizedBox.shrink());
          connection.dispose();
          mission.dispose();
        },
      );
    }
  }
}
