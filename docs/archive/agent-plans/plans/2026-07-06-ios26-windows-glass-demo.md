# iOS 26 Windows Glass Demo Implementation Plan

> 归档说明：这是历史临时执行计划，不属于当前开发基线。当前源码包的 `prototypes/` 目录为空，文中涉及的原型路径、Flutter 版本和测试命令不能作为当前验证依据。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary native-iOS demo with a Windows-runnable, Chinese-first iOS 26 liquid glass visual replica demo.

**Architecture:** Keep the prototype isolated under `prototypes/mgc_ios26_style_demo`. Use `liquid_glass_widgets 0.21.1` for real glass surfaces where its public API fits, and use small custom Flutter widgets for Windows pixel replication when UIKit platform views cannot render. Keep main app code untouched.

**Tech Stack:** Flutter 3.44.2, Dart 3.12.2, `adaptive_platform_ui 0.1.107`, `liquid_glass_widgets 0.21.1`, `fluentui_system_icons 1.1.273`, Cupertino widgets, Flutter painting and `BackdropFilter`.

---

## File Structure

- Modify: `prototypes/mgc_ios26_style_demo/pubspec.yaml`
  - Add local path dependencies for `liquid_glass_widgets` and `fluentui_system_icons`.
  - Keep `adaptive_platform_ui` as the iOS 26 semantic reference.
- Modify: `prototypes/mgc_ios26_style_demo/lib/main.dart`
  - Reduce entry point to app initialization and root widget.
- Create: `prototypes/mgc_ios26_style_demo/lib/src/demo_copy.dart`
  - Chinese/English strings and runtime locale enum.
- Create: `prototypes/mgc_ios26_style_demo/lib/src/demo_theme.dart`
  - Theme enum, font stacks, semantic color tokens, glass tuning values.
- Create: `prototypes/mgc_ios26_style_demo/lib/src/glass_primitives.dart`
  - Windows-safe glass surface, icon button, text button, status tag, text field shell.
- Create: `prototypes/mgc_ios26_style_demo/lib/src/ios26_showcase_app.dart`
  - Stateful demo app, locale/theme switching, component showcase.
- Modify: `prototypes/mgc_ios26_style_demo/test/ios26_demo_smoke_test.dart`
  - Verify Chinese-first render, locale toggle, theme toggle, core showcase sections.
- Create: `prototypes/mgc_ios26_style_demo/test/theme_tokens_test.dart`
  - Verify all three themes expose complete semantic tokens.

---

### Task 1: Add Demo Dependencies

**Files:**
- Modify: `prototypes/mgc_ios26_style_demo/pubspec.yaml`

- [ ] **Step 1: Modify dependencies**

Use local package paths already present in `D:\UI`:

```yaml
dependencies:
  flutter:
    sdk: flutter
  cupertino_icons: ^1.0.8
  adaptive_platform_ui:
    path: D:/UI/10_ios_cupertino_flutter/01_pub_packages/02_adaptive_desktop_shell/adaptive_platform_ui-0.1.107
  liquid_glass_widgets:
    path: D:/UI/10_ios_cupertino_flutter/01_pub_packages/05_liquid_glass_experiments/liquid_glass_widgets-0.21.1
  fluentui_system_icons:
    path: D:/UI/01_core_ui_admin_icons/fluentui_system_icons-1.1.273
```

Do not add `macos_ui` as a runtime dependency. Its README says non-macOS support is not guaranteed, so this demo uses it as visual reference only.

- [ ] **Step 2: Fetch dependencies**

Run:

```powershell
flutter pub get
```

Expected: dependency resolution succeeds and `pubspec.lock` contains `liquid_glass_widgets` and `fluentui_system_icons`.

---

### Task 2: Add Theme Tokens

**Files:**
- Create: `prototypes/mgc_ios26_style_demo/lib/src/demo_theme.dart`
- Test: `prototypes/mgc_ios26_style_demo/test/theme_tokens_test.dart`

- [ ] **Step 1: Write token test**

Create `test/theme_tokens_test.dart`:

