import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_connection_bootstrap_badge.dart';
import 'mac_glass_tokens.dart';

final class MacGlassStatusBar extends StatelessWidget {
  const MacGlassStatusBar({
    required this.model,
    required this.bootstrap,
    required this.outputPanelOpen,
    required this.onOutputPressed,
    super.key,
  });

  final FrontstageModel model;
  final FrontstageConnectionBootstrapModel bootstrap;
  final bool outputPanelOpen;
  final VoidCallback onOutputPressed;

  @override
  Widget build(BuildContext context) {
    final statusChips = <Widget>[
      for (var index = 0; index < model.status.items.length; index++) ...[
        _StatusChip(
          item: model.status.items[index],
          selected: model.status.items[index].id == 'output' && outputPanelOpen,
          onPressed: model.status.items[index].id == 'output'
              ? onOutputPressed
              : null,
        ),
        if (index < model.status.items.length - 1) const SizedBox(width: 10),
      ],
    ];
    const statusChipWidth = 132.0;
    const statusChipGap = 10.0;
    const bootstrapWidth = 290.0;
    final statusWidth =
        model.status.items.length * statusChipWidth +
        math.max(0, model.status.items.length - 1) * statusChipGap;
    final minimumContentWidth =
        statusWidth + (bootstrap.visible ? bootstrapWidth + statusChipGap : 0);

    return ClipRRect(
      key: const ValueKey('mg-top-status-bar'),
      borderRadius: BorderRadius.circular(MacGlassTokens.radiusPanel),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: MacGlassTokens.glass,
            border: Border.all(color: Colors.white.withValues(alpha: 0.86)),
            borderRadius: BorderRadius.circular(MacGlassTokens.radiusPanel),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: SizedBox(
            height: MacGlassTokens.topBarHeight,
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 18),
              child: LayoutBuilder(
                builder: (context, constraints) {
                  return SingleChildScrollView(
                    scrollDirection: Axis.horizontal,
                    child: SizedBox(
                      width: math.max(
                        constraints.maxWidth,
                        minimumContentWidth,
                      ),
                      child: Row(
                        children: [
                          if (bootstrap.visible) ...[
                            MacGlassConnectionBootstrapBadge(model: bootstrap),
                            const SizedBox(width: statusChipGap),
                          ],
                          const Spacer(),
                          ...statusChips,
                        ],
                      ),
                    ),
                  );
                },
              ),
            ),
          ),
        ),
      ),
    );
  }
}

final class _StatusChip extends StatelessWidget {
  const _StatusChip({
    required this.item,
    required this.selected,
    this.onPressed,
  });

  final FrontstageStatusItem item;
  final bool selected;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final chip = SizedBox(
      width: 132,
      height: 50,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: selected
              ? MacGlassTokens.black.withValues(alpha: 0.10)
              : Colors.white.withValues(alpha: 0.54),
          border: Border.all(
            color: selected
                ? MacGlassTokens.black.withValues(alpha: 0.18)
                : Colors.white.withValues(alpha: 0.74),
          ),
          borderRadius: BorderRadius.circular(15),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 10),
          child: Row(
            children: [
              _ToneDot(tone: item.tone),
              const SizedBox(width: 8),
              Expanded(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      item.label,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                        color: MacGlassTokens.muted,
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                    if (item.id == 'battery')
                      _BatteryStatusValue(
                        voltage: item.value,
                        capacity: item.detail ?? '--%',
                      )
                    else
                      Text(
                        item.value,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(
                          fontSize: 13,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );

    final message = item.detail == null
        ? '${item.label}：${item.value}'
        : '${item.label}：${item.value}，${item.detail}';
    if (onPressed == null) {
      return Tooltip(message: message, child: chip);
    }
    return Tooltip(
      message: message,
      child: MouseRegion(
        cursor: SystemMouseCursors.click,
        child: GestureDetector(
          key: const ValueKey('mg-output-status-chip'),
          behavior: HitTestBehavior.opaque,
          onTap: onPressed,
          child: chip,
        ),
      ),
    );
  }
}

final class _BatteryStatusValue extends StatelessWidget {
  const _BatteryStatusValue({required this.voltage, required this.capacity});

  final String voltage;
  final String capacity;

  @override
  Widget build(BuildContext context) {
    final known = capacity != '--%';
    return Row(
      key: const ValueKey('mg-battery-capacity'),
      children: [
        Expanded(
          child: Text(
            voltage,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
          ),
        ),
        Icon(
          known ? Icons.battery_5_bar_rounded : Icons.battery_unknown_rounded,
          size: 14,
          color: known ? MacGlassTokens.green : MacGlassTokens.muted,
        ),
        const SizedBox(width: 2),
        Text(
          capacity,
          style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w800),
        ),
      ],
    );
  }
}

final class _ToneDot extends StatelessWidget {
  const _ToneDot({required this.tone});

  final FrontstageStatusTone tone;

  @override
  Widget build(BuildContext context) {
    final color = switch (tone) {
      FrontstageStatusTone.good => MacGlassTokens.green,
      FrontstageStatusTone.warning => MacGlassTokens.amber,
      FrontstageStatusTone.danger => MacGlassTokens.red,
      FrontstageStatusTone.neutral => MacGlassTokens.blue,
    };
    return DecoratedBox(
      decoration: BoxDecoration(
        color: color,
        shape: BoxShape.circle,
        boxShadow: [
          BoxShadow(color: color.withValues(alpha: 0.16), spreadRadius: 5),
        ],
      ),
      child: const SizedBox(width: 8, height: 8),
    );
  }
}
