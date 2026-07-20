import 'package:flutter/material.dart';

import '../../../application/connection_models.dart';
import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_tokens.dart';

/// The connection state belongs in the status bar, not as a floating panel.
final class MacGlassConnectionBootstrapBadge extends StatelessWidget {
  const MacGlassConnectionBootstrapBadge({required this.model, super.key});

  final FrontstageConnectionBootstrapModel model;

  @override
  Widget build(BuildContext context) {
    return RepaintBoundary(
      child: SizedBox(
        width: 290,
        height: 50,
        child: _StatusChipSurface(
          child: switch (model.state) {
            ConnectionBootstrapState.downloading => _progressChip(),
            ConnectionBootstrapState.complete => _identityChip(),
            ConnectionBootstrapState.failed => _failureChip(),
            ConnectionBootstrapState.idle => _idleChip(),
          },
        ),
      ),
    );
  }

  Widget _progressChip() {
    final progress = model.progressPercent;
    return Semantics(
      key: const ValueKey('mg-bootstrap-progress'),
      label: progress == null ? '正在请求参数列表' : '正在下载参数 $progress%',
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        child: Row(
          children: [
            SizedBox.square(
              dimension: 28,
              child: CircularProgressIndicator(
                // A determinate fallback keeps this tag stable while the
                // autopilot has not yet reported its parameter total.
                value: progress == null ? 0.12 : progress / 100,
                strokeWidth: 3,
                strokeCap: StrokeCap.round,
                color: MacGlassTokens.green,
                backgroundColor: MacGlassTokens.green.withValues(alpha: 0.16),
              ),
            ),
            const SizedBox(width: 9),
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    '正在下载参数',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      color: MacGlassTokens.muted,
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  Text(
                    progress == null
                        ? '等待飞控返回参数总数'
                        : '${model.parameterLabel}  $progress%',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(
                      color: MacGlassTokens.ink,
                      fontSize: 12,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _idleChip() {
    return Semantics(
      key: const ValueKey('mg-bootstrap-idle'),
      label: '未连接飞控，选择连接方式',
      child: const Padding(
        padding: EdgeInsets.symmetric(horizontal: 10),
        child: Row(
          children: [
            Icon(Icons.link_rounded, size: 15, color: MacGlassTokens.muted),
            SizedBox(width: 7),
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    '未连接飞控',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      color: MacGlassTokens.ink,
                      fontSize: 11,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  Text(
                    '选择连接方式',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      color: MacGlassTokens.muted,
                      fontSize: 10,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _identityChip() {
    return Semantics(
      key: const ValueKey('mg-bootstrap-identity'),
      label:
          '${model.name}，机架类型：${model.airframe}，${model.firmwareLabel}，'
          '${model.linkLabel}，${model.remoteControlLabel}',
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        child: Row(
          children: [
            const Icon(
              Icons.memory_rounded,
              size: 15,
              color: MacGlassTokens.green,
            ),
            const SizedBox(width: 7),
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          model.name,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                            color: MacGlassTokens.ink,
                            fontSize: 12,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                      ),
                      const SizedBox(width: 5),
                      Flexible(
                        child: Text(
                          '机架类型：${model.airframe}',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                            color: MacGlassTokens.muted,
                            fontSize: 10,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 2),
                  Wrap(
                    spacing: 5,
                    children: [
                      _MetricLabel(text: model.firmwareLabel),
                      _MetricLabel(text: model.linkLabel),
                      _MetricLabel(text: model.remoteControlLabel),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _failureChip() {
    return Semantics(
      key: const ValueKey('mg-bootstrap-failed'),
      label: model.errorMessage ?? '参数同步失败',
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        child: Row(
          children: [
            const Icon(Icons.error_outline_rounded, color: MacGlassTokens.red),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                model.errorMessage ?? '参数同步失败',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  color: MacGlassTokens.red,
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

final class _MetricLabel extends StatelessWidget {
  const _MetricLabel({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    return Text(
      text,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: const TextStyle(
        color: MacGlassTokens.muted,
        fontSize: 10,
        fontWeight: FontWeight.w700,
      ),
    );
  }
}

final class _StatusChipSurface extends StatelessWidget {
  const _StatusChipSurface({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.54),
        border: Border.all(color: Colors.white.withValues(alpha: 0.74)),
        borderRadius: BorderRadius.circular(15),
      ),
      child: child,
    );
  }
}
