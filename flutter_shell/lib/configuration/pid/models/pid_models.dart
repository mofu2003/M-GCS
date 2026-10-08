import 'package:mgcs_frontend/connection/connection_controller.dart';

/// 调参层级。基础档只放自稳 P 和速率 P/I/D；扩展档补齐前馈、滤波、限幅
/// 以及位置环，避免一屏同时铺开四十多个输入框。
enum PidLevel { basic, extended }

/// PID 字段的显示标签与飞控参数名。
class PidGainField {
  const PidGainField(this.label, this.parameterName);

  final String label;
  final String parameterName;
}

/// PID 参数卡片的标题与字段配置。
class PidCard {
  const PidCard(this.title, this.fields);

  final String title;
  final List<PidGainField> fields;
}

/// PID 参数区块的标题与卡片配置。
class PidSection {
  const PidSection(this.title, this.cards);

  final String title;
  final List<PidCard> cards;
}

const _pidAxes = [('横滚', 'RLL'), ('俯仰', 'PIT'), ('偏航', 'YAW')];

/// 三轴速率卡：基础档给 P/I/D，扩展档给前馈、积分上限与三档滤波。
PidCard _pidRateCard(String title, String base, PidLevel level) =>
    switch (level) {
      PidLevel.basic => PidCard(title, [
        PidGainField('比例 (P)', '${base}_P'),
        PidGainField('积分 (I)', '${base}_I'),
        PidGainField('微分 (D)', '${base}_D'),
      ]),
      PidLevel.extended => PidCard(title, [
        PidGainField('前馈 (FF)', '${base}_FF'),
        PidGainField('积分上限 (I Max)', '${base}_IMAX'),
        PidGainField('误差滤波 (FLTE)', '${base}_FLTE'),
        PidGainField('微分滤波 (FLTD)', '${base}_FLTD'),
        PidGainField('目标滤波 (FLTT)', '${base}_FLTT'),
      ]),
    };

List<PidSection> pidSections(
  VehicleControlProfile profile,
  bool vtol,
  PidLevel level,
) {
  // 垂起的多旋翼部分与多旋翼一一对应，只是把 ATC_/PSC_ 换成 Q_A_/Q_P_；
  // 参数名已在 ArduPilot 4.8 参数库里逐条核对过。
  if (vtol || profile == VehicleControlProfile.copter) {
    return _multirotorPidSections(vtol, level);
  }
  if (profile == VehicleControlProfile.fixedWing ||
      profile == VehicleControlProfile.planeVtol) {
    return _fixedWingPidSections(level);
  }
  return const [];
}

List<PidSection> _multirotorPidSections(bool vtol, PidLevel level) {
  final angle = vtol ? 'Q_A_ANG' : 'ATC_ANG';
  final rate = vtol ? 'Q_A_RAT' : 'ATC_RAT';
  final accel = vtol ? 'Q_A_ACC' : 'ATC_ACC';
  final rateMax = vtol ? 'Q_A_RATE' : 'ATC_RATE';
  final attitude = vtol ? 'Q_A' : 'ATC';
  final psc = vtol ? 'Q_P' : 'PSC';

  if (level == PidLevel.basic) {
    return [
      PidSection('自稳 P', [
        for (final (label, axis) in _pidAxes)
          PidCard('自稳 $label', [PidGainField('比例 (P)', '${angle}_${axis}_P')]),
      ]),
      PidSection('速率 PID', [
        for (final (label, axis) in _pidAxes)
          _pidRateCard('$label速率', '${rate}_$axis', level),
      ]),
    ];
  }
  return [
    PidSection('速率滤波与前馈', [
      for (final (label, axis) in _pidAxes)
        _pidRateCard('$label速率', '${rate}_$axis', level),
    ]),
    PidSection('响应限制', [
      PidCard('最大角加速度 (ACCEL MAX)', [
        PidGainField('横滚', '${accel}_R_MAX'),
        PidGainField('俯仰', '${accel}_P_MAX'),
        PidGainField('偏航', '${accel}_Y_MAX'),
      ]),
      PidCard('最大角速度 (RATE MAX)', [
        PidGainField('横滚', '${rateMax}_R_MAX'),
        PidGainField('俯仰', '${rateMax}_P_MAX'),
        PidGainField('偏航', '${rateMax}_Y_MAX'),
      ]),
      PidCard('姿态平滑', [
        PidGainField('输入时间常数 (INPUT TC)', '${attitude}_INPUT_TC'),
        PidGainField('角度上限 (ANGLE MAX)', '${attitude}_ANGLE_MAX'),
      ]),
    ]),
    PidSection('位置与速度', [
      PidCard('水平位置', [PidGainField('位置 P', '${psc}_NE_POS_P')]),
      PidCard('水平速度', [
        PidGainField('比例 (P)', '${psc}_NE_VEL_P'),
        PidGainField('积分 (I)', '${psc}_NE_VEL_I'),
        PidGainField('微分 (D)', '${psc}_NE_VEL_D'),
        PidGainField('积分上限 (I Max)', '${psc}_NE_VEL_IMAX'),
      ]),
      PidCard('垂直位置', [PidGainField('位置 P', '${psc}_D_POS_P')]),
      PidCard('垂直速度', [
        PidGainField('比例 (P)', '${psc}_D_VEL_P'),
        PidGainField('积分 (I)', '${psc}_D_VEL_I'),
        PidGainField('积分上限 (I Max)', '${psc}_D_VEL_IMAX'),
      ]),
    ]),
  ];
}

List<PidSection> _fixedWingPidSections(PidLevel level) {
  const axes = [('横滚', 'RLL'), ('俯仰', 'PTCH'), ('偏航', 'YAW')];
  if (level == PidLevel.basic) {
    return [
      PidSection('速率 PID', [
        for (final (label, axis) in axes)
          _pidRateCard('$label速率', '${axis}_RATE', level),
      ]),
    ];
  }
  return [
    PidSection('速率滤波与前馈', [
      for (final (label, axis) in axes)
        _pidRateCard('$label速率', '${axis}_RATE', level),
    ]),
    PidSection('舵机与姿态', [
      PidCard('滚转', [PidGainField('时间常数 (TCONST)', 'RLL2SRV_TCONST')]),
      PidCard('俯仰', [
        PidGainField('时间常数 (TCONST)', 'PTCH2SRV_TCONST'),
        PidGainField('俯仰→滚转混控', 'PTCH2SRV_RLL'),
      ]),
      PidCard('偏航', [
        PidGainField('阻尼 (DAMP)', 'YAW2SRV_DAMP'),
        PidGainField('积分 (INT)', 'YAW2SRV_INT'),
        PidGainField('积分上限 (I Max)', 'YAW2SRV_IMAX'),
        PidGainField('偏航→滚转补偿', 'YAW2SRV_RLL'),
        PidGainField('侧滑补偿 (SLIP)', 'YAW2SRV_SLIP'),
      ]),
    ]),
    PidSection('TECS 能量控制', [
      PidCard('高度与速度', [
        PidGainField('时间常数', 'TECS_TIME_CONST'),
        PidGainField('速度权重', 'TECS_SPDWEIGHT'),
        PidGainField('积分增益', 'TECS_INTEG_GAIN'),
        PidGainField('油门阻尼', 'TECS_THR_DAMP'),
        PidGainField('俯仰阻尼', 'TECS_PTCH_DAMP'),
      ]),
      PidCard('爬升与下降', [
        PidGainField('最大爬升率', 'TECS_CLMB_MAX'),
        PidGainField('最大下降率', 'TECS_SINK_MAX'),
      ]),
    ]),
  ];
}
