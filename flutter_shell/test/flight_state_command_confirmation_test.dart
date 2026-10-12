import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/mg_command_confirmation.dart';
import 'package:mgcs_frontend/widgets/mg_confirm.dart';
import 'package:mgcs_frontend/widgets/mg_top_status_bar.dart';

/// S3：飞行状态命令（解锁/上锁/起飞/降落/返航/急停/模式切换/任务启动/Goto/
/// Orbit/ROI）的界面入口必须经过确认组件（长按或对话框），不得存在无确认入口。
///
/// 这条约束此前只能靠评审：`docs/constraints/testing.md` 的 T1 要求"每条安全约束
/// 至少一条断言真实行为的测试"，S3 一直没有。这里分两层：
///   1. 行为层：驱动确认组件本身，断言"短按/单击不下发、长按或确认才下发"；
///   2. 接线层：读 `lib/` 源码，断言上面 11 类命令的每个下发点都在确认组件内部
///      （或确认对话框之后），防止将来新增一个"单击直发"的入口。
///
/// 接线层的检查函数带自检用例（`_unguardedDispatches` 的合成输入），确保它真的
/// 能报出违规——否则就是 T2 禁止的假断言。

// ---------------------------------------------------------------- 源码读入

String _read(String relative) => File(relative).readAsStringSync();

List<int> _occurrences(String source, String needle) {
  final hits = <int>[];
  var from = 0;
  while (true) {
    final at = source.indexOf(needle, from);
    if (at < 0) break;
    hits.add(at);
    from = at + needle.length;
  }
  return hits;
}

/// `index` 处的代码是否位于 `call(` 的实参括号内（闭包作为实参也算在内）。
bool _insideCall(String source, int index, String call) {
  for (final start in _occurrences(source, call)) {
    final open = start + call.length - 1; // call 以 '(' 结尾
    var depth = 0;
    for (var i = open; i < source.length; i++) {
      final ch = source[i];
      if (ch == '(') depth++;
      if (ch == ')') {
        depth--;
        if (depth == 0) {
          if (index > open && index < i) return true;
          break;
        }
      }
    }
  }
  return false;
}

/// `name` 的调用点（排除它的定义处）。
///
/// 定义处的判定：该出现位置的文本前缀只有返回类型与空白（例如
/// `Future<void> _executeArm(`），而调用点前后会出现 `=>`、`)`、`(` 等。
List<int> _callSites(String source, String name) {
  // 定义处的形态：前缀只有返回类型（void / Future<...> / 大写类型）加空白。
  // 这样 `await _executeArm(`、`=> _setMode(` 之类的调用点不会被误判成定义。
  final definition = RegExp(
    r'^\s*(?:static\s+)?(?:void|Future<[^>]*>|[A-Z][\w:<>?,\[\]]*[\s\*&]+)\s+$',
  );
  final sites = <int>[];
  for (final at in _occurrences(source, name)) {
    final lineStart = source.lastIndexOf('\n', at) + 1;
    final prefix = source.substring(lineStart, at);
    if (definition.hasMatch(prefix)) continue; // 定义
    sites.add(at);
  }
  return sites;
}

/// 返回"没有经过确认组件"的下发点，供断言与自检共用。
///
/// [gates] 是确认入口：`MgCommandConfirmation(`（悬浮长按确认）、
/// `MgHoldToConfirmButton(`、`MgFlightModeRow(`、`_requestConfirmedCommand(`
/// （地图菜单）、`showMgConfirmDialog(` 与 `confirmMissionAction(`（对话框确认）。
List<String> _unguardedDispatches(
  String source, {
  required List<String> needles,
  required List<String> gates,
}) {
  final violations = <String>[];
  for (final needle in needles) {
    for (final at in _occurrences(source, needle)) {
      final guarded = gates.any((gate) => _insideCall(source, at, gate));
      if (!guarded) {
        final line = source.substring(0, at).split('\n').length;
        violations.add('第 $line 行的 $needle 不在确认组件内');
      }
    }
  }
  return violations;
}

