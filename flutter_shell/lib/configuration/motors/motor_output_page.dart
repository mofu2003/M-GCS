import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../widgets/page_data_notice.dart';
import '../widgets/parameter_helpers.dart';
import '../widgets/parameter_write_status.dart';
import 'widgets/motor_output_cells.dart';

/// 电机输出参数表，显示实时 PWM 并提供参数编辑。
class MotorOutputPage extends StatefulWidget {
  const MotorOutputPage({
    super.key,
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<MotorOutputPage> createState() => _MotorOutputPageState();
}

class _MotorOutputPageState extends State<MotorOutputPage> {
  /// 表格能排开所有固定列所需的最小宽度；窄于它就横向滚动。
  static const double _motorTableMinWidth = 1040;

  static const _headers = [
    '序号',
    'PWM 值',
    '反向',
    '功能',
    'PWM 最小值',
    'PWM 中位值',
    'PWM 最大值',
  ];

  @override
  void initState() {
    super.initState();
    widget.controller.loadParameterMetadata([
      for (var channel = 1; channel <= 16; channel++) ...[
        'SERVO${channel}_FUNCTION',
        'SERVO${channel}_REVERSED',
        'SERVO${channel}_MIN',
        'SERVO${channel}_TRIM',
        'SERVO${channel}_MAX',
      ],
    ]);
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.telemetryChanges,
    ]),
    builder: (context, _) {
      final controller = widget.controller;
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
            // 没有数据也照常显示整张表，数值是「—」，只用状态条说明原因。
            if (!controller.isConnected)
              PageDataNotice(controller: controller, message: '连接飞控后读取电机输出参数')
            else if (!controller.parametersLoaded)
              PageDataNotice(
                controller: controller,
                message: '正在读取飞控参数…',
                showProgress: true,
              ),
            if (controller.parameterWriteError != null) ...[
              ParameterWriteStatus(controller: controller),
              const SizedBox(height: 10),
            ],
            DecoratedBox(
              decoration: BoxDecoration(
                color: context.mgColors.surface,
                border: Border.all(color: context.mgColors.divider),
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
                // 表格宽度跟着面板走：面板比 1040 宽就铺满，窄于 1040 才横向滚动。
                // 之前子项被写死成"至少 1040"，窗口变宽时表格不跟着变宽，右侧留白。
                child: LayoutBuilder(
                  builder: (context, constraints) => SingleChildScrollView(
                    scrollDirection: Axis.horizontal,
                    child: SizedBox(
                      width: math.max(
                        _motorTableMinWidth,
                        constraints.maxWidth,
                      ),
                      child: Table(
                        columnWidths: const {
                          0: FixedColumnWidth(76),
                          1: FixedColumnWidth(220),
                          2: FixedColumnWidth(90),
                          3: FlexColumnWidth(1.5),
                          4: FixedColumnWidth(132),
                          5: FixedColumnWidth(132),
                          6: FixedColumnWidth(132),
                        },
                        children: [
                          TableRow(
                            decoration: BoxDecoration(
                              color: context.mgColors.accentSurface,
                            ),
                            children: [
                              for (final header in _headers)
                                HeaderCell(label: header),
                            ],
                          ),
                          for (var channel = 1; channel <= 16; channel++)
                            TableRow(
                              decoration: BoxDecoration(
                                border: Border(
                                  bottom: BorderSide(
                                    color: context.mgColors.divider,
                                    width: 0.6,
                                  ),
                                ),
                              ),
                              children: [
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: ChannelNumberCell(number: channel),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: LiveOutputCell(
                                    value:
                                        widget.controller.servoOutputsValid &&
                                            channel <=
                                                widget
                                                    .controller
                                                    .servoOutputs
                                                    .length
                                        ? widget
                                              .controller
                                              .servoOutputs[channel - 1]
                                        : null,
                                    minimum: parameterNumeric(
                                      widget.controller,
                                      'SERVO${channel}_MIN',
                                      fallback: 1100,
                                    ),
                                    maximum: parameterNumeric(
                                      widget.controller,
                                      'SERVO${channel}_MAX',
                                      fallback: 1900,
                                    ),
                                  ),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: ReverseCell(
                                    parameter: widget
                                        .controller
                                        .parameters['SERVO${channel}_REVERSED'],
                                    controller: widget.controller,
                                  ),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: FunctionCell(
                                    parameter: widget
                                        .controller
                                        .parameters['SERVO${channel}_FUNCTION'],
                                    controller: widget.controller,
                                  ),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: LimitCell(
                                    parameter: widget
                                        .controller
                                        .parameters['SERVO${channel}_MIN'],
                                    tone: LimitTone.min,
                                    controller: widget.controller,
                                  ),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: LimitCell(
                                    parameter: widget
                                        .controller
                                        .parameters['SERVO${channel}_TRIM'],
                                    tone: LimitTone.mid,
                                    controller: widget.controller,
                                  ),
                                ),
                                TableCell(
                                  verticalAlignment:
                                      TableCellVerticalAlignment.middle,
                                  child: LimitCell(
                                    parameter: widget
                                        .controller
                                        .parameters['SERVO${channel}_MAX'],
                                    tone: LimitTone.max,
                                    controller: widget.controller,
                                  ),
                                ),
                              ],
                            ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      );
    },
  );
}