```dart
import 'package:flutter_test/flutter_test.dart';
import 'package:mgc_ios26_style_demo/src/demo_theme.dart';

void main() {
  test('all iOS 26 replica themes expose complete tokens', () {
    for (final mode in DemoThemeMode.values) {
      final palette = DemoPalette.forMode(mode);

      expect(palette.accent.a, greaterThan(0));
      expect(palette.backgroundBase.a, greaterThan(0));
      expect(palette.backgroundWashA.a, greaterThan(0));
      expect(palette.backgroundWashB.a, greaterThan(0));
      expect(palette.backgroundWashC.a, greaterThan(0));
      expect(palette.glassTint.a, greaterThan(0));
      expect(palette.glassStroke.a, greaterThan(0));
      expect(palette.glassHighlight.a, greaterThan(0));
      expect(palette.textPrimary.a, greaterThan(0));
      expect(palette.textSecondary.a, greaterThan(0));
      expect(palette.textMuted.a, greaterThan(0));
      expect(palette.danger.a, greaterThan(0));
      expect(palette.warning.a, greaterThan(0));
      expect(palette.success.a, greaterThan(0));
      expect(palette.blurSigma, inInclusiveRange(18, 36));
      expect(palette.cornerRadius, inInclusiveRange(22, 34));
    }
  });
}
```

- [ ] **Step 2: Run test and verify it fails**

Run:

```powershell
flutter test test/theme_tokens_test.dart
```

Expected: FAIL because `src/demo_theme.dart` does not exist.

- [ ] **Step 3: Create theme file**

Create `lib/src/demo_theme.dart`:

```dart
import 'package:flutter/cupertino.dart';

enum DemoThemeMode { lightGlass, darkGlass, flightContrast }

class DemoFonts {
  const DemoFonts._();

  static const chinese = <String>[
    'PingFang SC',
    'Microsoft YaHei UI',
    'Noto Sans CJK SC',
    'Source Han Sans SC',
  ];

  static const english = <String>[
    'SF Pro Text',
    'SF Pro Display',
    'Segoe UI Variable',
    'Segoe UI',
    'Arial',
  ];

  static const mono = <String>[
    'SF Mono',
    'Cascadia Mono',
    'JetBrains Mono',
    'Consolas',
  ];
}

class DemoPalette {
  const DemoPalette({
    required this.name,
    required this.accent,
    required this.backgroundBase,
    required this.backgroundWashA,
    required this.backgroundWashB,
    required this.backgroundWashC,
    required this.glassTint,
    required this.glassStroke,
    required this.glassHighlight,
    required this.textPrimary,
    required this.textSecondary,
    required this.textMuted,
    required this.danger,
    required this.warning,
    required this.success,
    required this.shadowSoft,
    required this.shadowLifted,
    required this.blurSigma,
    required this.cornerRadius,
  });

  final String name;
  final Color accent;
  final Color backgroundBase;
  final Color backgroundWashA;
  final Color backgroundWashB;
  final Color backgroundWashC;
  final Color glassTint;
  final Color glassStroke;
  final Color glassHighlight;
  final Color textPrimary;
  final Color textSecondary;
  final Color textMuted;
  final Color danger;
  final Color warning;
  final Color success;
  final Color shadowSoft;
  final Color shadowLifted;
  final double blurSigma;
  final double cornerRadius;

  bool get isDark => backgroundBase.computeLuminance() < 0.2;

  static DemoPalette forMode(DemoThemeMode mode) {
    return switch (mode) {
      DemoThemeMode.lightGlass => const DemoPalette(
          name: 'Light Glass',
          accent: Color(0xFF2F6BFF),
          backgroundBase: Color(0xFFF6F8FB),
          backgroundWashA: Color(0xFFDDEEFF),
          backgroundWashB: Color(0xFFFFF5D6),
          backgroundWashC: Color(0xFFE9E4FF),
          glassTint: Color(0xB8FFFFFF),
          glassStroke: Color(0x8CFFFFFF),
          glassHighlight: Color(0xE6FFFFFF),
          textPrimary: Color(0xF0000000),
          textSecondary: Color(0xB0000000),
          textMuted: Color(0x73000000),
          danger: Color(0xFFFF3B30),
          warning: Color(0xFFFF9F0A),
          success: Color(0xFF34C759),
          shadowSoft: Color(0x26000000),
          shadowLifted: Color(0x33000000),
          blurSigma: 28,
          cornerRadius: 30,
        ),
      DemoThemeMode.darkGlass => const DemoPalette(
          name: 'Dark Glass',
          accent: Color(0xFF7AA2FF),
          backgroundBase: Color(0xFF080A10),
          backgroundWashA: Color(0xFF1B315F),
          backgroundWashB: Color(0xFF2D244B),
          backgroundWashC: Color(0xFF092F38),
          glassTint: Color(0x663A3D46),
          glassStroke: Color(0x33FFFFFF),
          glassHighlight: Color(0x66FFFFFF),
          textPrimary: Color(0xF5FFFFFF),
          textSecondary: Color(0xB8FFFFFF),
          textMuted: Color(0x80FFFFFF),
          danger: Color(0xFFFF6961),
          warning: Color(0xFFFFB340),
          success: Color(0xFF4EE083),
          shadowSoft: Color(0x73000000),
          shadowLifted: Color(0x99000000),
          blurSigma: 30,
          cornerRadius: 30,
        ),
      DemoThemeMode.flightContrast => const DemoPalette(
          name: 'Flight Contrast',
          accent: Color(0xFF00A7FF),
          backgroundBase: Color(0xFF071018),
          backgroundWashA: Color(0xFF102A3E),
          backgroundWashB: Color(0xFF0D3C2C),
          backgroundWashC: Color(0xFF3B250A),
          glassTint: Color(0x8A101820),
          glassStroke: Color(0x4DFFFFFF),
          glassHighlight: Color(0x66FFFFFF),
          textPrimary: Color(0xFFFFFFFF),
          textSecondary: Color(0xD9FFFFFF),
          textMuted: Color(0xA6FFFFFF),
          danger: Color(0xFFFF453A),
          warning: Color(0xFFFFD60A),
          success: Color(0xFF30D158),
          shadowSoft: Color(0x8C000000),
          shadowLifted: Color(0xB3000000),
          blurSigma: 20,
          cornerRadius: 24,
        ),
    };
  }
}
```