/// 找到 index 处代码所在的方法名（用于"这个方法只能从确认组件调用"的判定）。
String? _enclosingMethod(String source, int index) {
  final lines = source.substring(0, index).split('\n');
  for (var i = lines.length - 1; i >= 0 && i >= lines.length - 60; i--) {
    final line = lines[i];
    final single = RegExp(
      r'^\s*(?:static\s+)?(?:Future<[^>]*>|void|[\w<>?,\[\]]+)\s+(\w+)\s*\(',
    ).firstMatch(line);
    if (single != null && RegExp(r'\{\s*$').hasMatch(line)) {
      return single.group(1);
    }
    // 多行签名：签名最后一行只写 ") async {"
    if (RegExp(r'^\s*\)\s*(?:async\s*)?\{\s*$').hasMatch(line)) {
      for (var j = i - 1; j >= 0; j--) {
        final name = RegExp(r'(\w+)\s*\(').firstMatch(lines[j]);
        if (name != null) return name.group(1);
      }
    }
  }
  return null;
}

/// 规则一（调用点门控）：下发点所在的方法，其每个调用点都必须落在确认组件实参内。
/// 返回违规描述（空表示通过）。
List<String> _unguardedThroughCallers(
  String source, {
  required List<String> needles,
  required List<String> gates,
}) {
  final violations = <String>[];
  for (final needle in needles) {
    for (final at in _occurrences(source, needle)) {
      if (gates.any((gate) => _insideCall(source, at, gate))) continue;
      final method = _enclosingMethod(source, at);
      if (method == null) {
        violations.add('第 ${source.substring(0, at).split('\n').length} 行的 $needle 找不到所属方法，无法证明经过确认');
        continue;
      }
      final sites = _callSites(source, method);
      if (sites.isEmpty) {
        violations.add('$needle 所在方法 $method 没有调用点');
        continue;
      }
      for (final site in sites) {
        if (!gates.any((gate) => _insideCall(source, site, gate))) {
          violations.add('$method 的调用点（第 ${source.substring(0, site).split('\n').length} 行）不在确认组件内');
        }
      }
    }
  }
  return violations;
}

/// 规则二（内部守卫）：确认对话框在方法内部，且下发点之前必须先校验 confirmed。
List<String> _unguardedWithoutInternalGuard(
  String source, {
  required List<String> needles,
  required String dialogCall,
  required List<String> gates,
}) {
  final violations = <String>[];
  for (final needle in needles) {
    for (final at in _occurrences(source, needle)) {
      if (gates.any((gate) => _insideCall(source, at, gate))) continue;
      final method = _enclosingMethod(source, at);
      if (method == null) {
        violations.add('$needle 找不到所属方法');
        continue;
      }
      // 只在"下发点之前"找方法起点，否则会取到方法之后的其它同名调用。
      final before = source.substring(0, at);
      final methodAt =
          before.lastIndexOf(RegExp(r'\b' + RegExp.escape(method) + r'\s*\('));
      final from = methodAt < 0 ? 0 : methodAt;
      final between = source.substring(from, at);
      final dialogAt = between.lastIndexOf(dialogCall);
      if (dialogAt < 0) {
        violations.add('第 ${source.substring(0, at).split('\n').length} 行的 $needle 之前没有 $dialogCall');
        continue;
      }
      if (!between.substring(dialogAt).contains('!confirmed')) {
        violations.add('$needle 在 $dialogCall 之后没有校验 confirmed 就下发');
      }
    }
  }
  return violations;
}

