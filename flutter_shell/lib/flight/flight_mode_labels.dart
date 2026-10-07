/// 飞行模式的中文显示名。
///
/// 模式标识（`FlightModeOption.name`）由后端按固件模式表给出，只有英文标识；
/// 这里集中放一份中文显示名，**只给模式列表用**，避免中文散落到各个页面。
///
/// 顶部状态栏按约定**不使用**这张表：状态栏只显示英文模式名和 VTOL 状态后缀。
const Map<String, String> _flightModeLabels = {
  // ── ArduPilot Plane ──
  'Manual': '手动',
  'Circle': '绕圈',
  'Stabilize': '自稳',
  'Training': '训练',
  'Acro': '特技',
  'FBWA': '增稳 A',
  'FBWB': '增稳 B',
  'Cruise': '巡航',
  'Autotune': '自动调参',
  'Auto': '自动航线',
  'AutoRTL': '自动返航',
  'RTL': '返航',
  'SmartRTL': '智能返航',
  'Loiter': '盘旋',
  'Loiter2QLand': '盘旋转多旋翼降落',
  'Takeoff': '起飞',
  'AvoidADSB': '避让 ADSB',
  'Guided': '引导',
  'GuidedNoGPS': '无 GPS 引导',
  'Thermal': '热气流',
  'Autoland': '自动降落',
  'QStabilize': '多旋翼自稳',
  'QHover': '多旋翼定高',
  'QLoiter': '多旋翼悬停',
  'QLand': '多旋翼降落',
  'QRTL': '多旋翼返航',
  'QAutotune': '多旋翼自动调参',
  'QAcro': '多旋翼特技',

  // ── ArduPilot Copter ──
  'AltHold': '定高',
  'Land': '降落',
  'Drift': '漂移',
  'Sport': '运动',
  'Flip': '翻滚',
  'PosHold': '定点悬停',
  'Brake': '刹车',
  'Throw': '抛飞',
  'FlowHold': '光流定位',
  'Follow': '跟随',
  'Zigzag': '之字形',
  'SystemID': '系统辨识',
  'Autorotate': '自转降落',

  // ── ArduPilot Rover / Sub / Tracker ──
  'Hold': '保持',
  'Simple': '简单模式',
  'SuperSimple': '超简单模式',
  'Steering': '转向',
  'Steer': '转向',
  'Scan': '扫描',
  'ServoTest': '舵机测试',
  'Dock': '入库',
  'Surface': '水面',
  'Stabilized': '自稳',
  'Rattitude': '特技自稳',
  'Turtle': '翻正',
  'Termination': '紧急终止',
  'Initialising': '初始化中',

  // ── PX4 ──
  'Offboard': '外部控制',
};

/// 未收录的模式直接回退成原始英文标识。
String flightModeLabel(String name) => _flightModeLabels[name] ?? name;