- [ ] **Step 4: Run token test**

Run:

```powershell
flutter test test/theme_tokens_test.dart
```

Expected: PASS.

---

### Task 3: Add Chinese/English Copy

**Files:**
- Create: `prototypes/mgc_ios26_style_demo/lib/src/demo_copy.dart`
- Modify: `prototypes/mgc_ios26_style_demo/test/ios26_demo_smoke_test.dart`

- [ ] **Step 1: Create copy file**

Create `lib/src/demo_copy.dart`:

```dart
enum DemoLocale { zh, en }

class DemoCopy {
  const DemoCopy(this.locale);

  final DemoLocale locale;

  bool get isZh => locale == DemoLocale.zh;

  String get appTitle => isZh ? 'iOS 26 玻璃复刻' : 'iOS 26 Glass Replica';
  String get toolbarTitle => isZh ? '飞控界面' : 'Flight UI';
  String get language => isZh ? '中文' : 'EN';
  String get theme => isZh ? '主题' : 'Theme';
  String get components => isZh ? '组件' : 'Components';
  String get controls => isZh ? '控制' : 'Controls';
  String get telemetry => isZh ? '遥测' : 'Telemetry';
  String get menu => isZh ? '弹出菜单' : 'Popup Menu';
  String get fields => isZh ? '文本框' : 'Fields';
  String get alerts => isZh ? '弹窗' : 'Alerts';
  String get connected => isZh ? '已连接' : 'Connected';
  String get warning => isZh ? '警告' : 'Warning';
  String get armed => isZh ? '已解锁' : 'Armed';
  String get gps => isZh ? 'GPS 良好' : 'GPS Good';
  String get battery => isZh ? '电池 78%' : 'Battery 78%';
  String get searchHint => isZh ? '搜索参数、任务、航点' : 'Search params, missions, waypoints';
  String get textFieldLabel => isZh ? '任务名称' : 'Mission name';
  String get textFieldValue => isZh ? '东侧巡检航线' : 'East inspection route';
  String get showAlert => isZh ? '显示 Alert' : 'Show Alert';
  String get alertTitle => isZh ? '确认上传任务？' : 'Upload mission?';
  String get alertMessage => isZh ? '这会把当前航线上传到已连接飞控。' : 'This uploads the active route to the connected controller.';
  String get cancel => isZh ? '取消' : 'Cancel';
  String get confirm => isZh ? '确认' : 'Confirm';
  String get bottomHome => isZh ? '首页' : 'Home';
  String get bottomFlight => isZh ? '飞行' : 'Flight';
  String get bottomSearch => isZh ? '搜索' : 'Search';
  String get bottomSettings => isZh ? '设置' : 'Settings';
}
```

