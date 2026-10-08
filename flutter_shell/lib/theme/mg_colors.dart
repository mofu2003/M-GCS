import 'package:flutter/cupertino.dart';

/// 应用语义色板。
///
/// 每个颜色都按明暗模式成对定义，统一通过 [MgColors.of] 取当前模式的具体色值。
///
/// 两条约定：
///
/// 1. 调用处不要再写裸色值，也不要再写 `brightness.resolve(浅色, 深色)`；
/// 2. 不要直接使用未解析的 [CupertinoDynamicColor]。它未被解析时
///    `value` / `toARGB32()` 取的是浅色分支（Flutter 内部用 `_effectiveColor`），
///    直接交给画笔在深色模式下不会变色，必须经色板解析。
@immutable
class MgColors {
  const MgColors._(this.brightness);

  /// 取当前上下文的色板快照。
  static MgColors of(BuildContext context) =>
      MgColors._(MediaQuery.platformBrightnessOf(context));

  /// 当前明暗模式。
  final Brightness brightness;

  bool get isDark => brightness == Brightness.dark;

  Color _pick(Color light, Color dark) => isDark ? dark : light;

  // ── 强调色与状态色：取值对齐 macOS 系统色 ──────────────────────────

  /// 强调色（系统蓝）。
  Color get accent => _pick(const Color(0xFF007AFF), const Color(0xFF0A84FF));

  /// 强调色前景，用于选中导航项等实心强调底上的文字和图标。
  Color get onAccent => const Color(0xFFFFFFFF);

  /// 完全透明，供 Material、IconButton 等要求显式色值的位置使用。
  Color get transparent => const Color(0x00000000);

  /// 强调色的淡底，用于选中行、标记块和姿态图框。
  Color get accentSurface => accent.withValues(alpha: isDark ? 0.28 : 0.12);

  /// 成功、警告、错误状态色。
  Color get success => _pick(const Color(0xFF34C759), const Color(0xFF30D158));
  Color get warning => _pick(const Color(0xFFFF9500), const Color(0xFFFF9F0A));
  Color get danger => _pick(const Color(0xFFFF3B30), const Color(0xFFFF453A));

  // ── 底色 ──────────────────────────────────────────────────────────

  /// 卡片、弹层等前景底色。
  Color get surface => _pick(const Color(0xFFFFFFFF), const Color(0xFF252629));

  /// 页面、列表行等衬托底色。
  Color get surfaceMuted =>
      _pick(const Color(0xFFF7F7F9), const Color(0xFF26272B));

  /// 输入框、文本框底色。
  Color get well => _pick(const Color(0xFFF5F5F7), const Color(0xFF303136));

  /// 数值卡片底色：叠在信号卡片上，深色下比卡片底亮一档。
  Color get valueTile =>
      _pick(const Color(0xFFFFFFFF), const Color(0xFF303136));

  /// 图表画布底色。
  Color get chartSurface =>
      _pick(const Color(0xFFFDFDFD), const Color(0xFF202124));

  /// 面板底色与其边框。
  Color get panel => _pick(const Color(0xFFE6E9EA), const Color(0xFF2B2E33));
  Color get panelBorder =>
      _pick(const Color(0xFFE1E2E4), const Color(0xFF3E4045));

  /// 配置工作区底色、侧栏与标题栏；跨平台外壳必须显式绘制底色。
  Color get workspace =>
      _pick(const Color(0xFFF6F6F6), const Color(0xFF28282A));
  Color get sidebar => _pick(const Color(0xFFE3E3E3), const Color(0xFF303136));
  Color get toolbar => _pick(const Color(0xFFFFFFFF), const Color(0xFF28282A));

  // ── 文字与线条 ────────────────────────────────────────────────────

  /// 主要文字。
  Color get textPrimary =>
      _pick(const Color(0xFF1D1D1F), const Color(0xFFF5F5F7));

  /// 次要文字：说明文案、状态标签（对齐 macOS secondaryLabel）。
  Color get textSecondary =>
      _pick(const Color(0x80000000), const Color(0x8CFFFFFF));

  /// 更弱的文字：轴标签、辅助读数（对齐 black54 / white70）。
  Color get textTertiary =>
      _pick(const Color(0x8A000000), const Color(0xB3FFFFFF));

