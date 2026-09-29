import 'dart:io';

import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

/// 字体族：全应用只允许使用这里定义的两种。
///
/// 之前界面同时混用了 `macos_ui` 默认字体、Flutter 默认字体（模拟器上是
/// Roboto）和 `Menlo`，在 Windows 上会退化成"西文一种、中文另一种"的拼贴
/// 效果。这里为每个平台给出唯一的一条字体链，并统一走 [MgTypography]。
class MgFonts {
  const MgFonts._();

  /// 界面文字。按平台给出首选字体，并显式列出中文回退，避免逐字回退。
  static String get sans {
    if (Platform.isMacOS) return 'SF Pro Text';
    if (Platform.isWindows) return 'Segoe UI Variable Text';
    if (Platform.isAndroid) return 'Roboto';
    return 'Noto Sans';
  }

  /// 西文与中文回退链。顺序固定，保证同一台机器上每次都选中同一款字体。
  static List<String> get sansFallback {
    if (Platform.isMacOS) {
      return const ['SF Pro Text', 'Helvetica Neue', 'PingFang SC'];
    }
    if (Platform.isWindows) {
      return const ['Segoe UI Variable Text', 'Segoe UI', 'Microsoft YaHei UI'];
    }
    if (Platform.isAndroid) {
      return const ['Roboto', 'Noto Sans SC', 'Noto Sans CJK SC'];
    }
    return const ['Noto Sans', 'DejaVu Sans', 'Noto Sans CJK SC'];
  }

  /// 参数值、PWM、传感器读数等需要等宽对齐的数字。
  ///
  /// 不再使用 `Menlo`：它在 Windows/Linux 上不存在，会触发不可控回退。
  static String get mono {
    if (Platform.isMacOS) return 'SF Mono';
    if (Platform.isWindows) return 'Cascadia Mono';
    return 'Noto Sans Mono';
  }

  static List<String> get monoFallback {
    if (Platform.isMacOS) {
      return const ['SF Mono', 'Menlo', 'PingFang SC'];
    }
    if (Platform.isWindows) {
      return const ['Cascadia Mono', 'Consolas', 'Microsoft YaHei UI'];
    }
    return const ['Noto Sans Mono', 'DejaVu Sans Mono', 'Noto Sans CJK SC'];
  }
}

/// 全应用字号、字重、行高唯一来源。
///
/// 铁律：页面里不要再出现裸 `TextStyle(fontSize: ...)`。字号只能从下面取值，
/// 行高必须显式给出，这样中英文混排时行盒高度稳定，控件不会忽高忽低。
class MgTypography {
  const MgTypography._();

  static TextStyle sans(TextStyle style) => style.copyWith(
    fontFamily: MgFonts.sans,
    fontFamilyFallback: MgFonts.sansFallback,
  );

  static TextStyle mono(TextStyle style) => style.copyWith(
    fontFamily: MgFonts.mono,
    fontFamilyFallback: MgFonts.monoFallback,
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

  /// 说明文字的重音版本。
  static const TextStyle captionStrong = TextStyle(
    fontSize: 11,
    height: 1.40,
    fontWeight: FontWeight.w600,
  );

  /// 数字读数：等宽数字，位数变化时不抖动。
  static const TextStyle data = TextStyle(
    fontSize: 13,
    height: 1.30,
    fontWeight: FontWeight.w600,
    fontFamily: 'Cascadia Mono',
    fontFamilyFallback: ['SF Mono', 'Menlo', 'Consolas', 'Noto Sans Mono'],
    fontFeatures: [FontFeature.tabularFigures()],
  );

  /// 大号数字读数，例如传感器三轴当前值。
  static const TextStyle dataLarge = TextStyle(
    fontSize: 15,
    height: 1.30,
    fontWeight: FontWeight.w700,
    fontFamily: 'Cascadia Mono',
    fontFamilyFallback: ['SF Mono', 'Menlo', 'Consolas', 'Noto Sans Mono'],
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
      caption2: style(const TextStyle(), captionStrong),
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

  /// 电机输出进度条高度。
  static const double motorBarHeight = 24;
}
