import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/flight/flight_message_translator.dart';

void main() {
  test(
    'frame messages distinguish rotor class from layout and preserve custom names',
    () {
      const examples = {
        'Frame: QUAD/PLUS': '机架：四旋翼 / 十字型布局',
        'Frame: QUAD/X': '机架：四旋翼 / X 型布局',
        'Frame: HEXA/X': '机架：六旋翼 / X 型布局',
        'Frame: OCTAQUAD/X': '机架：四轴共轴八旋翼 / X 型布局',
        'Frame: HELI': '机架：传统直升机',
        'Frame: QUAD/BF_X': '机架：四旋翼 / X 型布局（Betaflight 电机编号）',
        'Frame: QUAD/NEW_LAYOUT': '机架：四旋翼 / NEW_LAYOUT',
        'Frame: MyCustom/X': '机架：MyCustom/X',
      };
      for (final entry in examples.entries) {
        final result = translateFlightMessage(entry.key);
        expect(result.displayText, entry.value, reason: entry.key);
        expect(result.originalText, entry.key);
      }
    },
  );

  test(
    'arming refusals and mode change failures translate the full reason',
    () {
      const examples = {
        'Arm: Auto mode not armable': '解锁检查：自动航线模式不允许解锁',
        'PreArm: RTL mode not armable': '解锁前检查：返航模式不允许解锁',
        'PreArm: mode not armable': '解锁前检查：当前模式不允许解锁',
        'PreArm: MANUAL not Q mode': '解锁前检查：手动：当前不处于多旋翼模式',
        'PreArm: AUTO missing takeoff waypoint': '解锁前检查：自动航线：缺少起飞航点',
        'Arm: Collective too high': '解锁检查：总距过高',
        'Mode change to Auto failed: throttle too high': '切换至自动航线模式失败：油门过高',
        'Mode change to Guided failed: requires position': '切换至引导模式失败：需要位置估计',
        'Mode change to RTL failed: GCS entry disabled (FLTMODE_GCSBLOCK)':
            '切换至返航模式失败：FLTMODE_GCSBLOCK 禁止通过地面站进入该模式',
        'Mode change to Auto failed: Future diagnostic X-17':
            '切换至自动航线模式失败：Future diagnostic X-17',
      };
      for (final entry in examples.entries) {
        final result = translateFlightMessage(entry.key);
        expect(result.displayText, entry.value, reason: entry.key);
        expect(result.originalText, entry.key);
      }
    },
  );

  test('nested AHRS checks and failsafe actions keep their actual meaning', () {
    const examples = {
      'PreArm: AHRS: GPS numsats 5 (needs 6)': '解锁前检查：AHRS：GPS 卫星数 5（要求至少 6）',
      'EKF variance: position lost': 'EKF 位置估计丢失',
      'Radio Failsafe - Disarming': '遥控故障保护：正在上锁',
      'GCS Failsafe - Continuing Auto Mode': '地面站故障保护：继续自动任务',
      'Radio Failsafe Cleared': '遥控故障保护已解除',
      'GCS Failsafe On: switched to RTL': '地面站故障保护已触发：切换至 返航',
      'VTOL position2 started v=2.5 d=12.0 h=30.5':
          '开始垂直起降定位阶段 2，地速 2.5 米/秒，距离 12.0 米，离地高度 30.5 米',
    };
    for (final entry in examples.entries) {
      expect(
        translateFlightMessage(entry.key).displayText,
        entry.value,
        reason: entry.key,
      );
    }
  });

  test('translates common ArduPilot status text templates', () {
    expect(
      translateFlightMessage('PreArm: GPS 3D fix required').displayText,
      '解锁前检查：GPS 尚未获得三维定位',
    );
    expect(
      translateFlightMessage('EKF3 IMU0 is using GPS').displayText,
      'EKF3 IMU0 正在使用 GPS',
    );
    expect(translateFlightMessage('Battery 1 is low').displayText, '电池 1 电量低');
    expect(
      translateFlightMessage('Mode changed to AUTO').displayText,
      '飞行模式已切换为 自动航线',
    );
  });

  test('mission messages retain raw indices and command parameters', () {
    const examples = {
      'Takeoff': '起飞',
      '1 WP': '1 航点',
      '8 RTL': '8 返航',
      'Mission: 8 RTL': '任务：8 返航',
      'Mission: 12 Jump 3': '任务：12 跳转 3',
      'Mission: 4 LoitTime': '任务：4 定时盘旋或悬停',
      'Reached command #6': '已完成任务项 #6',
      'Reached waypoint #2': '已到达航点 #2',
      'VTOLTakeoff': '垂直起飞',
      'VTOLLand': '垂直降落',
      'PreArm: Missing mission item: do land start': '解锁前检查：缺少任务项：降落序列起点',
    };
    for (final entry in examples.entries) {
      final result = translateFlightMessage(entry.key);
      expect(result.displayText, entry.value, reason: entry.key);
      expect(result.originalText, entry.key);
      expect(result.translated, isTrue);
    }
  });

  test('pre-arm and arming checks use complete reviewed fault phrases', () {
    const examples = {
      'PreArm: RC not calibrated': '解锁前检查：遥控器未校准',
      'Arm: Throttle too high': '解锁检查：油门过高',
      'PreArm: Hardware safety switch': '解锁前检查：硬件安全开关未解除',
      'GPS: No GPS': 'GPS：未检测到 GPS',
      'Transition FW done': '固定翼转换完成',
      'Land complete': '降落完成',
    };
    for (final entry in examples.entries) {
      expect(
        translateFlightMessage(entry.key).displayText,
        entry.value,
        reason: entry.key,
      );
    }
  });

  test(
    'unrecognised fault bodies and parameter names are not word-replaced',
    () {
      const body = 'Custom disarming diagnostic Q_RTL_MODE XYZ-42';
      expect(
        translateFlightMessage('PreArm: $body').displayText,
        '解锁前检查：$body',
      );
      expect(
        translateFlightMessage('9 FutureCommand').displayText,
        '9 FutureCommand',
      );
    },
  );

  test('unknown messages stay verbatim', () {
    const text = 'Custom firmware diagnostic XYZ-42';
    final result = translateFlightMessage(text);
    expect(result.translated, isFalse);
    expect(result.displayText, text);
    expect(result.originalText, text);
  });

  test(
    'source templates preserve sensor indices, thresholds and signed values',
    () {
      const examples = {
        'Mission: 2 Jump 1/unlimited': '任务：2 跳转 1/无限次',
        'Reached waypoint #7 dist 12m': '已到达航点 #7，距离 12 米',
        'Skipping invalid cmd #31000': '跳过无效命令，命令ID #31000',
        'PreArm: RC3_MIN is greater than RC3_TRIM': '解锁前检查：RC3_MIN 大于 RC3_TRIM',
        'PreArm: Battery 2 below minimum arming voltage': '解锁前检查：电池 2 低于最低解锁电压',
        'GPS 2: detected u-blox': 'GPS 2：检测到 u-blox',
        'EKF3 lane switch 0': 'EKF3 切换至实例 0',
        'EKF3 IMU1 MAG2 ground mag anomaly, yaw re-aligned':
            'EKF3 IMU1 MAG2 地面磁场异常，已重新对准航向',
        'EKF Failsafe: changed to LAND Mode': 'EKF 故障保护：已切换至 降落 模式',
        'GPS numsats 5 (needs 6)': 'GPS 卫星数 5（要求至少 6）',
        'Battery 2 is low 21.50V used 1250 mAh': '电池 2 低电量 21.50V，已消耗 1250 mAh',
        'Battery 2 is missing, last: 21.50V used 1250 mAh':
            '电池 2 数据丢失，最后读数： 21.50V，已消耗 1250 mAh',
        'Transition started airspeed 12.5': '开始模式转换，空速 12.5 米/秒',
        'Place vehicle on its LEFT side and press any key.':
            '请将机体左侧朝下放置，然后按任意键继续。',
        'Trim OK: roll=-1.25 pitch=+0.50 yaw=12.00':
            '配平完成：横滚=-1.25 俯仰=+0.50 航向=12.00',
      };
      for (final entry in examples.entries) {
        final result = translateFlightMessage(entry.key);
        expect(result.displayText, entry.value, reason: entry.key);
        expect(result.originalText, entry.key);
      }
    },
  );
}
