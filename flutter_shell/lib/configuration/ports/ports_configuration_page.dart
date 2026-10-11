import 'dart:async';

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/aligned_field_row.dart';

import '../widgets/page_data_notice.dart';
import '../widgets/parameter_summary_row.dart';
import '../widgets/parameter_value_control.dart';
import '../widgets/parameter_write_status.dart';

/// 飞控串口参数配置页，保留硬件映射与字体署名。
class PortsConfigurationPage extends StatefulWidget {
  const PortsConfigurationPage({
    super.key,
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<PortsConfigurationPage> createState() => _PortConfigurationPageState();
}

class _PortConfigurationPageState extends State<PortsConfigurationPage> {
  @override
  void initState() {
    super.initState();
    if (widget.controller.isConnected) {
      unawaited(widget.controller.refreshSerialHardwareMappings());
    }
    widget.controller.loadParameterMetadata([
      for (var port = 0; port <= 15; port++) ...[
        'SERIAL${port}_PROTOCOL',
        'SERIAL${port}_BAUD',
      ],
      for (var port = 1; port <= 4; port++) ...[
        'SER_TEL${port}_PROT',
        'SER_TEL${port}_BAUD',
      ],
      for (var port = 1; port <= 2; port++) ...[
        'SER_GPS${port}_PROT',
        'SER_GPS${port}_BAUD',
      ],
    ]);
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.parameterChanges,
    builder: (context, _) {
      final controller = widget.controller;
      final serialPorts = controller.serialParameterPorts;
      final theme = MacosTheme.of(context);
      // 参数还没到时不空着：先用标准 SERIAL0..7 渲染骨架，数值是「—」、控件置灰。
      final ports = serialPorts.isEmpty
          ? [for (var port = 0; port <= 7; port++) 'SERIAL$port']
          : serialPorts;
      final notice = !controller.isConnected
          ? '连接飞控后读取串口配置'
          : !controller.parametersLoaded
          ? '正在读取飞控参数…'
          : serialPorts.isEmpty
          ? '飞控未返回串口配置参数，以下为占位'
          : null;
      return SingleChildScrollView(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(
          MgMetrics.configPageMargin,
          22,
          MgMetrics.configPageMargin,
          32,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (notice != null)
              PageDataNotice(
                controller: controller,
                message: notice,
                showProgress:
                    controller.isConnected && !controller.parametersLoaded,
              ),
            if (controller.parameterWriteError != null) ...[
              ParameterWriteStatus(controller: controller),
              const SizedBox(height: 10),
            ],
            if (controller.serialHardwareMappingLoading)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '正在读取飞控硬件串口映射…',
                  style: theme.typography.caption1.copyWith(
                    color: context.mgColors.textSecondary,
                  ),
                ),
              )
            else if (controller.serialHardwareMappingError case final error?)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '飞控未提供硬件串口映射：$error',
                  style: theme.typography.caption1.copyWith(
                    color: context.mgColors.textSecondary,
                  ),
                ),
              ),
            LayoutBuilder(
              builder: (context, constraints) {
                final columns = constraints.maxWidth >= 900 ? 2 : 1;
                final cardWidth =
                    (constraints.maxWidth - (columns - 1) * 14) / columns;
                return Wrap(
                  spacing: 14,
                  runSpacing: 14,
                  children: [
                    for (final port in ports)
                      SizedBox(
                        width: cardWidth,
                        child: _SerialParameterCard(
                          serialPrefix: port,
                          controller: controller,
                          dividerColor: context.mgColors.divider,
                          hardwareName: controller.serialHardwareMappings[port],
                        ),
                      ),
                  ],
                );
              },
            ),
            const SizedBox(height: MgMetrics.configSectionGap),
            // MiSans 许可要求在软件中特别注明使用了该字体，这里给出署名。
            Text(
              '界面字体：MiSans（小米科技有限责任公司授权）',
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: context.mgColors.textTertiary,
              ),
            ),
          ],
        ),
      );
    },
  );
}

class _SerialParameterCard extends StatefulWidget {
  const _SerialParameterCard({
    required this.serialPrefix,
    required this.controller,
    required this.dividerColor,
    required this.hardwareName,
  });