- [ ] **Step 2: Update smoke test expectations**

Replace `test/ios26_demo_smoke_test.dart` with:

```dart
import 'package:flutter_test/flutter_test.dart';
import 'package:mgc_ios26_style_demo/main.dart';

void main() {
  testWidgets('iOS 26 replica demo renders Chinese-first showcase', (
    tester,
  ) async {
    await tester.pumpWidget(const Ios26ReplicaDemoApp());

    expect(find.text('iOS 26 玻璃复刻'), findsOneWidget);
    expect(find.text('组件'), findsAtLeastNWidgets(1));
    expect(find.text('控制'), findsAtLeastNWidgets(1));
    expect(find.text('文本框'), findsOneWidget);
    expect(find.text('弹出菜单'), findsOneWidget);
  });

  testWidgets('demo can switch to English', (tester) async {
    await tester.pumpWidget(const Ios26ReplicaDemoApp());

    await tester.tap(find.text('EN'));
    await tester.pumpAndSettle();

    expect(find.text('iOS 26 Glass Replica'), findsOneWidget);
    expect(find.text('Components'), findsAtLeastNWidgets(1));
  });
}
```

Expected after this step: test still FAILS because `Ios26ReplicaDemoApp` is not defined.

---

### Task 4: Add Glass Primitives

**Files:**
- Create: `prototypes/mgc_ios26_style_demo/lib/src/glass_primitives.dart`

- [ ] **Step 1: Create primitive widgets**

Create `lib/src/glass_primitives.dart`:

```dart
import 'dart:ui';

import 'package:flutter/cupertino.dart';
import 'package:fluentui_system_icons/fluentui_system_icons.dart';

import 'demo_theme.dart';

class MgcGlassSurface extends StatelessWidget {
  const MgcGlassSurface({
    super.key,
    required this.palette,
    required this.child,
    this.padding = const EdgeInsets.all(16),
    this.radius,
    this.opacityBoost = 0,
  });

  final DemoPalette palette;
  final Widget child;
  final EdgeInsetsGeometry padding;
  final double? radius;
  final double opacityBoost;

  @override
  Widget build(BuildContext context) {
    final r = radius ?? palette.cornerRadius;
    return ClipRRect(
      borderRadius: BorderRadius.circular(r),
      child: BackdropFilter(
        filter: ImageFilter.blur(
          sigmaX: palette.blurSigma,
          sigmaY: palette.blurSigma,
        ),
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: palette.glassTint.withValues(
              alpha: (palette.glassTint.a + opacityBoost).clamp(0.0, 1.0),
            ),
            borderRadius: BorderRadius.circular(r),
            border: Border.all(color: palette.glassStroke),
            boxShadow: [
              BoxShadow(
                color: palette.shadowSoft,
                blurRadius: 32,
                offset: const Offset(0, 18),
              ),
            ],
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [
                palette.glassHighlight.withValues(alpha: 0.34),
                palette.glassTint.withValues(alpha: 0.10),
              ],
            ),
          ),
          child: Padding(padding: padding, child: child),
        ),
      ),
    );
  }
}

class MgcGlassButton extends StatefulWidget {
  const MgcGlassButton({
    super.key,
    required this.palette,
    required this.label,
    required this.onPressed,
    this.icon,
    this.danger = false,
    this.filled = false,
    this.enabled = true,
  });

  final DemoPalette palette;
  final String label;
  final VoidCallback onPressed;
  final IconData? icon;
  final bool danger;
  final bool filled;
  final bool enabled;

  @override
  State<MgcGlassButton> createState() => _MgcGlassButtonState();
}

class _MgcGlassButtonState extends State<MgcGlassButton> {
  bool _down = false;

  @override
  Widget build(BuildContext context) {
    final foreground = widget.danger ? widget.palette.danger : widget.palette.accent;
    final active = widget.enabled;
    return GestureDetector(
      onTapDown: active ? (_) => setState(() => _down = true) : null,
      onTapCancel: active ? () => setState(() => _down = false) : null,
      onTapUp: active
          ? (_) {
              setState(() => _down = false);
              widget.onPressed();
            }
          : null,
      child: AnimatedScale(
        scale: _down ? 0.96 : 1,
        duration: const Duration(milliseconds: 120),
        curve: Curves.easeOutCubic,
        child: Opacity(
          opacity: active ? 1 : 0.45,
          child: MgcGlassSurface(
            palette: widget.palette,
            radius: 999,
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 11),
            opacityBoost: widget.filled ? 0.16 : 0,
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                if (widget.icon != null) ...[
                  Icon(widget.icon, size: 18, color: foreground),
                  const SizedBox(width: 8),
                ],
                Text(
                  widget.label,
                  style: TextStyle(
                    color: widget.filled ? widget.palette.textPrimary : foreground,
                    fontWeight: FontWeight.w700,
                    fontSize: 14,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class MgcIconButton extends StatelessWidget {
  const MgcIconButton({
    super.key,
    required this.palette,
    required this.icon,
    required this.label,
    required this.onPressed,
  });

  final DemoPalette palette;
  final IconData icon;
  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      label: label,
      button: true,
      child: Tooltip(
        message: label,
        child: CupertinoButton(
          padding: EdgeInsets.zero,
          onPressed: onPressed,
          child: MgcGlassSurface(
            palette: palette,
            radius: 999,
            padding: const EdgeInsets.all(10),
            child: Icon(icon, size: 22, color: palette.textPrimary),
          ),
        ),
      ),
    );
  }
}

class MgcStatusTag extends StatelessWidget {
  const MgcStatusTag({
    super.key,
    required this.palette,
    required this.label,
    required this.color,
  });

  final DemoPalette palette;
  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return MgcGlassSurface(
      palette: palette,
      radius: 999,
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      opacityBoost: 0.08,
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(FluentIcons.circle_12_filled, size: 9, color: color),
          const SizedBox(width: 6),
          Text(
            label,
            style: TextStyle(
              color: palette.textPrimary,
              fontSize: 12,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    );
  }
}
```

