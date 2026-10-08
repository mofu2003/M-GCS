import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_floating_panel.dart';

/// QGC `GuidedActionsController.qml` 里 `show*` 条件的等价实现。
///
/// 抽成纯函数是为了能直接对门控做单元测试——地图菜单最容易出错的地方就是
/// “什么状态下该出现哪一项”。
({
  bool gotoLocation,
  bool orbit,
  bool roi,
  bool setHome,
  bool setEstimatorOrigin,
  bool changeHeading,
})
mapActionVisibility({
  required bool guidedEnabled,
  required bool flying,
  required bool homeReady,
  required bool missionActive,
  required bool hasGpsSensor,
  required bool supportsChangeHeading,
}) => (
  gotoLocation: guidedEnabled && flying,
  orbit: guidedEnabled && flying && homeReady && !missionActive,
  roi: guidedEnabled && flying,
  setHome: guidedEnabled,
  // QGC 只在没有 GPS 传感器时才给室内定位用的估计原点入口。
  setEstimatorOrigin: !hasGpsSensor,
  changeHeading: guidedEnabled && flying && supportsChangeHeading,
);

/// 地图点击菜单里的一项。
@immutable
class MgMapAction {
  const MgMapAction({required this.label, required this.onSelected});

  final String label;
  final VoidCallback onSelected;
}

/// QGC 地图点击浮层（`FlyViewMap.qml` 的 `mapClickDropPanel`）。
///
/// 一个贴着点击位置的浮动卡片：可用动作逐个铺满一行按钮，底部显示点击处的
/// 经纬度。显示哪些动作由调用方按 QGC `GuidedActionsController.qml` 的条件算好，
/// 这里只负责排版。
class MgMapActionMenu extends StatelessWidget {
  const MgMapActionMenu({
    super.key,
    required this.latitude,
    required this.longitude,
    required this.actions,
  });

  /// 菜单宽度，也是地图里做边界收拢时用的估值。
  static const double width = 208;

  /// 每项按钮高度。
  static const double rowHeight = 34;

  final double latitude;
  final double longitude;
  final List<MgMapAction> actions;

  /// 菜单大致高度，供调用方把浮层收在地图范围内。
  static double heightFor(int actionCount) =>
      actionCount * rowHeight + 52 /* 经纬度两行 + 内边距 */;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return SizedBox(
      width: width,
      child: MgFloatingPanel(
        child: Material(
          color: colors.transparent,
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 4),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (final action in actions) _MapActionRow(action: action),
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 6, 12, 4),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        '纬度 ${latitude.toStringAsFixed(6)}',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                      Text(
                        '经度 ${longitude.toStringAsFixed(6)}',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                    ],
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

class _MapActionRow extends StatelessWidget {
  const _MapActionRow({required this.action});

  final MgMapAction action;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Material(
      color: colors.transparent,
      child: InkWell(
        onTap: action.onSelected,
        hoverColor: colors.mapControlHover,
        child: SizedBox(
          height: MgMapActionMenu.rowHeight,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            child: Align(
              alignment: Alignment.centerLeft,
              child: Text(
                action.label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textPrimary),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
