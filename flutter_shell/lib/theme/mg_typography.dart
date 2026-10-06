import 'dart:io';

import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

/// 字体族：全应用只允许使用这里定义的那一套。
///
/// 当前实验配置：MiSans 同时覆盖汉字与拉丁，因此全应用压到**一套字体**，
/// 三个平台字形完全一致。Arimo 仍随包分发但未启用，改一行即可切回
/// "Arimo 西文 + 系统中文" 的组合。决策与实测见
/// `docs/adr/0007-unified-font-stack.md` 与 `docs/research/FONT_ALTERNATIVES_RESEARCH.md`。
class MgFonts {
  const MgFonts._();

  /// 西文界面字体族名，与 `pubspec.yaml` 的声明一致。
  static const String arimo = 'Arimo';

  /// 中英文统一字体族名。
  static const String misans = 'MiSans';

  /// 界面文字族。MiSans 覆盖中英文，三平台一致。
  static String get sans => misans;

  /// 兜底链：只用于 MiSans 缺失的生僻字或符号，正常界面文本走不到这里。
  ///
  /// 不再按平台挑西文字体——不同平台解析到不同字体，正是此前 macOS 与 Windows
  /// 观感差异很大的原因。
  static List<String> get sansFallback {
    if (Platform.isMacOS) {
      return const ['PingFang SC'];
    }
    if (Platform.isWindows) {
      return const ['Microsoft YaHei UI'];
    }
    if (Platform.isAndroid) {
      return const ['Noto Sans SC'];
    }
    return const ['Noto Sans CJK SC'];
  }
}

/// 全应用字号、字重、行高唯一来源。
///
/// 铁律：页面里不要再出现裸 `TextStyle(fontSize: ...)`。字号只能从下面取值，
/// 行高必须显式给出，这样中英文混排时行盒高度稳定，控件不会忽高忽低。
///
/// 字重规则：**只有标题加粗**。页面标题、区块标题、卡片标题用 [title] /
/// [section] / [bodyStrong]；其余一切——正文、数值、状态、标签、徽标、传感器型号、
/// 固件名、机型名——都用 [body] / [caption]。语义层级靠字号与颜色承担，
/// 不靠字重堆叠，否则整页都在喊。
class MgTypography {
  const MgTypography._();

  static TextStyle sans(TextStyle style) => style.copyWith(
    fontFamily: MgFonts.sans,
    fontFamilyFallback: MgFonts.sansFallback,
  );

  /// 页面标题，例如"电机输出""飞控校准"。
  static const TextStyle title = TextStyle(
    fontSize: 17,
    height: 1.30,
    fontWeight: FontWeight.w600,
  );

  /// 区块标题，例如卡片里的"六面校准""容差等级"。
  static const TextStyle section = TextStyle(
    fontSize: 15,
    height: 1.35,
    fontWeight: FontWeight.w600,
  );

  /// 正文与表格内容。
  static const TextStyle body = TextStyle(
    fontSize: 13,
    height: 1.40,
    fontWeight: FontWeight.w400,
  );

  /// 强调正文，例如当前选中的导航项。
  static const TextStyle bodyStrong = TextStyle(
    fontSize: 13,
    height: 1.40,
    fontWeight: FontWeight.w600,
  );

  /// 说明文字、单位、次要状态。
  static const TextStyle caption = TextStyle(
    fontSize: 11,
    height: 1.40,
    fontWeight: FontWeight.w400,
  );

  /// 数字读数：与界面同一套字体，靠 tabularFigures 保证位数变化时不抖动。
  ///
  /// 数字一律不加粗。界面里数字密度高，加粗会让整片读数抢走标题的视觉层级，
  /// 也会让等宽数字的笔画糊在一起。层级靠字号和颜色区分，不靠字重。
  ///
  /// 不再单独引入等宽字体族：多一套族就多一处跨平台回退风险，而真正要的
  /// "数字对齐"由 tabular figures 提供。取用时用 [sans] 套上字体族。
  static const TextStyle data = TextStyle(
    fontSize: 13,
    height: 1.30,
    fontWeight: FontWeight.w400,
    fontFeatures: [FontFeature.tabularFigures()],
  );

  /// 大号数字读数，例如传感器三轴当前值。
  static const TextStyle dataLarge = TextStyle(
    fontSize: 15,
    height: 1.30,
    fontWeight: FontWeight.w400,
    fontFeatures: [FontFeature.tabularFigures()],
  );

  /// macos_ui 组件主题使用的同一套字体标准。
  ///
  /// 这只是兼容当前窗口主题的适配层；业务页面的新组件直接使用上面的
  /// token，后续完全移除 macos_ui 时无需改业务字号。
  static MacosTypography macos(Brightness brightness) {
    final color = brightness == Brightness.dark
        ? const Color(0xFFF5F5F7)
        : const Color(0xFF1D1D1F);
    TextStyle style(TextStyle source, TextStyle token) => source.copyWith(
      fontFamily: MgFonts.sans,
      fontFamilyFallback: MgFonts.sansFallback,
      fontSize: token.fontSize,
      height: token.height,
      fontWeight: token.fontWeight,
      color: color,
    );
    return MacosTypography(
      color: color,
      largeTitle: style(const TextStyle(), title),
      title1: style(const TextStyle(), title),
      title2: style(const TextStyle(), title),
      title3: style(const TextStyle(), section),
      headline: style(const TextStyle(), bodyStrong),
      body: style(const TextStyle(), body),
      callout: style(const TextStyle(), body),
      subheadline: style(const TextStyle(), caption),
      footnote: style(const TextStyle(), caption),
      caption1: style(const TextStyle(), caption),
      // caption2 是小号说明文字，不是标题，保持常规字重。
      caption2: style(const TextStyle(), caption),
    );
  }
}

/// 尺寸标准：控件高度、行高、间距、圆角。
///
/// 与字号一样，页面不要各自写魔法数字，避免"这个按钮 28、那个按钮 34"。
class MgMetrics {
  const MgMetrics._();

  /// 表格行高，所有单元格居中于该高度。
  static const double rowHeight = 48;

  /// 标准控件高度（输入框、下拉框、开关行）。
  static const double controlHeight = 30;

  /// 侧栏导航项高度。
  static const double navItemHeight = 34;

  /// 导航项图标尺寸。
  static const double navIconSize = 17;

  /// 工具栏高度（不含 macOS 标题栏留白）。
  static const double toolbarHeight = 52;

  /// 侧栏宽度。
  static const double sidebarWidth = 220;

  /// 窄窗口下收起为图标栏的宽度。
  static const double sidebarCompactWidth = 64;

  /// 卡片圆角。
  static const double panelRadius = 10;

  /// 控件圆角。
  static const double controlRadius = 7;

  /// 底部遥测仪表盘内容高度：两行读数。
  static const double dashboardHeight = 60;

  /// 仪表盘每列宽度：列内两行读数左对齐，列与列等宽。
  static const double dashboardColumnWidth = 168;

  /// 电机输出 / 遥控器校准的 PWM 进度条高度。
  ///
  /// PWM 读数就叠在条中间，而 [MgTypography.data] 的行盒是 13 × 1.30 ≈ 17，
  /// 所以这是"读数留在条内"前提下的下限；再矮就得把读数挪到条外。
  static const double motorBarHeight = 18;
}
