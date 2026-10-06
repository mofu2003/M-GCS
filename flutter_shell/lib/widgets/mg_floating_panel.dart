import 'dart:ui';

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 统一的悬浮面板容器：圆角 + 毛玻璃背景 + 细边框 + 投影。
///
/// 所有浮在地图或窗口之上的面板（顶部状态栏、地图工具列、飞行动作条、遥测条、
/// 设置菜单、连接/消息/航线规划浮层）都套这一层，保证三件事全局只有一处定义：
///
/// - 底色：`MgColors.floatingSurface`（65% 不透明）
/// - 模糊：[blurSigma]（默认 18，地图细节被抹平但仍看得出底下是什么）
/// - 投影与圆角：`MgMetrics.panelRadius`
///
/// 单独给某个面板改透明度或模糊，就会重新出现"这个面板看起来和别人不一样"
/// 的问题——要调就调这里。
class MgFloatingPanel extends StatelessWidget {
  const MgFloatingPanel({
    super.key,
    required this.child,
    this.borderRadius,
    this.border,
    this.showShadow = true,
    this.blurSigma = defaultBlurSigma,
  });

  /// 毛玻璃模糊强度。18 大致是 macOS vibrancy 的量级。
  static const double defaultBlurSigma = 18;

  final Widget child;

  /// 默认 [MgMetrics.panelRadius]；通栏（如顶部状态栏）传 `BorderRadius.zero`。
  final BorderRadius? borderRadius;

  /// 默认四周 1px 分隔线；通栏可以只留一条下边线。
  final BoxBorder? border;

  final bool showShadow;
  final double blurSigma;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final radius = borderRadius ?? BorderRadius.circular(MgMetrics.panelRadius);
    return DecoratedBox(
      // 投影留在裁剪层之外，否则会被 ClipRRect 剪掉。
      decoration: BoxDecoration(
        borderRadius: radius,
        boxShadow: showShadow
            ? [
                BoxShadow(
                  color: colors.shadow,
                  blurRadius: 18,
                  offset: const Offset(0, 5),
                ),
              ]
            : null,
      ),
      child: ClipRRect(
        borderRadius: radius,
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: blurSigma, sigmaY: blurSigma),
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: colors.floatingSurface,
              borderRadius: radius,
              border: border ?? Border.all(color: colors.divider),
            ),
            child: child,
          ),
        ),
      ),
    );
  }
}
