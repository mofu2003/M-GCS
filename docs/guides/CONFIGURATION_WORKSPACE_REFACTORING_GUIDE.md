# Configuration Workspace 拆分指导文档

**文档版本**: 0.0.78  
**目标文件**: `flutter_shell/lib/configuration_workspace.dart`  
**当前状态**: 4099 行，69 个类  
**目标状态**: 模块化结构，主文件 < 200 行

---

## 📋 目录

1. [概述](#概述)
2. [目标结构](#目标结构)
3. [拆分准备](#拆分准备)
4. [核心注意事项](#核心注意事项)
5. [拆分步骤](#拆分步骤)
6. [验收清单](#验收清单)
7. [常见问题](#常见问题)

---

## 概述

### 当前问题

`configuration_workspace.dart` 是一个巨型单文件，包含了飞控配置工作区的所有功能：

- **9 个配置页面**: 系统状态、校准、端口、电机、飞行模式、PID、电池、解锁、全部参数
- **69 个类**: 页面、子页面、卡片、字段、进度条、图表等
- **4099 行代码**: 难以维护、查找、测试

### 拆分目标

1. **提高可维护性** - 每个文件职责单一，易于理解
2. **便于测试** - 独立模块可以单独测试
3. **支持并行开发** - 不同开发者可以同时修改不同模块
4. **降低合并冲突** - 修改互不影响
5. **符合项目规范** - 遵循 AGENTS.md 中的代码组织要求

### 预期成果

```
拆分前: 1 个文件, 4099 行
拆分后: ~30 个文件, 主文件 ~200 行, 每个模块 < 500 行
```

---

## 目标结构

### 推荐的目录布局

```
lib/configuration/
├── configuration_workspace.dart          # 主入口 (~200 行)
│   └── ConfigurationWorkspace            # 路由和框架
│
├── widgets/                              # 通用组件
│   ├── mac_tab_surface.dart              # 标签页容器
│   └── config_constants.dart             # 布局常量
│
├── calibration/                          # 校准模块
│   ├── calibration_page.dart             # 校准主页面
│   ├── rc_calibration_view.dart          # 遥控器校准
│   ├── magnetometer_calibration_view.dart # 磁力计校准
│   ├── accelerometer_calibration_view.dart # 加速度计校准
│   └── widgets/
│       ├── calibration_card.dart
│       ├── calibration_progress.dart
│       ├── calibration_aircraft.dart
│       └── magnetic_field_strength.dart
│
├── pid/                                  # PID 调参模块
│   ├── pid_tuning_page.dart
│   ├── pid_parameters_page.dart
│   └── widgets/
│       ├── pid_card.dart
│       ├── pid_gain_tile.dart
│       ├── pid_section_header.dart
│       └── pid_card_grid.dart
│
├── battery/                              # 电池配置模块
│   └── battery_configuration_page.dart
│
├── motors/                               # 电机输出模块
│   └── motor_output_page.dart
│
├── system/                               # 系统状态模块
│   └── system_status_page.dart
│
├── flight_modes/                         # 飞行模式模块
│   └── flight_modes_page.dart
│
├── ports/                                # 端口配置模块
│   └── ports_configuration_page.dart
│
├── arming/                               # 解锁检查模块
│   └── arming_checks_page.dart
│
└── parameters/                           # 全部参数模块
    └── all_parameters_page.dart
```

### 模块职责划分

| 模块 | 职责 | 主要组件 |
|------|------|---------|
| **configuration_workspace.dart** | 主入口、路由、页面框架 | ConfigurationWorkspace |
| **calibration/** | 所有校准相关功能 | 遥控器、磁力计、加速度计 |
| **pid/** | PID 参数调整 | PID 卡片、增益字段 |
| **battery/** | 电池配置 | 电池参数表单 |
| **motors/** | 电机输出和测试 | 电机输出条、测试控制 |
| **system/** | 系统状态显示 | 传感器健康、系统信息 |
| **widgets/** | 跨模块通用组件 | 标签页、常量定义 |

---

## 拆分准备

### 1. 创建备份和分支

```bash
# 切换到 flutter_shell 目录
cd /Users/fansicheng/Projects/M-GCS/flutter_shell

# 创建专门的拆分分支
git checkout -b refactor/split-configuration-workspace

# 备份原文件（以防万一）
cp lib/configuration_workspace.dart lib/configuration_workspace.dart.backup
```

### 2. 分析文件结构

```bash
# 查找所有类定义
grep -n "^class " lib/configuration_workspace.dart > class_list.txt

# 查找 ConnectionController 使用
grep -n "connectionController" lib/configuration_workspace.dart > controller_usage.txt

# 查找 FFI 调用
grep -n "\.request\|\.send\|\.set.*Parameter" lib/configuration_workspace.dart > ffi_calls.txt

# 查找所有 StatefulWidget
grep -n "extends Stateful" lib/configuration_workspace.dart > stateful_widgets.txt
```

### 3. 理解依赖关系

**关键依赖**：
- `ConnectionController` - 几乎所有页面都需要
- `onReturnToMap` - 返回地图的回调
- 主题系统: `MgColors`, `MgTypography`, `MgMetrics`
- 通用组件: `MacPanel`, `AlignedFieldRow`, `AdaptivePopupButton`

**共享状态**：
- 当前选中的页面 ID (`_selectedPageId`)
- 校准进度和状态
- 参数加载状态

### 4. 准备测试基线

```bash
# 记录拆分前的测试结果
flutter test > test_baseline_before_split.txt

# 确保当前测试全部通过
flutter analyze lib test
flutter test
```

---

## 核心注意事项

### 1. 识别和处理共享状态

**问题**: 当前所有状态都在 `_ConfigurationWorkspaceState` 中

**解决方案**: 
- 保留主框架状态在主文件
- 页面内部状态下沉到各自的 StatefulWidget
- 通过回调和 ConnectionController 通信

```dart
// ❌ 拆分前：所有状态在一起
class _ConfigurationWorkspaceState {
  String _selectedPageId;
  CalibrationState _calibrationState;
  List<Parameter> _parameters;
  // ... 更多状态
}

// ✅ 拆分后：状态分离
// configuration_workspace.dart
class _ConfigurationWorkspaceState {
  String _selectedPageId;  // 只保留路由状态
}

// calibration_page.dart
class _CalibrationPageState {
  CalibrationState _calibrationState;  // 校准专属状态
}

// parameters_page.dart
class _ParametersPageState {
  List<Parameter> _parameters;  // 参数专属状态
}
```

### 2. 保持 FFI 调用的完整性

**重要**: 每个 FFI 调用必须有完整的错误处理

```dart
// ✅ 正确模式：完整的调用链
class MagnetometerCalibrationView extends StatefulWidget {
  // ...
  
  Future<void> _startCalibration() async {
    if (!mounted) return;
    
    try {
      // 1. 检查前置条件
      if (!widget.connectionController.isConnected) {
        _showError('未连接飞控');
        return;
      }
      
      // 2. FFI 调用
      final result = await widget.connectionController
          .calibrationBackend
          .requestMagnetometerCalibration();
      
      // 3. 更新 UI 状态
      if (mounted) {
        setState(() {
          _calibrationState = CalibrationState.inProgress;
        });
      }
      
      // 4. 监听进度
      _progressSubscription = widget.connectionController
          .calibrationBackend
          .progressStream
          .listen(_onProgress);
          
    } on FfiException catch (e) {
      // 5. FFI 特定错误
      if (mounted) {
        _showError('校准失败: ${e.message}');
      }
    } catch (e) {
      // 6. 通用错误
      if (mounted) {
        _showError('未知错误: $e');
      }
    }
  }
  
  @override
  void dispose() {
    // 7. 清理资源
    _progressSubscription?.cancel();
    super.dispose();
  }
}
```

**检查清单**：
- [ ] 每个 FFI 调用都在 try-catch 内
- [ ] 有连接状态检查
- [ ] 有 mounted 检查
- [ ] 有资源清理（dispose）
- [ ] 错误消息对用户友好

### 3. 统一常量和样式

**当前问题**: 布局常量散落在文件顶部

```dart
// 当前定义（第 19-27 行）
const double _pageMargin = 24;
const double _tabBodyInset = 12;
const double _sectionGap = 16;
const double _headerGap = 8;
const double _cardGap = 10;
```

**解决方案 A（推荐）**: 提升到主题系统

```dart
// lib/theme/mg_metrics.dart
class MgMetrics {
  // ... 现有定义 ...
  
  // 配置页面布局
  static const double configPageMargin = 24.0;
  static const double configTabBodyInset = 12.0;
  static const double configSectionGap = 16.0;
  static const double configHeaderGap = 8.0;
  static const double configCardGap = 10.0;
}
```

**解决方案 B**: 创建模块内部常量文件

```dart
// lib/configuration/widgets/config_constants.dart
class ConfigMetrics {
  static const double pageMargin = 24.0;
  static const double tabBodyInset = 12.0;
  static const double sectionGap = 16.0;
  static const double headerGap = 8.0;
  static const double cardGap = 10.0;
}
```

**使用时**：
```dart
// 拆分前
padding: EdgeInsets.all(_pageMargin),

// 拆分后
padding: EdgeInsets.all(MgMetrics.configPageMargin),
// 或
padding: EdgeInsets.all(ConfigMetrics.pageMargin),
```

### 4. Import 路径规范

**原则**: 
- 模块内使用相对路径
- 跨模块使用包路径
- 避免超过 3 层的相对路径

```dart
// ✅ 推荐：模块内相对路径
// lib/configuration/calibration/magnetometer_calibration_view.dart
import '../widgets/mac_tab_surface.dart';
import 'widgets/calibration_card.dart';

// ✅ 推荐：跨模块包路径
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';

// ❌ 避免：过深的相对路径
import '../../../theme/mg_colors.dart';
import '../../../../widgets/mac_panel.dart';
```

**import 顺序**（按 Dart 官方风格）：
```dart
// 1. Dart SDK
import 'dart:async';
import 'dart:math';

// 2. Flutter
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

// 3. 第三方包
import 'package:macos_ui/macos_ui.dart';

// 4. 本项目包路径
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';

// 5. 相对路径
import '../widgets/mac_tab_surface.dart';
import 'widgets/calibration_card.dart';
```

### 5. 私有类的可见性决策

**决策树**：

```
这个类需要被其他文件使用吗？
├── 是 → 改为公有类（去掉下划线前缀）
│   └── class MagnetometerCalibrationView { }
│
└── 否 → 保持私有
    ├── 只在本文件内使用 → class _InternalHelper { }
    └── 作为实现细节 → class _PrivateWidget { }
```

**示例**：

```dart
// lib/configuration/calibration/magnetometer_calibration_view.dart

// ✅ 公有：主要导出类
class MagnetometerCalibrationView extends StatefulWidget { }
class MagnetometerCalibrationState extends State<MagnetometerCalibrationView> { }

// ✅ 私有：内部实现细节
class _MagnetometerCalibrationCard extends StatelessWidget { }
class _MagnetometerProgressIndicator extends StatelessWidget { }
class _CompassRose extends CustomPainter { }
```

### 6. 资源清理和生命周期管理

**必须清理的资源**：
- Timer
- AnimationController
- StreamSubscription
- ScrollController
- TextEditingController
- FocusNode

**检查方法**：
```bash
# 搜索需要清理的资源
grep -n "Timer\|AnimationController\|StreamSubscription\|ScrollController" configuration_workspace.dart
```

**标准模式**：
```dart
class _SomePageState extends State<SomePage> {
  Timer? _pollingTimer;
  AnimationController? _animationController;
  StreamSubscription? _dataSubscription;
  
  @override
  void initState() {
    super.initState();
    
    // 初始化资源
    _animationController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 300),
    );
    
    _dataSubscription = widget.connectionController
        .telemetryStream
        .listen(_onTelemetryUpdate);
        
    _pollingTimer = Timer.periodic(
      const Duration(milliseconds: 500),
      (_) => _poll(),
    );
  }
  
  @override
  void dispose() {
    // 清理所有资源
    _pollingTimer?.cancel();
    _animationController?.dispose();
    _dataSubscription?.cancel();
    
    super.dispose();
  }
}
```

### 7. 测试策略

**拆分前**: 记录基线
```bash
flutter test --coverage > coverage_before.txt
flutter test --reporter expanded > test_results_before.txt
```

**拆分过程**: 每个模块独立测试

```dart
// test/configuration/calibration/magnetometer_calibration_view_test.dart
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/configuration/calibration/magnetometer_calibration_view.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

import '../../../mocks/mock_connection_controller.dart';

void main() {
  late MockConnectionController mockController;
  
  setUp(() {
    mockController = MockConnectionController();
  });
  
  group('MagnetometerCalibrationView', () {
    testWidgets('应该渲染标题和说明', (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: MagnetometerCalibrationView(
            connectionController: mockController,
          ),
        ),
      );
      
      expect(find.text('磁力计校准'), findsOneWidget);
      expect(find.text('请按照提示旋转飞机'), findsOneWidget);
    });
    
    testWidgets('点击开始按钮应该调用 FFI', (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: MagnetometerCalibrationView(
            connectionController: mockController,
          ),
        ),
      );
      
      await tester.tap(find.text('开始校准'));
      await tester.pump();
      
      verify(mockController.calibrationBackend.requestMagnetometerCalibration())
          .called(1);
    });
    
    testWidgets('未连接时应该禁用开始按钮', (tester) async {
      when(mockController.isConnected).thenReturn(false);
      
      await tester.pumpWidget(
        MaterialApp(
          home: MagnetometerCalibrationView(
            connectionController: mockController,
          ),
        ),
      );
      
      final button = tester.widget<ElevatedButton>(
        find.widgetWithText(ElevatedButton, '开始校准'),
      );
      
      expect(button.enabled, isFalse);
    });
  });
}
```

**关键测试点**：
- [ ] Widget 能够渲染
- [ ] FFI 调用正确触发
- [ ] 连接状态正确反映
- [ ] 错误处理正确显示
- [ ] 资源正确清理（dispose 测试）

---

## 拆分步骤

### 阶段 0: 准备工作（1 小时）

**目标**: 创建目录结构，提取常量

```bash
# 1. 创建目录结构
mkdir -p lib/configuration/{widgets,calibration/widgets,pid/widgets,battery,motors,system,flight_modes,ports,arming,parameters}

# 2. 创建测试目录
mkdir -p test/configuration/{calibration,pid,battery,motors,system}

# 3. 提交空目录结构
git add lib/configuration/ test/configuration/
git commit -m "refactor: create configuration module directory structure"
```

**提取常量**：

```bash
# 创建常量文件
touch lib/configuration/widgets/config_constants.dart
```

编辑 `config_constants.dart`：

```dart
/// Configuration 模块布局常量
///
/// 统一配置页面的间距、圆角、尺寸等视觉参数。
/// 新页面应该使用这些常量而不是硬编码数值。
class ConfigMetrics {
  ConfigMetrics._();
  
  /// 页面左右边距
  static const double pageMargin = 24.0;
  
  /// 标签页内容缩进（MacosTabView 自带 12pt）
  static const double tabBodyInset = 12.0;
  
  /// 区块之间的间距
  static const double sectionGap = 16.0;
  
  /// 区块标题到内容的间距
  static const double headerGap = 8.0;
  
  /// 卡片之间的间距
  static const double cardGap = 10.0;
}
```

```bash
git add lib/configuration/widgets/config_constants.dart
git commit -m "refactor: extract configuration layout constants"
```

---

### 阶段 1: 提取通用组件（2-3 小时）

**目标**: 提取 `_MacTabSurface` 等通用组件

#### 步骤 1.1: 提取 MacTabSurface

从 `configuration_workspace.dart` 复制第 34-103 行到新文件：

```dart
// lib/configuration/widgets/mac_tab_surface.dart
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'config_constants.dart';

/// 标签页外壳：标签卡用 macos_ui 的 [MacosSegmentedControl]，
/// 面板沿用 macos_ui 标签视图的配色与圆角。
///
/// 不用 [MacosTabView]：它在 didChangeDependencies 中重复 addListener，
/// 而 dispose 只移除一次，点击标签会给已销毁的 State 发通知。
class MacTabSurface extends StatefulWidget {
  const MacTabSurface({
    super.key,
    required this.labels,
    required this.children,
    this.controller,
  });

  final List<String> labels;
  final List<Widget> children;

  /// 外部控制器。需要知道当前选中哪一页时传入，
  /// 不传则由本组件自己创建（默认行为）。
  final MacosTabController? controller;

  @override
  State<MacTabSurface> createState() => _MacTabSurfaceState();
}

class _MacTabSurfaceState extends State<MacTabSurface> {
  late final MacosTabController _tabs =
      widget.controller ??
      MacosTabController(initialIndex: 0, length: widget.labels.length);
  late final bool _ownsTabs = widget.controller == null;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
  }

  void _handleTabChanged() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    if (_ownsTabs) _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(
      ConfigMetrics.tabBodyInset,
      kMacTabStripGap,
      ConfigMetrics.tabBodyInset,
      ConfigMetrics.tabBodyInset,
    ),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Center(
          child: MacosSegmentedControl(
            controller: _tabs,
            tabs: [for (final label in widget.labels) MacosTab(label: label)],
          ),
        ),
        const SizedBox(height: kMacTabStripGap),
        Expanded(
          child: MacPanel(
            child: IndexedStack(index: _tabs.index, children: widget.children),
          ),
        ),
      ],
    ),
  );
}
```

更新 `configuration_workspace.dart`：
```dart
// 添加 import
import 'configuration/widgets/mac_tab_surface.dart';
import 'configuration/widgets/config_constants.dart';

// 删除原来的 _MacTabSurface 类定义（第 34-103 行）
// 删除原来的常量定义（第 19-27 行）

// 使用时改名
MacTabSurface(  // 原来是 _MacTabSurface
  labels: ...,
  children: ...,
)
```

测试：
```bash
flutter analyze lib
flutter test
flutter run -d macos
```

提交：
```bash
git add lib/configuration/widgets/mac_tab_surface.dart lib/configuration_workspace.dart
git commit -m "refactor: extract MacTabSurface to separate file"
```

---

### 阶段 2: 提取独立页面（每个 1-2 小时）

从最简单、依赖最少的页面开始。

#### 步骤 2.1: 提取 BatteryConfigurationPage

**原因**: 电池配置页面相对独立，依赖少

1. 在 `configuration_workspace.dart` 中找到 `_BatteryConfigurationPage`
2. 复制到新文件 `lib/configuration/battery/battery_configuration_page.dart`
3. 改为公有类: `class BatteryConfigurationPage`
4. 添加必要的 imports
5. 在原文件中删除定义，添加 import

```dart
// lib/configuration/battery/battery_configuration_page.dart
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/aligned_field_row.dart';

import '../widgets/config_constants.dart';

/// 电池配置页面
class BatteryConfigurationPage extends StatelessWidget {
  const BatteryConfigurationPage({
    super.key,
    required this.connectionController,
  });

  final ConnectionController connectionController;

  @override
  Widget build(BuildContext context) {
    // ... 原来的实现 ...
  }
}
```

```dart
// configuration_workspace.dart
import 'configuration/battery/battery_configuration_page.dart';

// 使用时
case 'battery':
  return BatteryConfigurationPage(
    connectionController: widget.connectionController,
  );
```

测试并提交：
```bash
flutter analyze lib
flutter test
git add lib/configuration/battery/ lib/configuration_workspace.dart
git commit -m "refactor: extract BatteryConfigurationPage"
```

#### 步骤 2.2-2.8: 依次提取其他简单页面

按以下顺序提取：
1. ✅ `BatteryConfigurationPage`
2. `SystemStatusPage`
3. `MotorOutputPage`
4. `FlightModesPage`
5. `PortsConfigurationPage`
6. `ArmingChecksPage`
7. `AllParametersPage`

每个页面：提取 → 测试 → 提交

---

### 阶段 3: 提取复杂模块 - 校准（4-6 小时）

**挑战**: 校准模块有多个子页面和共享组件

#### 步骤 3.1: 提取校准子组件

先提取独立的展示组件：

1. **CalibrationAircraft** - 飞机图示
```dart
// lib/configuration/calibration/widgets/calibration_aircraft.dart
class CalibrationAircraft extends StatelessWidget { }
```

2. **MagneticFieldStrength** - 磁场强度显示
```dart
// lib/configuration/calibration/widgets/magnetic_field_strength.dart
class MagneticFieldStrength extends StatelessWidget { }
```

3. **CalibrationProgress** - 校准进度条
```dart
// lib/configuration/calibration/widgets/calibration_progress.dart
class CalibrationProgress extends StatelessWidget { }
```

#### 步骤 3.2: 提取校准视图

1. **AccelerometerCalibrationView**
```dart
// lib/configuration/calibration/accelerometer_calibration_view.dart
class AccelerometerCalibrationView extends StatelessWidget {
  const AccelerometerCalibrationView({
    super.key,
    required this.connectionController,
  });

  final ConnectionController connectionController;
  
  // ... 实现 ...
}
```

2. **MagnetometerCalibrationView** - 包含其子组件
3. **RcCalibrationView** - 包含遥控器校准逻辑

#### 步骤 3.3: 提取校准主页面

```dart
// lib/configuration/calibration/calibration_page.dart
import 'accelerometer_calibration_view.dart';
import 'magnetometer_calibration_view.dart';
import 'rc_calibration_view.dart';
import '../widgets/mac_tab_surface.dart';

class CalibrationPage extends StatefulWidget {
  const CalibrationPage({
    super.key,
    required this.connectionController,
  });

  final ConnectionController connectionController;

  @override
  State<CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<CalibrationPage> {
  @override
  Widget build(BuildContext context) {
    return MacTabSurface(
      labels: const ['加速度计', '磁力计', '遥控器'],
      children: [
        AccelerometerCalibrationView(
          connectionController: widget.connectionController,
        ),
        MagnetometerCalibrationView(
          connectionController: widget.connectionController,
        ),
        RcCalibrationView(
          connectionController: widget.connectionController,
        ),
      ],
    );
  }
}
```

**注意**: 确保每个组件的 FFI 调用都有完整错误处理

---

### 阶段 4: 提取 PID 模块（4-6 小时）

**挑战**: PID 模块有复杂的参数结构和布局

#### 步骤 4.1: 提取 PID 数据结构

```dart
// lib/configuration/pid/models/pid_models.dart

/// PID 增益字段
class PidGainField {
  const PidGainField({
    required this.label,
    required this.paramName,
  });

  final String label;
  final String paramName;
}

/// PID 卡片配置
class PidCard {
  const PidCard({
    required this.title,
    required this.gains,
  });

  final String title;
  final List<PidGainField> gains;
}

/// PID 区块配置
class PidSection {
  const PidSection({
    required this.title,
    required this.cards,
  });

  final String title;
  final List<PidCard> cards;
}
```

#### 步骤 4.2: 提取 PID 组件

1. `PidSectionHeader`
2. `PidCardGrid`
3. `PidCardView`
4. `PidGainTile`

#### 步骤 4.3: 提取 PID 页面

1. `PidTuningPage` - 顶层页面
2. `PidParametersPage` - 参数编辑页面

---

### 阶段 5: 重构主入口（2-3 小时）

**目标**: 将 `configuration_workspace.dart` 简化为路由和框架

最终的 `configuration_workspace.dart` 应该只包含：

```dart
import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

import 'configuration/calibration/calibration_page.dart';
import 'configuration/pid/pid_tuning_page.dart';
import 'configuration/battery/battery_configuration_page.dart';
import 'configuration/motors/motor_output_page.dart';
import 'configuration/system/system_status_page.dart';
import 'configuration/flight_modes/flight_modes_page.dart';
import 'configuration/ports/ports_configuration_page.dart';
import 'configuration/arming/arming_checks_page.dart';
import 'configuration/parameters/all_parameters_page.dart';

/// 飞控配置工作区
///
/// 提供系统状态、校准、参数配置、PID 调参等功能的统一入口。
/// 使用侧边栏导航，每个功能模块在独立页面中实现。
class ConfigurationWorkspace extends StatefulWidget {
  const ConfigurationWorkspace({
    super.key,
    required this.onReturnToMap,
    required this.connectionController,
  });

  final VoidCallback onReturnToMap;
  final ConnectionController connectionController;

  @override
  State<ConfigurationWorkspace> createState() => _ConfigurationWorkspaceState();
}

class _ConfigurationWorkspaceState extends State<ConfigurationWorkspace> {
  static const _basePages = <_ConfigPageInfo>[
    _ConfigPageInfo('system', '系统状态', CupertinoIcons.heart),
    _ConfigPageInfo('calibration', '飞控校准', CupertinoIcons.scope),
    _ConfigPageInfo('ports', '端口配置', CupertinoIcons.antenna_radiowaves_left_right),
    _ConfigPageInfo('motors', '电机输出', CupertinoIcons.slider_horizontal_3),
    _ConfigPageInfo('flightModes', '飞行模式', CupertinoIcons.airplane),
    _ConfigPageInfo('pid', 'PID 调参', CupertinoIcons.waveform_path_ecg),
    _ConfigPageInfo('battery', '电池配置', CupertinoIcons.battery_100),
    _ConfigPageInfo('arming', '解锁检查', CupertinoIcons.checkmark_shield),
    _ConfigPageInfo('allParameters', '全部参数', CupertinoIcons.list_number),
  ];

  String _selectedPageId = 'motors';

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.connectionController.profileChanges,
    builder: (context, _) {
      final pages = _pagesForProfile(widget.connectionController.controlProfile);
      final selectedPageId = pages.any((page) => page.id == _selectedPageId)
          ? _selectedPageId
          : 'motors';
      final selectedIndex = pages.indexWhere((page) => page.id == selectedPageId);
      
      return AppWorkspaceShell(
        items: [
          for (final page in pages)
            AppWorkspaceItem(id: page.id, label: page.label, icon: page.icon),
        ],
        selectedIndex: selectedIndex,
        onItemSelected: (index) {
          setState(() {
            _selectedPageId = pages[index].id;
          });
        },
        onClose: widget.onReturnToMap,
        child: _buildPage(selectedPageId),
      );
    },
  );

  Widget _buildPage(String pageId) {
    switch (pageId) {
      case 'system':
        return SystemStatusPage(connectionController: widget.connectionController);
      case 'calibration':
        return CalibrationPage(connectionController: widget.connectionController);
      case 'ports':
        return PortsConfigurationPage(connectionController: widget.connectionController);
      case 'motors':
        return MotorOutputPage(connectionController: widget.connectionController);
      case 'flightModes':
        return FlightModesPage(connectionController: widget.connectionController);
      case 'pid':
        return PidTuningPage(connectionController: widget.connectionController);
      case 'battery':
        return BatteryConfigurationPage(connectionController: widget.connectionController);
      case 'arming':
        return ArmingChecksPage(connectionController: widget.connectionController);
      case 'allParameters':
        return AllParametersPage(connectionController: widget.connectionController);
      default:
        return const Center(child: Text('未实现'));
    }
  }

  List<_ConfigPageInfo> _pagesForProfile(String? profile) {
    // 根据飞控类型过滤可用页面
    return _basePages;
  }
}

class _ConfigPageInfo {
  const _ConfigPageInfo(this.id, this.label, this.icon);
  
  final String id;
  final String label;
  final IconData icon;
}
```

**目标行数**: ~200 行

---

## 验收清单

### 代码质量

- [ ] `flutter analyze lib` - 0 issues
- [ ] `flutter analyze test` - 0 issues
- [ ] 所有文件 < 500 行（理想 < 300 行）
- [ ] 没有重复的常量定义
- [ ] Import 路径清晰统一
- [ ] 所有公有类有文档注释

### 功能完整性

- [ ] 9 个配置页面都能正常打开
- [ ] 页面切换流畅无错误
- [ ] 校准流程完整可用：
  - [ ] 加速度计校准
  - [ ] 磁力计校准
  - [ ] 遥控器校准
- [ ] PID 调参页面：
  - [ ] 参数能正常加载
  - [ ] 参数能正常编辑
  - [ ] 参数能正常保存
- [ ] 电池配置页面正常
- [ ] 电机输出页面正常
- [ ] 系统状态页面正常
- [ ] 返回地图按钮正常工作

### 测试覆盖

- [ ] `flutter test` - 所有测试通过
- [ ] 测试覆盖率未降低
- [ ] 每个新模块都有对应测试：
  - [ ] calibration/
  - [ ] pid/
  - [ ] battery/
  - [ ] motors/
  - [ ] system/
- [ ] FFI 调用都有 mock 测试
- [ ] 错误处理都有测试

### 构建和运行

- [ ] `flutter build macos --debug` - 构建成功
- [ ] `flutter run -d macos` - 运行成功
- [ ] 手动测试所有页面无崩溃
- [ ] 真实飞控连接测试通过

### Git 历史

- [ ] 每个模块是独立提交
- [ ] 提交信息清晰
- [ ] 没有巨大的单次提交
- [ ] 提交历史可追溯

### 文档

- [ ] 更新 `flutter_shell/README.md` 架构说明
- [ ] 更新 ADR（如有架构变更）
- [ ] 代码注释完整

---

## 常见问题

### Q1: 拆分后 import 太多，很难管理

**A**: 使用桶文件（barrel file）：

```dart
// lib/configuration/calibration/calibration.dart
export 'calibration_page.dart';
export 'accelerometer_calibration_view.dart';
export 'magnetometer_calibration_view.dart';
export 'rc_calibration_view.dart';

// 使用时
import 'package:mgcs_frontend/configuration/calibration/calibration.dart';
// 而不是
// import 'package:mgcs_frontend/configuration/calibration/calibration_page.dart';
// import 'package:mgcs_frontend/configuration/calibration/accelerometer_calibration_view.dart';
// ...
```

### Q2: 拆分后文件太多，难以导航

**A**: 
1. 使用 IDE 的文件搜索（Cmd+P / Ctrl+P）
2. 按功能而非技术分组（calibration/ 而非 views/ + models/ + controllers/）
3. 保持目录深度 ≤ 3 层

### Q3: ConnectionController 在每个组件都要传递，很繁琐

**A**: 可以考虑使用 InheritedWidget 或 Provider：

```dart
// lib/connection/connection_provider.dart
class ConnectionProvider extends InheritedWidget {
  const ConnectionProvider({
    super.key,
    required this.controller,
    required super.child,
  });

  final ConnectionController controller;

  static ConnectionController of(BuildContext context) {
    final provider = context.dependOnInheritedWidgetOfExactType<ConnectionProvider>();
    assert(provider != null, 'No ConnectionProvider found in context');
    return provider!.controller;
  }

  @override
  bool updateShouldNotify(ConnectionProvider oldWidget) =>
      controller != oldWidget.controller;
}

// 使用
class MagnetometerCalibrationView extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final controller = ConnectionProvider.of(context);
    // ...
  }
}
```

但注意：
- 增加了间接性
- 可能影响测试（需要 mock InheritedWidget）
- 当前直接传递也没有太大问题

### Q4: 某个页面依赖多个状态，拆分很困难

**A**: 
1. 先识别状态归属：这个状态属于哪个页面？
2. 状态下沉：每个页面管理自己的状态
3. 如果确实需要共享状态，使用回调或状态管理库
4. 考虑是否真的需要共享，还是可以重新获取

### Q5: 拆分后代码行数反而增加了

**A**: 这是正常的：
- 每个文件需要 import 语句（~10 行）
- 每个文件需要文档注释（~5 行）
- 去重后的公有 API 需要类定义
- 但可维护性大大提升

总代码行数可能从 4099 增加到 ~4500，但：
- 每个文件 < 300 行，易于理解
- 模块边界清晰
- 测试和修改更容易

### Q6: 如何处理循环依赖？

**A**: 循环依赖说明设计有问题：

```dart
// ❌ 错误：循环依赖
// calibration_page.dart
import 'magnetometer_calibration_view.dart';  // A → B

// magnetometer_calibration_view.dart
import 'calibration_page.dart';  // B → A

// ✅ 正确：单向依赖
// calibration_page.dart (父)
import 'magnetometer_calibration_view.dart';  // 父 → 子

// magnetometer_calibration_view.dart (子)
// 不 import 父组件，通过回调通信
```

解决方法：
1. 提取共享代码到第三个文件
2. 使用回调而非直接调用
3. 重新设计模块边界

---

## 参考资料

### 项目文档

- [`AGENTS.md`](../../AGENTS.md) - 项目工作规则
- [`flutter_shell/README.md`](../../flutter_shell/README.md) - Flutter 架构说明
- [`docs/adr/0007-unified-font-stack.md`](../architecture/adr/0007-unified-font-stack.md) - 字体规范

### Flutter 最佳实践

- [Effective Dart: Style](https://dart.dev/guides/language/effective-dart/style)
- [Flutter Widget Catalog](https://docs.flutter.dev/ui/widgets)
- [Layouts in Flutter](https://docs.flutter.dev/ui/layout)

### 重构参考

- Martin Fowler - *Refactoring: Improving the Design of Existing Code*
- 重构原则：小步快跑，频繁测试，渐进式改进

---

## 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| 1.0 | 2026-10-08 | M-GCS Team | 初始版本 |

---

## 后续优化建议

拆分完成后，可以考虑的进一步优化：

1. **状态管理升级** - 引入 Riverpod 或 Bloc 统一状态管理
2. **测试覆盖提升** - 目标 80%+ 覆盖率
3. **性能优化** - 使用 DevTools 分析重建和性能瓶颈
4. **可访问性** - 添加语义标签和屏幕阅读器支持
5. **国际化** - 支持多语言切换

但这些都不是拆分任务的一部分，应该作为独立的改进任务进行。

---

**祝拆分顺利！有问题随时在 issue 中讨论。**
