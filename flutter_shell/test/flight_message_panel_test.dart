import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/flight/flight_message_panel.dart';

void main() {
  testWidgets(
    'message panel displays Chinese while retaining the raw messages',
    (tester) async {
      const raw = [
        'Mission: 1 WP',
        'Reached command #6',
        'Custom diagnostic XYZ-42',
      ];
      final controller = ConnectionController()
        ..flightMessages = [
          for (var i = 0; i < raw.length; i++)
            FlightMessage(
              seq: i + 1,
              time: DateTime(2026, 11, 3, 12, 34, 56),
              severity: 6,
              severityName: 'INFO',
              text: raw[i],
            ),
        ];
      await tester.pumpWidget(
        MacosApp(
          home: Scaffold(
            body: SizedBox(
              width: kFlightMessagePanelWidth,
              height: 500,
              child: FlightMessageFloatingPanel(
                controller: controller,
                onClose: () {},
              ),
            ),
          ),
        ),
      );
      await tester.pump();
      expect(find.text('任务：1 航点'), findsOneWidget);
      expect(find.text('已完成任务项 #6'), findsOneWidget);
      expect(find.text('Custom diagnostic XYZ-42'), findsOneWidget);
      final timeBox = tester.renderObject<RenderBox>(
        find.text('12:34:56').first,
      );
      final messageBox = tester.renderObject<RenderBox>(
        find.byType(EditableText).first,
      );
      double baselineY(RenderBox box) => box
          .localToGlobal(
            Offset(
              0,
              box.getDryBaseline(box.constraints, TextBaseline.alphabetic)!,
            ),
          )
          .dy;
      expect(baselineY(timeBox), closeTo(baselineY(messageBox), 0.1));
      expect(
        find.byWidgetPredicate(
          (widget) =>
              widget is MacosTooltip &&
              widget.message == '原文：Reached command #6',
        ),
        findsOneWidget,
      );
      expect(controller.flightMessages.map((m) => m.text), raw);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox.shrink());
      controller.dispose();
    },
  );
}
