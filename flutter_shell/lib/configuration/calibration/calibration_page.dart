import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';

import '../widgets/mac_tab_surface.dart';
import 'accelerometer_calibration_view.dart';
import 'magnetometer_calibration_view.dart';
import 'rc_calibration_view.dart';

/// 校准标签页入口，统一控制前两页的波形轮询并在退出时关闭。
class CalibrationPage extends StatefulWidget {
  const CalibrationPage({super.key, required this.controller});

  final ConnectionController controller;

  @override
  State<CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<CalibrationPage> {
  /// 加速度校准 / 磁力计校准 / 空速计校准 / 遥控器校准
  static const int _pageCount = 4;

  late final MacosTabController _tabs = MacosTabController(
    initialIndex: 0,
    length: _pageCount,
  );

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
    _syncWaveformPolling();
    // 磁力计标签页的两个下拉（安装方向、容差等级）选项来自飞控描述库，
    // 不登记元数据的话下拉里连一个选项都没有。
    widget.controller.loadParameterMetadata([
      'COMPASS_ORIENT',
      'COMPASS_CAL_FIT',
    ]);
  }

  void _handleTabChanged() {
    if (!mounted) return;
    _syncWaveformPolling();
    setState(() {});
  }

  /// 前两页显示波形，只有停在这两页时才向后端取数。
  /// 两个面板同时挂在树上，所以取数开关必须由页面统一决定，
  /// 不能让面板各自调用，否则会互相覆盖。
  void _syncWaveformPolling() {
    widget.controller.setWaveformActive(_tabs.index <= 1);
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    widget.controller.setWaveformActive(false);
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MacTabSurface(
    controller: _tabs,
    labels: const ['加速度校准', '磁力计校准', '空速计校准', '遥控器校准'],
    children: [
      AccelerometerCalibrationView(controller: widget.controller),
      MagnetometerCalibrationView(controller: widget.controller),
      const SizedBox.expand(),
      RcCalibrationView(controller: widget.controller),
    ],
  );
}