  /// 网格线、细分隔线。
  Color get hairline => _pick(const Color(0x14000000), const Color(0x14FFFFFF));

  /// 控件边框、工具栏分隔线；数值对齐 macos_ui 主题的 `dividerColor`。
  Color get divider => _pick(const Color(0x1F000000), const Color(0x1FFFFFFF));

  /// 浮层阴影。
  Color get shadow => const Color(0x3D000000);

  // ── 旧版 MacGlass 仪表盘配色 ─────────────────────────────────────
  //
  // 这一组颜色来自 v0.0.1-v0.0.5 的 MacGlassAttitudePanel，不再由当前页面
  // 各自发明。刻度、玻璃、姿态球都按这套旧仪表盘的视觉语义取值。
  /// 仪表盘主墨色。
  Color get instrumentInk => const Color(0xFF171A21);

  /// 姿态球天空：轻微透明粉蓝。
  Color get attitudeSky => const Color(0xE68FC6EA);

  /// 姿态球地面：轻微透明沙色。
  Color get attitudeGround => const Color(0xE6D9C6AA);

  /// 姿态球地平线。
  Color get attitudeHorizon => const Color(0xEFFFFFFF);

  /// 俯仰刻度线。
  Color get attitudeLadder => const Color(0x55FFFFFF);

  /// 姿态球边框。
  Color get attitudeBallBorder => const Color(0x55FFFFFF);

  /// 固定机翼基准。
  Color get attitudeAircraft => const Color(0xE8171A21);

  /// 仪表玻璃表盘与边框。
  Color get instrumentGlass => const Color(0xC7FFFFFF);
  Color get instrumentGlassBorder => const Color(0xE6FFFFFF);

  /// 右下角复合圆形仪表专用玻璃：仅保留很轻的透底和模糊。
  Color get attitudeCompassGlass => const Color(0xEBFFFFFF);
  Color get attitudeCompassGlassBorder => const Color(0xFFFFFFFF);

  /// 罗盘刻度、数字和北向标记。
  Color get instrumentTick => const Color(0x55171A21);
  Color get instrumentMajorTick => const Color(0xCC171A21);
  Color get instrumentLabel => const Color(0xA6171A21);
  Color get compassNorth => const Color(0xFFD85D53);

  // ── 地图浮动控件 ──────────────────────────────────────────────────

  /// 地图浮动控件的图标色。
  Color get mapControlIcon =>
      _pick(const Color(0xFF3B3B3B), const Color(0xFFE5E5E5));

  /// 所有悬浮面板的统一底色：65% 不透明。
  ///
  /// 顶部状态栏、地图工具列、飞行动作条、遥测条、设置菜单、连接/消息浮层都
  /// 用它，保证同一套透明度。改这里就是改全局"悬浮窗"观感。
  Color get floatingSurface =>
      _pick(const Color(0xA6FFFFFF), const Color(0xA62A2B2E));

  /// 地图浮动控件的悬停色。
  Color get mapControlHover =>
      _pick(const Color(0x11000000), const Color(0x22FFFFFF));

  /// 地图悬浮指令确认按钮：未按住时保持白色，长按后用强调色填充。
  Color get commandConfirmSurface => const Color(0xFFFFFFFF);

  /// 白色确认按钮上的文字和边框色。
  Color get commandConfirmForeground => const Color(0xFF000000);

  // ── 波形通道色 ────────────────────────────────────────────────────

  /// 波形曲线与瞬时值卡片的通道色。颜色固定，不随明暗模式切换。
  static const List<Color> waveformAccel = [
    Color(0xFFFF375F),
    Color(0xFF30D158),
    Color(0xFF0A84FF),
  ];

  static const List<Color> waveformGyro = [
    Color(0xFFFF7A00),
    Color(0xFFAF52DE),
    Color(0xFF14B8C4),
  ];

  static const List<Color> waveformMag = [
    Color(0xFFB25000),
    Color(0xFF34A853),
    Color(0xFF5E5CE6),
  ];

  // ── 数据进度条 ────────────────────────────────────────────────────

  /// 数据条填充区上的文字色。
  Color get onMotorBar => const Color(0xFF10231A);
}

/// 便捷取色：`context.mgColors.accent`。
extension MgColorsContext on BuildContext {
  MgColors get mgColors => MgColors.of(this);
}