  final String serialPrefix;
  final ConnectionController controller;
  final Color dividerColor;
  final String? hardwareName;

  @override
  State<_SerialParameterCard> createState() => _SerialParameterCardState();
}

class _SerialParameterCardState extends State<_SerialParameterCard> {
  bool _showOptions = false;

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final theme = MacosTheme.of(context);
    final serial = widget.serialPrefix;
    final extraParameters = controller.parameters.values
        .where(
          (parameter) =>
              parameter.name.startsWith('${serial}_') &&
              parameter.name != '${serial}_PROTOCOL' &&
              parameter.name != '${serial}_PROT' &&
              parameter.name != '${serial}_BAUD',
        )
        .toList(growable: false);
    final protocol =
        controller.parameters['${serial}_PROTOCOL'] ??
        controller.parameters['${serial}_PROT'];
    final baud = controller.parameters['${serial}_BAUD'];

    return DecoratedBox(
      decoration: BoxDecoration(
        color: context.mgColors.surfaceMuted,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: widget.dividerColor),
        boxShadow: [
          BoxShadow(
            color: context.mgColors.shadow,
            blurRadius: 10,
            offset: const Offset(0, 3),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Text(
                  serial,
                  style: MgTypography.sans(MgTypography.section),
                ),
                if (widget.hardwareName case final hardwareName?) ...[
                  const SizedBox(width: 10),
                  Flexible(
                    child: Text(
                      hardwareName,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.body.copyWith(
                          color: context.mgColors.accent,
                        ),
                      ),
                    ),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 10),
            LayoutBuilder(
              builder: (context, constraints) {
                // 并列控件使用同一套网格：标签列等宽、控件等宽等高，
                // 两列的上边缘与下边缘严格对齐。
                const labelWidth = 52.0;
                const columnGap = 14.0;
                final compact = constraints.maxWidth < 560;
                final fields = [
                  if (protocol != null)
                    AlignedFieldRow(
                      label: '协议',
                      labelWidth: labelWidth,
                      child: ParameterValueControl(
                        parameter: protocol,
                        controller: controller,
                        // 填满列宽，保证并列下拉框左右边缘严格对齐。
                        popupWidth: double.infinity,
                      ),
                    ),
                  if (baud != null)
                    AlignedFieldRow(
                      label: '波特率',
                      labelWidth: labelWidth,
                      child: ParameterValueControl(
                        parameter: baud,
                        controller: controller,
                        popupWidth: double.infinity,
                      ),
                    ),
                ];
                if (compact) {
                  return Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      for (var index = 0; index < fields.length; index++) ...[
                        if (index > 0) const SizedBox(height: 12),
                        fields[index],
                      ],
                    ],
                  );
                }
                return Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    for (var index = 0; index < fields.length; index++) ...[
                      if (index > 0) const SizedBox(width: columnGap),
                      Expanded(flex: 1, child: fields[index]),
                    ],
                  ],
                );
              },
            ),
            if (protocol == null && baud == null)
              Text(
                '该逻辑串口没有可读取的协议或波特率参数。',
                style: theme.typography.caption1.copyWith(
                  color: context.mgColors.textSecondary,
                ),
              ),
            if (extraParameters.isNotEmpty) ...[
              const SizedBox(height: 8),
              Divider(height: 1, color: context.mgColors.divider),
              Row(
                children: [
                  MacosDisclosureButton(
                    isPressed: _showOptions,
                    onPressed: () =>
                        setState(() => _showOptions = !_showOptions),
                  ),
                  const SizedBox(width: 4),
                  Flexible(
                    child: Text(
                      '其他串口参数',
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.typography.callout,
                    ),
                  ),
                  const SizedBox(width: 7),
                  Text(
                    '${extraParameters.length}',
                    style: theme.typography.caption1,
                  ),
                ],
              ),
              if (_showOptions)
                for (final parameter in extraParameters)
                  Padding(
                    padding: const EdgeInsets.only(left: 18, top: 4),
                    child: ParameterSummaryRow(
                      parameter: parameter,
                      controller: controller,
                    ),
                  ),
            ],
          ],
        ),
      ),
    );
  }
}
