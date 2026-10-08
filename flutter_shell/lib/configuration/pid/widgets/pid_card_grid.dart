import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../models/pid_models.dart';
import 'pid_card.dart';

/// 卡片按可用宽度自动分栏；卡片内部的字段再按两列排。
class PidCardGrid extends StatelessWidget {
  const PidCardGrid({super.key, required this.controller, required this.cards});

  final ConnectionController controller;
  final List<PidCard> cards;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      if (cards.isEmpty) return const SizedBox.shrink();
      final columns = constraints.maxWidth >= 1180
          ? math.min(4, cards.length)
          : constraints.maxWidth >= 800
          ? math.min(3, cards.length)
          : constraints.maxWidth >= 540
          ? math.min(2, cards.length)
          : 1;
      final width =
          (constraints.maxWidth - (columns - 1) * MgMetrics.configCardGap) /
          columns;
      return Wrap(
        spacing: MgMetrics.configCardGap,
        runSpacing: MgMetrics.configCardGap,
        children: [
          for (final card in cards)
            SizedBox(
              width: width,
              child: PidCardView(card: card, controller: controller),
            ),
        ],
      );
    },
  );
}
