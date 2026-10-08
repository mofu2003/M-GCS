import 'package:mgcs_frontend/connection/connection_controller.dart';

/// 枚举参数的选项：优先用飞控描述库给出的 options，取不到时用调用方兜底。
///
/// Mission Planner 的罗盘朝向/容差下拉也是这么取的
/// （`ParameterMetaDataRepository.GetParameterOptionsInt("COMPASS_ORIENT" | "COMPASS_CAL_FIT", ...)`），
/// 不自己维护一份枚举表。
List<int> enumOptionsFor(VehicleParameter? parameter, List<int> fallback) {
  final options =
      parameter?.options.map((option) => option.value.round()).toList() ??
      const <int>[];
  return options.isEmpty ? fallback : options;
}

String enumLabelFor(VehicleParameter? parameter, int value) {
  for (final option in parameter?.options ?? const <ParameterOption>[]) {
    if (option.value.round() == value) return option.label;
  }
  return '$value';
}

/// 判断某个遥控通道当前是否处在"高档"，即该档位的选项已经被拨起来。
///
/// 只有高档算亮：中档和低档都不亮。阈值取自这个通道自己的 `RCn_TRIM` /
/// `RCn_MAX`，不用固定数字，重做行程校准后自动跟着变。
bool rcSwitchActive(ConnectionController controller, int channel) {
  if (!controller.rcChannelsValid) return false;
  if (channel < 1 || channel > controller.rcChannels.length) return false;
  final pwm = controller.rcChannels[channel - 1];
  final trim = parameterNumeric(
    controller,
    'RC${channel}_TRIM',
    fallback: 1500,
  );
  final maximum = parameterNumeric(
    controller,
    'RC${channel}_MAX',
    fallback: 1900,
  );
  return pwm > (trim + maximum) / 2;
}

double parameterNumeric(
  ConnectionController controller,
  String name, {
  required double fallback,
}) => controller.parameter(name)?.value ?? fallback;
String formatParameterValue(double value) =>
    value.isFinite && value == value.roundToDouble()
    ? value.toInt().toString()
    : value.toString();
