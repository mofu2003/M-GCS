import 'dart:async';

import 'package:flutter/material.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/adaptive_popup_button.dart';

import 'parameter_helpers.dart';
import 'parameter_value_well.dart';

/// 参数值编辑入口，按描述选择枚举或数值编辑并保留写入门控。
class ParameterValueControl extends StatelessWidget {
  const ParameterValueControl({
    super.key,
    required this.parameter,
    required this.controller,
    this.center = false,
    this.popupWidth = 132,
  });

  final VehicleParameter? parameter;
  final ConnectionController controller;
  final bool center;
  final double popupWidth;

  @override
  Widget build(BuildContext context) {
    final current = parameter;
    if (current == null || !current.value.isFinite) {
      return ParameterValueWell(text: '—', enabled: false, center: center);
    }
    final matchingOptions = current.options.any(
      (option) => option.value == current.value,
    );
    final options = current.options.length > 1
        ? [
            ...current.options,
            if (!matchingOptions)
              ParameterOption(
                value: current.value,
                label: '未收录（${formatParameterValue(current.value)}）',
              ),
          ]
        : const <ParameterOption>[];
    if (options.length > 1) {
      final labels = {for (final option in options) option.value: option.label};
      return AdaptivePopupButton<double>(
        value: current.value,
        options: options.map((option) => option.value).toList(growable: false),
        labelOf: (value) => labels[value] ?? formatParameterValue(value),
        maxContentWidth: popupWidth,
        onChanged: !controller.isConnected || current.readOnly
            ? null
            : (value) =>
                  unawaited(controller.setParameter(current.name, value)),
      );
    }
    return ParameterValueWell(
      text: formatParameterValue(current.value),
      enabled: controller.isConnected && !current.readOnly,
      center: center,
      onSubmitted: (value) => controller.setParameter(
        current.name,
        _clampToDescriptor(current, value),
      ),
    );
  }

  double _clampToDescriptor(VehicleParameter parameter, double value) {
    var clamped = value;
    final minimum = parameter.minimum;
    final maximum = parameter.maximum;
    if (minimum != null && clamped < minimum) clamped = minimum;
    if (maximum != null && clamped > maximum) clamped = maximum;
    return clamped;
  }
}