- [ ] **Step 2: Run analyzer**

Run:

```powershell
flutter analyze
```

Expected: still FAILS until the root app is updated, or PASS if no references exist yet. Fix only syntax errors in this file.

---

### Task 5: Replace App Entry Point

**Files:**
- Modify: `prototypes/mgc_ios26_style_demo/lib/main.dart`
- Create: `prototypes/mgc_ios26_style_demo/lib/src/ios26_showcase_app.dart`

- [ ] **Step 1: Replace main entry**

Replace `lib/main.dart` with:

```dart
import 'package:flutter/cupertino.dart';
import 'package:liquid_glass_widgets/liquid_glass_widgets.dart';

import 'src/ios26_showcase_app.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await LiquidGlassWidgets.initialize();

  runApp(LiquidGlassWidgets.wrap(child: const Ios26ReplicaDemoApp()));
}
```

- [ ] **Step 2: Create showcase app**

Create `lib/src/ios26_showcase_app.dart` with these sections in one file for the first demo:

```dart
import 'package:flutter/cupertino.dart';
import 'package:fluentui_system_icons/fluentui_system_icons.dart';
import 'package:liquid_glass_widgets/liquid_glass_widgets.dart';

import 'demo_copy.dart';
import 'demo_theme.dart';
import 'glass_primitives.dart';

class Ios26ReplicaDemoApp extends StatefulWidget {
  const Ios26ReplicaDemoApp({super.key});

  @override
  State<Ios26ReplicaDemoApp> createState() => _Ios26ReplicaDemoAppState();
}

class _Ios26ReplicaDemoAppState extends State<Ios26ReplicaDemoApp> {
  DemoLocale _locale = DemoLocale.zh;
  DemoThemeMode _themeMode = DemoThemeMode.lightGlass;
  int _segment = 0;
  bool _armed = true;
  double _speed = 0.56;

  @override
  Widget build(BuildContext context) {
    final copy = DemoCopy(_locale);
    final palette = DemoPalette.forMode(_themeMode);

    return CupertinoApp(
      debugShowCheckedModeBanner: false,
      title: copy.appTitle,
      theme: CupertinoThemeData(
        brightness: palette.isDark ? Brightness.dark : Brightness.light,
        primaryColor: palette.accent,
        scaffoldBackgroundColor: palette.backgroundBase,
        textTheme: CupertinoTextThemeData(
          textStyle: TextStyle(
            color: palette.textPrimary,
            fontFamilyFallback: DemoFonts.chinese + DemoFonts.english,
            letterSpacing: 0,
          ),
        ),
      ),
      home: CupertinoPageScaffold(
        child: Stack(
          children: [
            Positioned.fill(child: _LiquidBackdrop(palette: palette)),
            Positioned.fill(
              top: 74,
              bottom: 82,
              child: _ShowcaseBody(
                copy: copy,
                palette: palette,
                segment: _segment,
                armed: _armed,
                speed: _speed,
                onSegment: (value) => setState(() => _segment = value),
                onArmed: (value) => setState(() => _armed = value),
                onSpeed: (value) => setState(() => _speed = value),
              ),
            ),
            Positioned(
              top: 14,
              left: 18,
              right: 18,
              child: _TopToolbar(
                copy: copy,
                palette: palette,
                onLocale: () => setState(() {
                  _locale = _locale == DemoLocale.zh ? DemoLocale.en : DemoLocale.zh;
                }),
                onTheme: () => setState(() {
                  final modes = DemoThemeMode.values;
                  _themeMode = modes[(modes.indexOf(_themeMode) + 1) % modes.length];
                }),
              ),
            ),
            Positioned(
              left: 22,
              right: 22,
              bottom: 18,
              child: _BottomTabs(copy: copy, palette: palette),
            ),
          ],
        ),
      ),
    );
  }
}

class _LiquidBackdrop extends StatelessWidget {
  const _LiquidBackdrop({required this.palette});

  final DemoPalette palette;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: palette.backgroundBase,
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            palette.backgroundWashA,
            palette.backgroundBase,
            palette.backgroundWashB,
            palette.backgroundWashC,
          ],
        ),
      ),
      child: CustomPaint(painter: _BackdropPainter(palette)),
    );
  }
}

class _BackdropPainter extends CustomPainter {
  const _BackdropPainter(this.palette);

  final DemoPalette palette;

  @override
  void paint(Canvas canvas, Size size) {
    final paints = [
      Paint()..color = palette.glassHighlight.withValues(alpha: 0.18),
      Paint()..color = palette.accent.withValues(alpha: 0.12),
      Paint()..color = palette.success.withValues(alpha: 0.08),
    ];

    canvas.drawOval(
      Rect.fromCenter(
        center: Offset(size.width * 0.22, size.height * 0.18),
        width: size.width * 0.75,
        height: size.height * 0.38,
      ),
      paints[0],
    );
    canvas.drawOval(
      Rect.fromCenter(
        center: Offset(size.width * 0.82, size.height * 0.38),
        width: size.width * 0.55,
        height: size.height * 0.48,
      ),
      paints[1],
    );
    canvas.drawOval(
      Rect.fromCenter(
        center: Offset(size.width * 0.46, size.height * 0.86),
        width: size.width * 0.82,
        height: size.height * 0.34,
      ),
      paints[2],
    );
  }

  @override
  bool shouldRepaint(covariant _BackdropPainter oldDelegate) =>
      oldDelegate.palette != palette;
}

class _TopToolbar extends StatelessWidget {
  const _TopToolbar({
    required this.copy,
    required this.palette,
    required this.onLocale,
    required this.onTheme,
  });

  final DemoCopy copy;
  final DemoPalette palette;
  final VoidCallback onLocale;
  final VoidCallback onTheme;

  @override
  Widget build(BuildContext context) {
    return MgcGlassSurface(
      palette: palette,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      child: Row(
        children: [
          MgcIconButton(
            palette: palette,
            icon: FluentIcons.chevron_left_24_regular,
            label: 'Back',
            onPressed: () {},
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              copy.appTitle,
              textAlign: TextAlign.center,
              style: TextStyle(
                color: palette.textPrimary,
                fontSize: 18,
                fontWeight: FontWeight.w800,
              ),
            ),
          ),
          const SizedBox(width: 10),
          MgcGlassButton(
            palette: palette,
            label: copy.language == '中文' ? 'EN' : '中文',
            onPressed: onLocale,
          ),
          const SizedBox(width: 8),
          MgcGlassButton(
            palette: palette,
            label: copy.theme,
            icon: FluentIcons.weather_moon_24_regular,
            onPressed: onTheme,
          ),
        ],
      ),
    );
  }
}

class _ShowcaseBody extends StatelessWidget {
  const _ShowcaseBody({
    required this.copy,
    required this.palette,
    required this.segment,
    required this.armed,
    required this.speed,
    required this.onSegment,
    required this.onArmed,
    required this.onSpeed,
  });

  final DemoCopy copy;
  final DemoPalette palette;
  final int segment;
  final bool armed;
  final double speed;
  final ValueChanged<int> onSegment;
  final ValueChanged<bool> onArmed;
  final ValueChanged<double> onSpeed;

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.fromLTRB(22, 16, 22, 28),
      children: [
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            MgcStatusTag(palette: palette, label: copy.connected, color: palette.success),
            MgcStatusTag(palette: palette, label: copy.armed, color: palette.warning),
            MgcStatusTag(palette: palette, label: copy.gps, color: palette.accent),
            MgcStatusTag(palette: palette, label: copy.battery, color: palette.success),
          ],
        ),
        const SizedBox(height: 16),
        _Section(
          title: copy.components,
          palette: palette,
          child: Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              MgcGlassButton(palette: palette, label: 'Filled', filled: true, onPressed: () {}),
              MgcGlassButton(palette: palette, label: 'Tinted', onPressed: () {}),
              MgcGlassButton(palette: palette, label: 'Glass', icon: FluentIcons.sparkle_24_regular, onPressed: () {}),
              MgcGlassButton(palette: palette, label: 'Delete', danger: true, icon: FluentIcons.delete_24_regular, onPressed: () {}),
              MgcGlassButton(palette: palette, label: 'Disabled', enabled: false, onPressed: () {}),
            ],
          ),
        ),
        _Section(
          title: copy.controls,
          palette: palette,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              GlassSegmentedControl(
                segments: [
                  GlassSegment(label: Text(copy.components), icon: const Icon(FluentIcons.apps_24_regular)),
                  GlassSegment(label: Text(copy.telemetry), icon: const Icon(FluentIcons.airplane_24_regular)),
                  GlassSegment(label: Text(copy.fields), icon: const Icon(FluentIcons.text_field_24_regular)),
                ],
                selectedIndex: segment,
                onSegmentSelected: onSegment,
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(child: Text(copy.armed, style: TextStyle(color: palette.textPrimary, fontWeight: FontWeight.w700))),
                  GlassSwitch(value: armed, onChanged: onArmed),
                ],
              ),
              const SizedBox(height: 12),
              GlassSlider(value: speed, onChanged: onSpeed),
            ],
          ),
        ),
        _Section(
          title: copy.fields,
          palette: palette,
          child: Column(
            children: [
              GlassTextField.search(placeholder: copy.searchHint),
              const SizedBox(height: 12),
              GlassTextField(
                placeholder: copy.textFieldLabel,
                controller: TextEditingController(text: copy.textFieldValue),
              ),
            ],
          ),
        ),
        _Section(
          title: copy.menu,
          palette: palette,
          child: Wrap(
            spacing: 10,
            children: [
              GlassPullDownButton(
                items: [
                  GlassMenuItem(title: copy.connected, icon: const Icon(FluentIcons.link_24_regular), onTap: () {}),
                  const GlassMenuDivider(),
                  GlassMenuItem(title: copy.warning, icon: const Icon(FluentIcons.warning_24_regular), onTap: () {}),
                ],
                label: copy.menu,
                icon: const Icon(FluentIcons.more_horizontal_24_regular),
                buttonWidth: 148,
              ),
              MgcGlassButton(
                palette: palette,
                label: copy.showAlert,
                icon: FluentIcons.window_24_regular,
                onPressed: () => showCupertinoDialog<void>(
                  context: context,
                  builder: (dialogContext) => GlassDialog(
                    title: copy.alertTitle,
                    message: copy.alertMessage,
                    actions: [
                      GlassDialogAction(label: copy.cancel, onPressed: () => Navigator.of(dialogContext).pop()),
                      GlassDialogAction(label: copy.confirm, onPressed: () => Navigator.of(dialogContext).pop(), isPrimary: true),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
        _TelemetryCards(copy: copy, palette: palette),
      ],
    );
  }
}

class _Section extends StatelessWidget {
  const _Section({required this.title, required this.palette, required this.child});

  final String title;
  final DemoPalette palette;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 16),
      child: MgcGlassSurface(
        palette: palette,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: TextStyle(color: palette.textPrimary, fontSize: 17, fontWeight: FontWeight.w800)),
            const SizedBox(height: 14),
            child,
          ],
        ),
      ),
    );
  }
}

class _TelemetryCards extends StatelessWidget {
  const _TelemetryCards({required this.copy, required this.palette});

  final DemoCopy copy;
  final DemoPalette palette;

  @override
  Widget build(BuildContext context) {
    final values = [
      ('ALT', '128.4', 'm'),
      ('SPD', '14.8', 'm/s'),
      ('HDG', '086', 'deg'),
    ];
    return Row(
      children: [
        for (final item in values)
          Expanded(
            child: Padding(
              padding: const EdgeInsets.only(right: 8),
              child: MgcGlassSurface(
                palette: palette,
                opacityBoost: 0.08,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(item.$1, style: TextStyle(color: palette.textMuted, fontSize: 11, fontWeight: FontWeight.w800)),
                    const SizedBox(height: 6),
                    Text(
                      item.$2,
                      style: TextStyle(
                        color: palette.textPrimary,
                        fontFamilyFallback: DemoFonts.mono,
                        fontSize: 24,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    Text(item.$3, style: TextStyle(color: palette.textSecondary, fontSize: 12)),
                  ],
                ),
              ),
            ),
          ),
      ],
    );
  }
}

class _BottomTabs extends StatelessWidget {
  const _BottomTabs({required this.copy, required this.palette});

  final DemoCopy copy;
  final DemoPalette palette;

  @override
  Widget build(BuildContext context) {
    return GlassBottomBar(
      tabs: [
        GlassBottomBarTab(icon: const Icon(FluentIcons.home_24_regular), label: copy.bottomHome),
        GlassBottomBarTab(icon: const Icon(FluentIcons.airplane_24_regular), label: copy.bottomFlight),
        GlassBottomBarTab(icon: const Icon(FluentIcons.search_24_regular), label: copy.bottomSearch),
        GlassBottomBarTab(icon: const Icon(FluentIcons.settings_24_regular), label: copy.bottomSettings),
      ],
      selectedIndex: 0,
      onTabSelected: (_) {},
    );
  }
}
```

