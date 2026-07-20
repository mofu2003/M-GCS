import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_tokens.dart';

final class MacGlassMissionEmptyState extends StatelessWidget {
  const MacGlassMissionEmptyState({required this.model, super.key});

  final FrontstageMissionEmptyStateModel model;

  @override
  Widget build(BuildContext context) {
    if (!model.visible) {
      return const SizedBox.shrink();
    }
    return ClipRRect(
      key: const ValueKey('mg-mission-empty-state'),
      borderRadius: BorderRadius.circular(32),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.54),
            border: Border.all(color: Colors.white.withValues(alpha: 0.86)),
            borderRadius: BorderRadius.circular(32),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 560),
            child: Padding(
              padding: const EdgeInsets.all(30),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    model.title,
                    textAlign: TextAlign.center,
                    style: const TextStyle(
                      fontSize: 38,
                      height: 1.05,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const SizedBox(height: 10),
                  Text(
                    model.message,
                    textAlign: TextAlign.center,
                    style: const TextStyle(
                      color: MacGlassTokens.muted,
                      height: 1.6,
                    ),
                  ),
                  const SizedBox(height: 24),
                  const Wrap(
                    alignment: WrapAlignment.center,
                    spacing: 10,
                    runSpacing: 10,
                    children: [
                      _MissionButton(label: '新建任务', primary: true),
                      _MissionButton(label: '导入任务'),
                      _MissionButton(label: '从飞控下载'),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

final class _MissionButton extends StatelessWidget {
  const _MissionButton({required this.label, this.primary = false});

  final String label;
  final bool primary;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: primary
            ? MacGlassTokens.black
            : Colors.white.withValues(alpha: 0.62),
        borderRadius: BorderRadius.circular(19),
        border: Border.all(
          color: primary
              ? Colors.black.withValues(alpha: 0.50)
              : Colors.white.withValues(alpha: 0.78),
        ),
      ),
      child: SizedBox(
        width: 150,
        height: 52,
        child: Center(
          child: Text(
            label,
            textAlign: TextAlign.center,
            style: TextStyle(
              color: primary ? Colors.white : MacGlassTokens.ink,
              fontWeight: FontWeight.w800,
            ),
          ),
        ),
      ),
    );
  }
}
