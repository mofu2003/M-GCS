import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/survey/survey_planning_page.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

void main() {
  testWidgets('原生库不可用时生成给出确定错误，不静默成功', (tester) async {
    tester.view.physicalSize = const Size(1400, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final controller = ConnectionController();
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light().copyWith(
          typography: MgTypography.macos(Brightness.light),
        ),
        child: MaterialApp(
          home: Scaffold(body: SurveyPlanningPage(controller: controller)),
        ),
      ),
    );

    // 没有焦点飞机时中心坐标留空，先提示参数不完整——不用 0,0 冒充坐标。
    await tester.tap(find.text('生成预览'));
    await tester.pump();
    expect(find.textContaining('有效数字'), findsOneWidget);
    expect(find.text('预览'), findsNothing);

    // 填上坐标后走到 backend：flutter_tester 没有原生库，必须报「未加载」
    // 而不是给出一份空的航线预览。
    final fields = find.byType(MacosTextField);
    await tester.enterText(fields.at(0), '31.0');
    await tester.enterText(fields.at(1), '121.0');
    await tester.tap(find.text('生成预览'));
    await tester.pumpAndSettle();

    expect(find.textContaining('原生库未加载'), findsOneWidget);
    expect(find.text('并入任务'), findsNothing);
    expect(tester.takeException(), isNull);
  });

  testWidgets('高度基准可选地形跟随 (AGL)，联动航高标签与提示', (tester) async {
    tester.view.physicalSize = const Size(1400, 1600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final controller = ConnectionController();
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light().copyWith(
          typography: MgTypography.macos(Brightness.light),
        ),
        child: MaterialApp(
          home: Scaffold(body: SurveyPlanningPage(controller: controller)),
        ),
      ),
    );

    // 默认相对 Home：航高标签不带 AGL，也没有地形数据提醒。
    expect(find.text('高度基准'), findsOneWidget);
    expect(find.text('相对高度 (Home)'), findsOneWidget);
    expect(find.text('航高 (m)'), findsOneWidget);
    expect(find.text('航高 (m, AGL)'), findsNothing);

    await tester.tap(find.text('相对高度 (Home)'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('地形跟随 (AGL)').last);
    await tester.pumpAndSettle();

    expect(find.text('航高 (m, AGL)'), findsOneWidget);
    expect(find.text('航高 (m)'), findsNothing);
    expect(find.textContaining('地形跟随由飞控按地形数据执行'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
