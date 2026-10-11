import 'package:mgcs_frontend/connection/connection_controller.dart';

/// 调参层级。基础档只放自稳 P 和速率 P/I/D；扩展档补齐前馈、滤波、限幅
/// 以及位置环，避免一屏同时铺开四十多个输入框。
enum PidLevel { basic, extended }

/// PID 字段的显示标签与语义角色。
///
/// 这里存的是**语义角色**（如 `pid.rate.roll.p`），不是固件参数名：角色名由
/// 前端定义、与固件和机型无关，实际参数名（`ATC_RAT_RLL_P` vs `Q_A_RAT_RLL_P`）
/// 由 [ConnectionController.roleParam] 按当前 profile 解析（safety.md S6）。
class PidGainField {
  const PidGainField(this.label, this.role);

  final String label;
  final String role;
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

const _pidAxes = [('横滚', 'roll'), ('俯仰', 'pitch'), ('偏航', 'yaw')];
const _fixedWingAxes = [('横滚', 'roll'), ('俯仰', 'pitch'), ('偏航', 'yaw')];

/// 三轴速率卡：基础档给 P/I/D，扩展档给前馈、积分上限与三档滤波。
PidCard _pidRateCard(String title, String prefix, PidLevel level) =>
    switch (level) {
      PidLevel.basic => PidCard(title, [
        PidGainField('比例 (P)', '$prefix.p'),
        PidGainField('积分 (I)', '$prefix.i'),
        PidGainField('微分 (D)', '$prefix.d'),
      ]),
      PidLevel.extended => PidCard(title, [
        PidGainField('前馈 (FF)', '$prefix.ff'),
        PidGainField('积分上限 (I Max)', '$prefix.imax'),
        PidGainField('误差滤波 (FLTE)', '$prefix.flte'),
        PidGainField('微分滤波 (FLTD)', '$prefix.fltd'),
        PidGainField('目标滤波 (FLTT)', '$prefix.fltt'),
      ]),
    };

List<PidSection> pidSections(
  VehicleControlProfile profile,
  bool vtol,
  PidLevel level,
) {
  // 垂起的多旋翼部分与多旋翼一一对应；角色表在 plane/vtol profile 下会把
  // `pid.*` 解析成 `Q_A_*`/`Q_P_*`，在多旋翼下解析成 `ATC_*`/`PSC_*`。
  if (vtol || profile == VehicleControlProfile.copter) {
    return _multirotorPidSections(level);
  }
  if (profile == VehicleControlProfile.fixedWing ||
      profile == VehicleControlProfile.planeVtol) {
    return _fixedWingPidSections(level);
  }
  return const [];
}

List<PidSection> _multirotorPidSections(PidLevel level) {
  if (level == PidLevel.basic) {
    return [
      PidSection('自稳 P', [
        for (final (label, axis) in _pidAxes)
          PidCard('自稳 $label', [PidGainField('比例 (P)', 'pid.angle.$axis.p')]),
      ]),
      PidSection('速率 PID', [
        for (final (label, axis) in _pidAxes)
          _pidRateCard('$label速率', 'pid.rate.$axis', level),
      ]),
    ];
  }
  return [
    PidSection('速率滤波与前馈', [
      for (final (label, axis) in _pidAxes)
        _pidRateCard('$label速率', 'pid.rate.$axis', level),
    ]),
    PidSection('响应限制', [
      PidCard('最大角加速度 (ACCEL MAX)', [
        PidGainField('横滚', 'pid.accel.roll.max'),
        PidGainField('俯仰', 'pid.accel.pitch.max'),
        PidGainField('偏航', 'pid.accel.yaw.max'),
      ]),
      PidCard('最大角速度 (RATE MAX)', [
        PidGainField('横滚', 'pid.rate_limit.roll.max'),
        PidGainField('俯仰', 'pid.rate_limit.pitch.max'),
        PidGainField('偏航', 'pid.rate_limit.yaw.max'),
      ]),
      PidCard('姿态平滑', [
        PidGainField('输入时间常数 (INPUT TC)', 'pid.attitude.input_tc'),
        PidGainField('角度上限 (ANGLE MAX)', 'pid.attitude.angle_max'),
      ]),
    ]),
    PidSection('位置与速度', [
      PidCard('水平位置', [PidGainField('位置 P', 'pos.ne.pos_p')]),
      PidCard('水平速度', [
        PidGainField('比例 (P)', 'pos.ne.vel.p'),
        PidGainField('积分 (I)', 'pos.ne.vel.i'),
        PidGainField('微分 (D)', 'pos.ne.vel.d'),
        PidGainField('积分上限 (I Max)', 'pos.ne.vel.imax'),
      ]),
      PidCard('垂直位置', [PidGainField('位置 P', 'pos.d.pos_p')]),
      PidCard('垂直速度', [
        PidGainField('比例 (P)', 'pos.d.vel.p'),
        PidGainField('积分 (I)', 'pos.d.vel.i'),
        PidGainField('积分上限 (I Max)', 'pos.d.vel.imax'),
      ]),
    ]),
  ];
}

List<PidSection> _fixedWingPidSections(PidLevel level) {
  if (level == PidLevel.basic) {
    return [
      PidSection('速率 PID', [
        for (final (label, axis) in _fixedWingAxes)
          _pidRateCard('$label速率', 'fixedwing.rate.$axis', level),
      ]),
    ];
  }
  return [
    PidSection('速率滤波与前馈', [
      for (final (label, axis) in _fixedWingAxes)
        _pidRateCard('$label速率', 'fixedwing.rate.$axis', level),
    ]),
    PidSection('舵机与姿态', [
      PidCard('滚转', [
        PidGainField('时间常数 (TCONST)', 'fixedwing.servo.roll.tconst'),
      ]),
      PidCard('俯仰', [
        PidGainField('时间常数 (TCONST)', 'fixedwing.servo.pitch.tconst'),
        PidGainField('俯仰→滚转混控', 'fixedwing.servo.pitch.mix_roll'),
      ]),
      PidCard('偏航', [
        PidGainField('阻尼 (DAMP)', 'fixedwing.servo.yaw.damp'),
        PidGainField('积分 (INT)', 'fixedwing.servo.yaw.int'),
        PidGainField('积分上限 (I Max)', 'fixedwing.servo.yaw.imax'),
        PidGainField('偏航→滚转补偿', 'fixedwing.servo.yaw.mix_roll'),
        PidGainField('侧滑补偿 (SLIP)', 'fixedwing.servo.yaw.slip'),
      ]),
    ]),
    PidSection('TECS 能量控制', [
      PidCard('高度与速度', [
        PidGainField('时间常数', 'tecs.time_const'),
        PidGainField('速度权重', 'tecs.speed_weight'),
        PidGainField('积分增益', 'tecs.integ_gain'),
        PidGainField('油门阻尼', 'tecs.throttle_damp'),
        PidGainField('俯仰阻尼', 'tecs.pitch_damp'),
      ]),
      PidCard('爬升与下降', [
        PidGainField('最大爬升率', 'tecs.climb_max'),
        PidGainField('最大下降率', 'tecs.sink_max'),
      ]),
    ]),
  ];
}
