import 'package:flutter/material.dart';

import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

/// 飞控加速度计校准步骤对应的摆放方向。
enum AccelOrientation {
  level('水平放置'),
  left('左侧朝下'),
  right('右侧朝下'),
  noseDown('机头朝下'),
  noseUp('机头朝上'),
  inverted('倒放');

  const AccelOrientation(this.label);

  final String label;

  static AccelOrientation fromStep(int step) => switch (step) {
    2 => left,
    3 => right,
    4 => noseDown,
    5 => noseUp,
    6 => inverted,
    _ => level,
  };
}

/// 加速度计校准步骤的摆放方向提示。
class CalibrationAircraft extends StatelessWidget {
  const CalibrationAircraft({super.key, required this.orientation});

  final AccelOrientation? orientation;

  @override
  Widget build(BuildContext context) {
    // 统一走色板：边框和文字取强调色，底色取其淡化的同色。
    //
    // 不用 MacosTheme.primaryColor：它跟随窗口焦点变化，窗口不是主窗口时
    // 会退化成灰色，图框一失焦就变色。
    final colors = context.mgColors;
    final label = orientation?.label ?? '等待开始';
    return Container(
      width: 142,
      height: 128,
      decoration: BoxDecoration(
        color: colors.accentSurface,
        border: Border.all(color: colors.accent, width: 2),
        borderRadius: BorderRadius.circular(14),
      ),
      child: Center(
        child: Text(
          label,
          textAlign: TextAlign.center,
          style: MgTypography.sans(
            MgTypography.title.copyWith(
              color: colors.accent,
              fontWeight: FontWeight.w700,
            ),
          ),
        ),
      ),
    );
  }
}
