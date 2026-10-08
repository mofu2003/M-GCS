import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

/// 磁力计校准进度条：识别到几个罗盘就画几条。
///
/// 进度来自飞控的 `MAG_CAL_PROGRESS`（msg 191）：后端按 compass_id 分开记录
/// （`CalibrationService::process_mag_cal_progress` → `status_.compass_progress`），
/// 前端只负责显示，不自己推算。Mission Planner 的 ConfigHWCompass 同样是
/// 按 compass_id 各画一条（horizontalProgressBar1/2/3），这里保持一致。
class CalibrationProgress extends StatelessWidget {
  const CalibrationProgress({
    super.key,
    required this.instances,
    required this.progressByCompass,
    required this.fallbackProgress,
  });

  /// 已识别到的罗盘实例，决定画几条。
  final List<WaveformInstance> instances;

  /// compass_id → completion_pct。还没收到某个罗盘的进度时按 0 处理。
  final Map<int, int> progressByCompass;

  /// 一个罗盘都还没识别出来时，退化成一条整体进度。
  final int fallbackProgress;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final caption = MgTypography.sans(
      MgTypography.caption.copyWith(color: colors.textSecondary),
    );
    final bars = instances.isEmpty
        ? [(label: '磁力计', progress: fallbackProgress)]
        : [
            for (final instance in instances)
              (
                label: instance.name.isEmpty
                    ? '罗盘 ${instance.index + 1}'
                    : '罗盘 ${instance.index + 1} · ${instance.name}',
                progress: progressByCompass[instance.index] ?? 0,
              ),
          ];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text('磁力计校准中，请绕各轴缓慢转动', maxLines: 1, style: caption),
        const SizedBox(height: 6),
        for (var index = 0; index < bars.length; index++) ...[
          if (index > 0) const SizedBox(height: 6),
          Row(
            children: [
              Expanded(
                child: Text(
                  bars[index].label,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: caption,
                ),
              ),
              const SizedBox(width: 10),
              Text('${bars[index].progress.clamp(0, 100)}%', style: caption),
            ],
          ),
          const SizedBox(height: 3),
          ProgressBar(
            value: bars[index].progress.clamp(0, 100).toDouble(),
            semanticLabel:
                '${bars[index].label} 校准进度 '
                '${bars[index].progress.clamp(0, 100)}%',
          ),
        ],
      ],
    );
  }
}