- [ ] **Step 3: Run analyzer for exact API verification**

Run:

```powershell
flutter analyze
```

Expected: `No issues found!`. The code above uses the checked 0.21.1 public API: `GlassSegmentedControl.onSegmentSelected`, `GlassMenuItem.title`, `GlassDialog.title/message`, `GlassDialogAction.isPrimary`, and `GlassBottomBar.onTabSelected`.

---

### Task 6: Verify Demo Behavior

**Files:**
- Modify if needed: `prototypes/mgc_ios26_style_demo/test/ios26_demo_smoke_test.dart`
- Modify if needed: `prototypes/mgc_ios26_style_demo/lib/src/ios26_showcase_app.dart`

- [ ] **Step 1: Run smoke tests**

Run:

```powershell
flutter test
```

Expected: all tests pass. If the widget tree lazily builds lower sections, scroll in the test before asserting them.

- [ ] **Step 2: Run analyzer**

Run:

```powershell
flutter analyze
```

Expected: `No issues found!`

- [ ] **Step 3: Build Windows**

Run:

```powershell
flutter build windows
```

Expected: `Built build\windows\x64\runner\Release\mgc_ios26_style_demo.exe`

- [ ] **Step 4: Confirm `D:\UI` stays clean**

Run from repo root:

```powershell
Get-ChildItem -LiteralPath 'D:\UI' -Recurse -Force -Directory |
  Where-Object { $_.Name -in @('.dart_tool','build','ephemeral') } |
  Select-Object -ExpandProperty FullName
```

Expected: no output.

---

## Self-Review

- Spec coverage: covers Windows visual-first iOS 26 replica, `liquid_glass_widgets 0.21.1`, `adaptive_platform_ui` as reference, Fluent icons, Chinese/English switching, three themes, component showcase, test/analyze/build verification, and `D:\UI` cleanliness.
- Placeholder scan: no `TODO`, `TBD`, or vague future work instructions.
- Type consistency: task files define `DemoThemeMode`, `DemoPalette`, `DemoLocale`, `DemoCopy`, `MgcGlassSurface`, `MgcGlassButton`, `MgcIconButton`, `MgcStatusTag`, and `Ios26ReplicaDemoApp` before tests rely on them.