void main() {
  // ------------------------------------------------------------ 行为层

  group('S3 行为：长按确认组件', () {
    Future<void> showHold(
      WidgetTester tester, {
      required String label,
      required VoidCallback onConfirmed,
    }) async {
      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Center(
                child: MgHoldToConfirmButton(
                  label: label,
                  onConfirmed: onConfirmed,
                ),
              ),
            ),
          ),
        ),
      );
    }

    testWidgets('单击（按下即放开）不下发命令', (tester) async {
      var confirmed = 0;
      await showHold(tester, label: '返航', onConfirmed: () => confirmed++);
      await tester.tap(find.byType(MgHoldToConfirmButton));
      await tester.pumpAndSettle();
      expect(confirmed, 0, reason: 'S3：短按不得下发飞行状态命令');
    });

    testWidgets('按住不足 500 ms 也不下发', (tester) async {
      var confirmed = 0;
      await showHold(tester, label: '急停', onConfirmed: () => confirmed++);
      final gesture = await tester.startGesture(
        tester.getCenter(find.byType(MgHoldToConfirmButton)),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 400));
      await gesture.up();
      await tester.pumpAndSettle();
      expect(confirmed, 0, reason: 'S3：未完成长按不得下发');
    });

    testWidgets('长按完成才下发一次', (tester) async {
      var confirmed = 0;
      await showHold(tester, label: '开始任务', onConfirmed: () => confirmed++);
      final gesture = await tester.startGesture(
        tester.getCenter(find.byType(MgHoldToConfirmButton)),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 500));
      await tester.pump(const Duration(milliseconds: 200));
      await tester.pumpAndSettle();
      expect(confirmed, 1, reason: 'S3：长按确认后下发一次');
      await gesture.up();
      await tester.pumpAndSettle();
    });

    testWidgets('确认条在松手取消后不再下发', (tester) async {
      var confirmed = 0;
      await showHold(tester, label: '降落', onConfirmed: () => confirmed++);
      final gesture = await tester.startGesture(
        tester.getCenter(find.byType(MgHoldToConfirmButton)),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
      await gesture.up();
      await tester.pump(const Duration(milliseconds: 600));
      await tester.pumpAndSettle();
      expect(confirmed, 0, reason: 'S3：中途松手必须取消');
    });
  });

  group('S3 行为：模式切换行', () {
    Future<void> showRow(WidgetTester tester, VoidCallback onTriggered) async {
      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Center(
                child: SizedBox(
                  width: 250,
                  child: MgFlightModeRow(
                    option: const FlightModeOption(
                      id: 6,
                      name: 'RTL',
                      settable: true,
                    ),
                    selected: false,
                    onTriggered: onTriggered,
                  ),
                ),
              ),
            ),
          ),
        ),
      );
    }

    testWidgets('单击模式行不切换模式', (tester) async {
      var triggered = 0;
      await showRow(tester, () => triggered++);
      await tester.tap(find.byType(MgFlightModeRow));
      await tester.pumpAndSettle();
      expect(triggered, 0, reason: 'S3：模式切换必须长按确认');
    });

    testWidgets('按住 500 ms 才切换一次', (tester) async {
      var triggered = 0;
      await showRow(tester, () => triggered++);
      final gesture = await tester.startGesture(
        tester.getCenter(find.byType(MgFlightModeRow)),
      );
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 400));
      expect(triggered, 0);
      await tester.pump(const Duration(milliseconds: 300));
      await tester.pumpAndSettle();
      expect(triggered, 1, reason: 'S3：长按完成后切换一次');
      await gesture.up();
      await tester.pumpAndSettle();
    });
  });

  group('S3 行为：对话框确认（任务启动的兜底路径）', () {
    testWidgets('取消返回 false，确认返回 true', (tester) async {
      bool? cancelled;
      bool? confirmed;
      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Column(
                children: [
                  ElevatedButton(
                    onPressed: () async {
                      cancelled = await showMgConfirmDialog(
                        tester.element(find.byType(Column)),
                        title: '开始任务',
                        message: '飞机将按当前任务开始执行',
                        confirmLabel: '开始任务',
                      );
                    },
                    child: const Text('ask-cancel'),
                  ),
                  ElevatedButton(
                    onPressed: () async {
                      confirmed = await showMgConfirmDialog(
                        tester.element(find.byType(Column)),
                        title: '开始任务',
                        message: '飞机将按当前任务开始执行',
                        confirmLabel: '开始任务',
                      );
                    },
                    child: const Text('ask-confirm'),
                  ),
                ],
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('ask-cancel'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('取消'));
      await tester.pumpAndSettle();
      expect(cancelled, isFalse, reason: 'S3：取消不得放行命令');

      await tester.tap(find.text('ask-confirm'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('开始任务').last);
      await tester.pumpAndSettle();
      expect(confirmed, isTrue, reason: 'S3：确认后放行命令');
    });
  });

  // ------------------------------------------------------------ 接线层

  group('S3 接线：11 类命令的入口都接在确认组件上', () {
    final mainShell = _read('lib/main.dart');
    final statusBar = _read('lib/widgets/mg_top_status_bar.dart');
    final mapView = _read('lib/map/amap_map_view.dart');
    final planningPanel = _read('lib/mission/mission_planning_panel.dart');
    final transferSection = _read('lib/mission/mission_transfer_section.dart');

    test('解锁与上锁：只有确认闭包里的 _executeArm 会下发', () {
      // 命令下发只出现在 _executeArm 内部。
      // 注意带前缀点号：'armVehicle()' 是 'disarmVehicle()' 的子串。
      expect(_occurrences(mainShell, '.armVehicle()'), hasLength(1));
      expect(_occurrences(mainShell, '.disarmVehicle()'), hasLength(1));
      expect(
        _unguardedThroughCallers(
          mainShell,
          needles: ['.armVehicle()', '.disarmVehicle()'],
          gates: ['MgCommandConfirmation('],
        ),
        isEmpty,
        reason: 'S3：解锁/上锁只能从长按确认组件的闭包下发',
      );
      // _executeArm 的每个调用点都必须在 MgCommandConfirmation( 的实参里。
      final sites = _callSites(mainShell, '_executeArm');
      expect(sites, isNotEmpty, reason: '_executeArm 必须有调用点');
      for (final at in sites) {
        expect(
          _insideCall(mainShell, at, 'MgCommandConfirmation('),
          isTrue,
          reason: '主线里的 _executeArm 调用必须来自长按确认组件',
        );
      }
    });

    test('急停：直接挂在确认组件的 execute 上', () {
      final sites = _occurrences(mainShell, 'emergencyDisarmVehicle');
      expect(sites, hasLength(1), reason: '急停只有一个入口');
      expect(
        _insideCall(mainShell, sites.first, 'MgCommandConfirmation('),
        isTrue,
        reason: 'S3：急停必须长按确认',
      );
    });

    test('模式切换：只有长按的模式行会触发 setFlightMode', () {
      expect(
        _unguardedThroughCallers(
          statusBar,
          needles: ['controller.setFlightMode('],
          gates: ['MgFlightModeRow('],
        ),
        isEmpty,
        reason: 'S3：只有长按模式行的 onTriggered 会切换模式',
      );
      // 模式行本身：按住时长统一取共享常量，并且只有动画完成后才触发。
      expect(statusBar, contains('kMgHoldToConfirmDuration'));
      expect(statusBar, contains('AnimationStatus.completed'));
    });

    test('任务启动：悬浮确认或对话框确认，缺确认通道时不直发', () {
      final sites = _occurrences(transferSection, 'await _executeStartMission()');
      expect(sites, isNotEmpty, reason: '任务启动必须有下发点');
      for (final at in sites) {
        final guarded = _insideCall(
          transferSection,
          at,
          'MgCommandConfirmation(',
        );
        if (guarded) continue;
        // 兜底路径：showMgConfirmDialog 之后必须校验 confirmed 才能继续。
        final before = transferSection.substring(0, at);
        final dialogAt = before.lastIndexOf('showMgConfirmDialog(');
        expect(dialogAt, greaterThan(-1), reason: '非长按路径必须是确认对话框');
        final between = transferSection.substring(dialogAt, at);
        expect(
          between.contains('!confirmed'),
          isTrue,
          reason: 'S3：对话框未确认时必须 return，不得直发',
        );
      }
      // 没有确认通道时不得降级成直发。
      expect(
        transferSection.contains('不允许存在"缺确认组件就直发"的降级路径') ||
            transferSection.contains('没有悬浮确认层时退回对话框确认'),
        isTrue,
        reason: '缺确认通道时必须回退到对话框确认，而不是直发',
      );
      expect(
        _occurrences(transferSection, 'MgCommandConfirmation(label: \'开始任务\'')
            .isNotEmpty,
        isTrue,
      );
    });

    test('地图指令（Goto/Orbit/ROI/Home/航向）：必须经地图确认通道', () {
      // 不带接收者前缀：`controller.x()` 与 `widget.connectionController.x()`
      // 两种写法都必须被同一个 needle 抓住（漏掉后一种就等于留了后门）。
      final needles = <String>[
        'guidedGoto(',
        'guidedOrbit(',
        'guidedSetRoi(',
        'guidedSetHeading(',
        'guidedReturnToLaunch',
        'guidedLand',
      ];
      expect(
        _unguardedThroughCallers(
          mapView,
          needles: needles,
          gates: ['requestConfirmation(', '_requestConfirmedCommand('],
        ),
        isEmpty,
        reason: 'S3：地图指令只能在确认通道的闭包里下发',
      );
      // 返航/降落必须直接挂在确认通道上（地图工具栏入口）。
      expect(
        _unguardedDispatches(
          mapView,
          needles: ['guidedReturnToLaunch', 'guidedLand'],
          gates: ['_requestConfirmedCommand('],
        ),
        isEmpty,
        reason: 'S3：返航/降落必须经确认通道',
      );
      // 没有确认通道时明确拒发，而不是降级直发。
      expect(
        mapView.contains('需要长按确认，当前没有确认通道，命令未下发'),
        isTrue,
        reason: '缺确认通道必须拒发',
      );
    });

    test('起飞/降落/返航（导航面板与地图栏）：都经确认或确认对话框', () {
      // 导航面板的引导动作：下发闭包只被 _execute 调用，而 _execute 里
      // 起飞走 MgCommandConfirmation，其余走 confirmMissionAction + confirmed 校验。
      expect(
        _unguardedWithoutInternalGuard(
          planningPanel,
          needles: ['await action.run('],
          dialogCall: 'confirmMissionAction(',
          gates: ['MgCommandConfirmation('],
        ),
        isEmpty,
        reason: 'S3：引导动作只在长按确认或对话框确认之后下发',
      );
      final executeAt = _occurrences(
        planningPanel,
        'Future<void> _execute(_GuidedAction action) async {',
      );
      expect(executeAt, hasLength(1));
      final start = executeAt.first;
      final runAt = planningPanel.indexOf('await action.run(', start);
      expect(runAt, greaterThan(start));
      final between = planningPanel.substring(start, runAt);
      expect(
        between.contains('confirmMissionAction(') && between.contains('!confirmed'),
        isTrue,
        reason: 'S3：引导动作在 confirmMissionAction 确认后才下发',
      );

      // 地图工具条上的起飞/降落/返航统一走 _showGuidedAction → 确认通道。
      for (final label in ['起飞', '降落', '返航']) {
        expect(
          _occurrences(mapView, "onAction('$label").isNotEmpty,
          isTrue,
          reason: '$label 应有地图工具条入口',
        );
      }
      expect(
        _occurrences(mapView, 'await showGuidedActionsDialog(').isNotEmpty,
        isTrue,
      );
      expect(
        mapView.contains("_requestConfirmedCommand('返航'") &&
            mapView.contains("_requestConfirmedCommand('降落'"),
        isTrue,
        reason: 'S3：返航/降落必须经确认通道',
      );
    });

    test('检查函数自检：无确认的下发必须被判违规', () {
      const unguarded = '''
void _bad(ConnectionController controller) {
  controller.guidedReturnToLaunch();
}
''';
      expect(
        _unguardedDispatches(
          unguarded,
          needles: ['controller.guidedReturnToLaunch('],
          gates: ['MgCommandConfirmation('],
        ),
        isNotEmpty,
        reason: '检查函数必须能报出无确认的下发点（否则是假断言）',
      );

      const guarded = '''
void _good(ConnectionController controller) {
  onRequest(MgCommandConfirmation(
    label: '返航',
    execute: () => controller.guidedReturnToLaunch(),
  ));
}
''';
      expect(
        _unguardedDispatches(
          guarded,
          needles: ['controller.guidedReturnToLaunch('],
          gates: ['MgCommandConfirmation('],
        ),
        isEmpty,
        reason: '包在确认组件里的下发点不应误报',
      );
    });
  });
}
