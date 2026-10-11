import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../widgets/page_data_notice.dart';
import '../widgets/parameter_write_status.dart';

/// 一条解锁检查项。位值与 ArduPilot 的 bitmask 定义一致。
class _ArmingCheck {
  const _ArmingCheck(
    this.bit,
    this.label, {
    this.fixedWingOnly = false,
    this.multirotorOnly = false,
  });

  final int bit;
  final String label;
  final bool fixedWingOnly;
  final bool multirotorOnly;
}

/// 解锁检查清单。旧固件写 ARMING_CHECK（置位=执行检查），
/// 新固件写 ARMING_SKIPCHK（置位=跳过检查），两者位值相同、语义相反。
const _armingCheckCatalog = [
  _ArmingCheck(2, '气压计'),
  _ArmingCheck(4, '罗盘'),
  _ArmingCheck(8, 'GPS 锁定'),
  _ArmingCheck(16, 'INS'),
  _ArmingCheck(32, '参数'),
  _ArmingCheck(64, '遥控通道'),
  _ArmingCheck(128, '板载电压'),
  _ArmingCheck(256, '电池电量'),
  _ArmingCheck(512, '空速计', fixedWingOnly: true),
  _ArmingCheck(1024, '日志可用性'),
  _ArmingCheck(2048, '硬件安全开关'),
  _ArmingCheck(4096, 'GPS 配置'),
  _ArmingCheck(8192, '系统'),
  _ArmingCheck(16384, '任务'),
  _ArmingCheck(32768, '测距仪'),
  _ArmingCheck(65536, '相机'),
  _ArmingCheck(131072, 'AuxAuth'),
  _ArmingCheck(262144, '视觉里程计', multirotorOnly: true),
  _ArmingCheck(524288, 'FFT'),
];

List<_ArmingCheck> _armingChecksFor(VehicleControlProfile profile) {
  final fixedWing =
      profile == VehicleControlProfile.fixedWing ||
      profile == VehicleControlProfile.planeVtol;
  final multirotor = profile == VehicleControlProfile.copter;
  return [
    for (final check in _armingCheckCatalog)
      if (!check.fixedWingOnly || fixedWing)
        if (!check.multirotorOnly || multirotor) check,
  ];
}

/// 新固件优先用角色 `arming.skip_check`（ARMING_SKIPCHK），
/// 旧固件回退到 `arming.check`（ARMING_CHECK）。参数名由后端角色表提供
/// （safety.md S6），前端只判断哪个角色在当前固件里真的存在。
String? _armingParameterName(ConnectionController controller) {
  final skip = controller.roleParam('arming.skip_check');
  if (skip != null && controller.parameters.containsKey(skip)) return skip;
  final check = controller.roleParam('arming.check');
  if (check != null && controller.parameters.containsKey(check)) return check;
  return null;
}

