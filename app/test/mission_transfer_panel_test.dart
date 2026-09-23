import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/application/mission_models.dart';
import 'package:mgcs/presentation/features/frontstage/mission_transfer_panel.dart';

void main() {
  testWidgets('mission panel reads edits and writes a local mission draft', (
    WidgetTester tester,
  ) async {
    final backend = _MissionBackend();

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(body: MissionTransferPanel(backend: backend)),
      ),
    );

    await tester.tap(find.byKey(const ValueKey('mg-mission-download')));
    await tester.pumpAndSettle();
    expect(find.text('1 个任务项'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('mg-mission-add-waypoint')));
    await tester.pumpAndSettle();
    expect(find.text('2 个任务项'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('mg-mission-upload')));
    await tester.pumpAndSettle();
    expect(find.text('演示任务已写入'), findsOneWidget);
  });
}

final class _MissionBackend implements MissionSessionBackend {
  List<MissionItemSnapshot> _items = const [
    MissionItemSnapshot(
      sequence: 0,
      command: 16,
      commandName: 'NAV_WAYPOINT',
      frame: 3,
      frameName: 'GLOBAL_RELATIVE_ALT',
      autoContinue: true,
      latitude: 31.2304,
      longitude: 121.4737,
      altitudeMeters: 60,
    ),
  ];

  @override
  Future<MissionPlanSnapshot> downloadMission() async =>
      MissionPlanSnapshot(items: _items, valid: _items.isNotEmpty);

  @override
  Future<ConnectionOperationResult> uploadMission(
    List<MissionItemSnapshot> items,
  ) async {
    _items = items;
    return const ConnectionOperationResult(success: true, message: '演示任务已写入');
  }
}
