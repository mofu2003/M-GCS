import 'dart:io';

import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

/// 字体族：全应用只允许使用这里定义的那一套。
///
/// MiSans 同时覆盖汉字与拉丁，是 pubspec.yaml 中唯一打包的字体族。
/// 平台回退仅用于这套字体不包含的字符。
class MgFonts {
  const MgFonts._();

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

  /// 表头，例如电机参数表的列名。U4 允许表头加粗，其余正文一律 w400。
  static const TextStyle tableHeader = TextStyle(
    fontSize: 11,
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

  /// 罗盘顶部的大号航向读数。读数不是标题，保持常规字重。
  static const TextStyle compassHeading = TextStyle(
    fontSize: 24,
    height: 1.15,
    fontWeight: FontWeight.w400,
    fontFeatures: [FontFeature.tabularFigures()],
  );

  /// 复合仪表航向读数：比普通数据字号小一级，保证方向字和三位航向同时放下。
  static const TextStyle instrumentHeadingReadout = TextStyle(
    fontSize: 12,
    height: 1.1,
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

  /// 后台页面统一间距标准（标签页内层叠加 MacosTabView 自身的 12pt）。
  ///
  /// 左右边距 24、区块间距 16、区块标题到内容 8、卡片间距 10。
  /// 新页面沿用这套数值，避免各页各自为政导致疏密不一致。
  static const double configPageMargin = 24;

  /// 配置标签页内容缩进；内层再补齐到页面边距。
  static const double configTabBodyInset = 12;

  /// 配置区块之间的间距。
  static const double configSectionGap = 16;

  /// 配置区块标题与内容的间距。
  static const double configHeaderGap = 8;

  /// 配置卡片之间的间距。
  static const double configCardGap = 10;

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

  /// 左下角姿态仪直径。

  /// 右下角罗盘表盘直径。
  static const double compassSize = 230;

  /// 右下角复合仪表的玻璃外径。
  static const double attitudeCompassSize = 196;

  /// 多机批量面板的统一布局尺寸。
  static const double fleetPanelWidth = 360;
  static const double fleetPanelInset = 14;
  static const double fleetPanelHeaderInset = 12;
  static const double fleetPanelActionWidth = 76;
  static const double fleetPanelCloseSize = 24;
  static const double fleetPanelAnchorInset = 12;

  /// 复合仪表内部绘制区域直径。
  static const double attitudeCompassContentSize = 188;

  /// 姿态球距离复合仪表内部边缘的内缩。
  static const double attitudeCompassBallInset = 32;

  /// 复合仪表的毛玻璃模糊强度。
  static const double attitudeCompassBlurSigma = 12;

  /// 飞控消息时间列固定宽度，保证所有消息正文从同一位置开始。
  static const double flightMessageTimeWidth = 72;

  /// 复合仪表底部航向读数胶囊尺寸。
  static const double attitudeHeadingReadoutWidth = 58;
  static const double attitudeHeadingReadoutHeight = 20;
  static const double attitudeHeadingReadoutRadius = 10;

  /// 罗盘顶部固定箭头的预留高度。
  static const double compassArrowArea = 16;

  /// 罗盘航向读数与表盘之间的距离。
  static const double compassHeadingGap = 4;

  /// 顶部状态栏指令确认按钮宽度，保证不同命令文案切换时布局不跳动。
  static const double commandConfirmWidth = 188;

  /// 窄窗口下的指令确认按钮宽度：隐藏“长按确认”提示，只保留命令名称。
  static const double commandConfirmCompactWidth = 132;

  /// 地图上方确认按钮离内容区顶边的距离。
  static const double commandConfirmTopInset = 12;

  /// 连接面板中的链路列表行高与可见上限。
  static const double fleetLinkRowHeight = 44;
  static const int fleetLinkVisibleRows = 4;

  /// 多机状态栏：窄窗口保留切机入口，不与关键遥测竞争空间。
  static const double fleetSelectorWidth = 190;
  static const double fleetSelectorCompactWidth = 88;
  static const double fleetSelectorWideBreakpoint = 1180;
  static const double fleetToolbarDetailBreakpoint = 1080;
  static const double fleetToolbarGap = 8;

  /// 电机输出 / 遥控器校准的 PWM 进度条高度。
  ///
  /// PWM 读数就叠在条中间，而 [MgTypography.data] 的行盒是 13 × 1.30 ≈ 17，
  /// 所以这是"读数留在条内"前提下的下限；再矮就得把读数挪到条外。
  static const double motorBarHeight = 18;
}