/// 解锁检查参数配置页，保留 CHECK / SKIPCHK 的相反位掩码语义。
class ArmingChecksPage extends StatefulWidget {
  const ArmingChecksPage({
    super.key,
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<ArmingChecksPage> createState() => _ArmingChecksPageState();
}

class _ArmingChecksPageState extends State<ArmingChecksPage> {
  Future<void> _write(
    ConnectionController controller,
    String? parameterName,
    bool inverted,
    List<_ArmingCheck> checks,
    bool Function(_ArmingCheck) enabled,
  ) async {
    // 固件没有解锁检查参数时开关是置灰的，这里再兜一层，避免空写入。
    if (parameterName == null) return;
    final current = controller.parameters[parameterName]?.value.round() ?? 0;
    var mask = current;
    for (final check in checks) {
      // 置位含义随参数而变：SKIPCHK 置位表示跳过，CHECK 置位表示执行。
      final bitSet = inverted ? !enabled(check) : enabled(check);
      mask = bitSet ? (mask | check.bit) : (mask & ~check.bit);
    }
    await controller.setParameter(parameterName, mask.toDouble());
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.profileChanges,
    ]),
    builder: (context, _) {
      final controller = widget.controller;
      final parameterName = _armingParameterName(controller);
      // 置位语义随固件而变：SKIPCHK 置位=跳过、CHECK 置位=执行。
      // 命中的究竟是哪个角色，决定了开关是否需要取反。
      final inverted =
          parameterName != null &&
          parameterName == controller.roleParam('arming.skip_check');
      final mask = parameterName == null
          ? 0
          : controller.parameters[parameterName]?.value.round() ?? 0;
      final checks = _armingChecksFor(controller.controlProfile);
      // 拿不到参数（未连接、未读完或固件没有该参数）时开关置灰，但清单照常显示。
      final writable =
          parameterName != null &&
          !controller.isParameterWriting(parameterName);
      bool enabled(_ArmingCheck check) =>
          inverted ? (mask & check.bit) == 0 : (mask & check.bit) != 0;
      final allEnabled = checks.every(enabled);
      final notice = !controller.isConnected
          ? '连接飞控后读取解锁检查项'
          : !controller.parametersLoaded
          ? '正在读取飞控参数…'
          : parameterName == null
          ? '当前固件未提供解锁检查参数（ARMING_SKIPCHK / ARMING_CHECK）'
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
              const SizedBox(height: MgMetrics.configHeaderGap),
            ],
            // 和其它配置页同一观感：整块用带边框的面板框住，内部按行/列打分隔线，
            // 行高取标准开关行高度，比表格行更紧凑。
            DecoratedBox(
              decoration: BoxDecoration(
                color: context.mgColors.surface,
                border: Border.all(color: context.mgColors.divider),
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
              ),
              child: Padding(
                // 垂直方向不留内边距：否则上下多出来的空白会算进首/末行，
                // 让它们比中间行更高。行高本身已经留出了呼吸空间。
                padding: const EdgeInsets.symmetric(horizontal: 4),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    SizedBox(
                      height: MgMetrics.controlHeight,
                      child: _ArmingToggle(
                        label: '全部',
                        value: allEnabled,
                        enabled: writable,
                        onChanged: (value) => unawaited(
                          _write(
                            controller,
                            parameterName,
                            inverted,
                            checks,
                            (_) => value,
                          ),
                        ),
                      ),
                    ),
                    Divider(
                      height: 1,
                      thickness: 1,
                      color: context.mgColors.divider,
                    ),
                    LayoutBuilder(
                      builder: (context, constraints) {
                        final columns = constraints.maxWidth >= 900
                            ? 3
                            : constraints.maxWidth >= 560
                            ? 2
                            : 1;
                        // 按列切行，行间与列间都打分隔线，形成参数表观感。
                        final rows = [
                          for (
                            var start = 0;
                            start < checks.length;
                            start += columns
                          )
                            checks.sublist(
                              start,
                              math.min(start + columns, checks.length),
                            ),
                        ];
                        return Column(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            for (var row = 0; row < rows.length; row++) ...[
                              if (row > 0)
                                Divider(
                                  height: 1,
                                  thickness: 1,
                                  color: context.mgColors.divider,
                                ),
                              SizedBox(
                                height: MgMetrics.controlHeight,
                                child: Row(
                                  crossAxisAlignment:
                                      CrossAxisAlignment.stretch,
                                  children: [
                                    for (
                                      var column = 0;
                                      column < columns;
                                      column++
                                    ) ...[
                                      if (column > 0)
                                        SizedBox(
                                          width: 1,
                                          child: ColoredBox(
                                            color: context.mgColors.divider,
                                          ),
                                        ),
                                      Expanded(
                                        child: column < rows[row].length
                                            ? _ArmingToggle(
                                                label: rows[row][column].label,
                                                value: enabled(
                                                  rows[row][column],
                                                ),
                                                enabled: writable,
                                                onChanged: (value) => unawaited(
                                                  _write(
                                                    controller,
                                                    parameterName,
                                                    inverted,
                                                    checks,
                                                    (item) =>
                                                        item.bit ==
                                                            rows[row][column]
                                                                .bit
                                                        ? value
                                                        : enabled(item),
                                                  ),
                                                ),
                                              )
                                            : const SizedBox.shrink(),
                                      ),
                                    ],
                                  ],
                                ),
                              ),
                            ],
                          ],
                        );
                      },
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      );
    },
  );
}

class _ArmingToggle extends StatelessWidget {
  const _ArmingToggle({
    required this.label,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final String label;
  final bool value;
  final bool enabled;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 8),
    child: Row(
      children: [
        Expanded(
          child: Text(
            label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(context).typography.body,
          ),
        ),
        const SizedBox(width: 8),
        MacosSwitch(
          value: value,
          size: ControlSize.small,
          onChanged: enabled ? onChanged : null,
          semanticLabel: label,
        ),
      ],
    ),
  );
}
