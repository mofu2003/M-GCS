// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Chinese (`zh`).
class MgLocalizationsZh extends MgLocalizations {
  MgLocalizationsZh([String locale = 'zh']) : super(locale);

  @override
  String get appTitle => 'M-GCS';

  @override
  String get navConnection => '连接';

  @override
  String get navMission => '任务';

  @override
  String get navDashboard => '仪表';

  @override
  String get navBackstage => '后台';

  @override
  String get returnFrontstage => '返回前台';

  @override
  String get statusGps => 'GPS 信息';

  @override
  String get statusBattery => '电池信息';

  @override
  String get statusFlightMode => '飞行模式';

  @override
  String get statusArmed => '解锁状态';

  @override
  String get statusFlightController => '飞控信息';

  @override
  String get noData => '无数据';

  @override
  String get currentMode => '当前模式';

  @override
  String get armed => '已解锁';

  @override
  String get disarmed => '未解锁';

  @override
  String get vehicleArmed => '飞行器已解锁';

  @override
  String get safe => '安全';

  @override
  String messagesCount(int count) {
    return '$count条消息';
  }

  @override
  String get notConnected => '未连接';

  @override
  String get clickToView => '点击查看';

  @override
  String get vehicleNotConnected => '飞控未连接';

  @override
  String get notSelected => '未选择';

  @override
  String get unidentifiedVehicle => '未识别飞控';

  @override
  String get noMission => '当前无任务';

  @override
  String get missionEmptyMessage => '新建任务、导入任务，或从飞控下载现有任务。';

  @override
  String get disconnected => '飞控连接已断开';

  @override
  String get waitingForConnection => '等待连接';

  @override
  String get chooseConnection => '请选择连接方式';

  @override
  String get connectedFlightController => '已连接飞控';

  @override
  String get airframePending => '机架待确认';

  @override
  String get firmwarePending => '固件待确认';

  @override
  String get versionPending => '版本待确认';

  @override
  String get linkMeasuring => '链路 测量中';

  @override
  String linkPercent(int percent) {
    return '链路 $percent%';
  }

  @override
  String get remoteUnavailable => '遥控器 不可用';

  @override
  String remotePercent(int percent) {
    return '遥控器 $percent%';
  }

  @override
  String remoteRssi(int rssi) {
    return '遥控器 RSSI $rssi';
  }

  @override
  String get requestingParameters => '正在请求参数列表';

  @override
  String parametersProgress(int received, int total) {
    return '$received / $total';
  }

  @override
  String get gpsNotLocated => '未定位';

  @override
  String satelliteCount(int count) {
    return '$count颗';
  }

  @override
  String get connect => '连接';

  @override
  String get disconnect => '断开';

  @override
  String get cancelConnection => '取消连接';

  @override
  String get serial => 'Serial';

  @override
  String get udp => 'UDP';

  @override
  String get tcp => 'TCP';

  @override
  String get serialPort => '串口连接';

  @override
  String get port => '端口';

  @override
  String get baudRate => '波特率';

  @override
  String get listenMode => '监听模式';

  @override
  String get remoteAddress => '远端地址';

  @override
  String get close => '关闭';

  @override
  String get closeConnectionPanel => '关闭连接面板';

  @override
  String get selectSerialPort => '请选择串口';

  @override
  String get udpListen => 'UDP 监听';

  @override
  String get tcpConnection => 'TCP 连接';

  @override
  String get mission => '航点任务';

  @override
  String get addWaypoint => '添加航点';

  @override
  String get editWaypoint => '编辑航点';

  @override
  String get read => '读取';

  @override
  String get write => '写入';

  @override
  String get clear => '清空';

  @override
  String get startMission => '开始任务';

  @override
  String get newMission => '新建任务';

  @override
  String get importMission => '导入任务';

  @override
  String get downloadFromVehicle => '从飞控下载';

  @override
  String get backstage => '后台';

  @override
  String get parameters => '参数配置';

  @override
  String get airframe => '机架类型';

  @override
  String get motorOutputs => '电机输出';

  @override
  String get portConfiguration => '端口配置';

  @override
  String get remoteControl => '遥控配置';

  @override
  String get pidTuning => 'PID 调参';

  @override
  String get calibration => '传感器/外设配置';

  @override
  String get safetySettings => '安全设置';

  @override
  String get flightLogs => '飞行日志';

  @override
  String parametersDescription(int count) {
    return '$count 个飞控参数';
  }

  @override
  String get airframeDescription => '按飞控类型配置 Copter 或 Plane/QuadPlane 机架';

  @override
  String get motorOutputsDescription => 'SERVO1-16 · 电机功能、反向和 PWM 范围';

  @override
  String portConfigurationDescription(int count) {
    return '$count 个可用串口 · 修改后立即写入飞控';
  }

  @override
  String get remoteControlDescription => '飞行模式、通道功能与摇杆校准';

  @override
  String get pidTuningDescription => '根据当前机架自动显示控制器 · 修改后立即写入飞控';

  @override
  String get calibrationDescription => 'IMU、罗盘、GPS 与空速计';

  @override
  String get safetySettingsDescription => '围栏、解锁检查与电量保护';

  @override
  String get flightLogsDescription => '浏览飞控内存卡文件，并将日志或其他文件下载到电脑';

  @override
  String parameterWriteFailures(int count) {
    return '$count 项参数写入失败，已保留草稿';
  }

  @override
  String parameterFileReadSummary(
    int parsed,
    int staged,
    int unchanged,
    int unknown,
  ) {
    return '已读取 $parsed 项：$staged 项待写入，$unchanged 项未变化，$unknown 项未匹配';
  }

  @override
  String parameterFileReadFailed(String error) {
    return '读取参数文件失败：$error';
  }

  @override
  String parametersSaved(int count) {
    return '已保存 $count 项参数';
  }

  @override
  String parameterFileSaveFailed(String error) {
    return '保存参数文件失败：$error';
  }

  @override
  String get unknown => '未知';

  @override
  String get notLocated => '未定位';
}
