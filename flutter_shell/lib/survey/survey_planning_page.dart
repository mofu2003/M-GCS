import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../mission/mission_document_models.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/adaptive_popup_button.dart';
import '../widgets/aligned_field_row.dart';
import '../widgets/mac_panel.dart';
import 'survey_controller.dart';
import 'survey_models.dart';

/// 测绘规划页：把测绘配置交给 native 生成航线，预览统计后并入任务文档。
///
/// 分工与 S1 保持同一条判据——原生是权威计算源：
/// - 页面只通过 [ConnectionController.surveyController] 触发生成；
/// - 航线几何、航点数量、航程与拍照数全部来自 `mg_survey_generate*` 的返回值；
/// - 生成是本地纯计算，不要求连接飞控；并入选中的是哪一架飞机由当前会话决定，
///   上传仍由任务面板负责，本页不假设「生成即上载成功」。
class SurveyPlanningPage extends StatefulWidget {
  const SurveyPlanningPage({
    super.key,
    required this.controller,
    this.scrollController,
  });

  final ConnectionController controller;
  final ScrollController? scrollController;

  @override
  State<SurveyPlanningPage> createState() => _SurveyPlanningPageState();
}

enum _SurveyMode { grid, crossGrid, corridor }

class _SurveyPlanningPageState extends State<SurveyPlanningPage> {
  static const _modeLabels = ['矩形网格', '交叉网格', '走廊扫描'];

  final MacosTabController _modeTabs = MacosTabController(
    initialIndex: 0,
    length: _modeLabels.length,
  );

  late final TextEditingController _centerLat;
  late final TextEditingController _centerLon;
  late final TextEditingController _width;
  late final TextEditingController _length;
  late final TextEditingController _angle;
  late final TextEditingController _altitude;
  late final TextEditingController _speed;
  late final TextEditingController _startLat;
  late final TextEditingController _startLon;
  late final TextEditingController _endLat;
  late final TextEditingController _endLon;
  late final TextEditingController _focalLength;
  late final TextEditingController _sensorWidth;
  late final TextEditingController _sensorHeight;
  late final TextEditingController _imageWidth;
  late final TextEditingController _imageHeight;
  late final TextEditingController _frontOverlap;
  late final TextEditingController _sideOverlap;

  /// 并入任务时写入的高度基准帧。default 与任务面板一致，相对 Home。
  int _frame = kMissionFrameRelativeAlt;

  SurveyResult? _result;
  String? _status;
  bool _statusIsError = false;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    _modeTabs.addListener(_handleModeChanged);

    final center = _defaultCenter();
    const grid = SurveyGridConfig(centerLat: 0, centerLon: 0);
    const corridor = SurveyCorridorConfig(
      startLat: 0,
      startLon: 0,
      endLat: 0,
      endLon: 0,
    );
    _centerLat = _field(center?.lat, );
    _centerLon = _field(center?.lon, );
    _width = _field(grid.widthM);
    _length = _field(grid.lengthM);
    _angle = _field(grid.angleDeg);
    _altitude = _field(grid.altitudeM);
    _speed = _field(grid.speedMps);
    _startLat = _field(center?.lat, );
    _startLon = _field(center?.lon, );
    _endLat = _field(center?.lat, );
    _endLon = _field(center?.lon, );
    _focalLength = _field(grid.camera.focalLengthMm);
    _sensorWidth = _field(grid.camera.sensorWidthMm);
    _sensorHeight = _field(grid.camera.sensorHeightMm);
    _imageWidth = _field(grid.camera.imageWidthPx);
    _imageHeight = _field(grid.camera.imageHeightPx);
    _frontOverlap = _field(grid.camera.minOverlapPct);
    _sideOverlap = _field(corridor.sideOverlapPct);
  }

  @override
  void dispose() {
    _modeTabs.removeListener(_handleModeChanged);
    _modeTabs.dispose();
    for (final controller in _allFields) {
      controller.dispose();
    }
    super.dispose();
  }

  List<TextEditingController> get _allFields => [
    _centerLat,
    _centerLon,
    _width,
    _length,
    _angle,
    _altitude,
    _speed,
    _startLat,
    _startLon,
    _endLat,
    _endLon,
    _focalLength,
    _sensorWidth,
    _sensorHeight,
    _imageWidth,
    _imageHeight,
    _frontOverlap,
    _sideOverlap,
  ];

  _SurveyMode get _mode => _SurveyMode.values[_modeTabs.index];

  void _handleModeChanged() {
    if (!mounted) return;
    setState(() {
      _result = null;
      _status = null;
    });
  }

  /// 默认中心：优先当前焦点飞机的当前位置，其次它的 Home。
  ///
  /// 两者都没有时返回 null，字段留空由用户填写——不用 0,0 冒充有效坐标，
  /// 避免生成一串落在几内亚湾的航线。
  ({double lat, double lon})? _defaultCenter() {
    final vehicle = widget.controller.fleetSnapshot.activeVehicle;
    if (vehicle == null) return null;
    if (vehicle.positionValid &&
        vehicle.latitude != 0 &&
        vehicle.longitude != 0) {
      return (lat: vehicle.latitude, lon: vehicle.longitude);
    }
    if (vehicle.homePositionValid &&
        vehicle.homeLatitude != 0 &&
        vehicle.homeLongitude != 0) {
      return (lat: vehicle.homeLatitude, lon: vehicle.homeLongitude);
    }
    return null;
  }

  /// 数值回填成输入框文本；null 留空。整数去掉多余的 ".0"，
  /// 让 200 显示成 200 而不是 200.0。
  static TextEditingController _field(double? value) {
    if (value == null) return TextEditingController(text: '');
    final text = value == value.roundToDouble()
        ? value.toStringAsFixed(0)
        : value.toString();
    return TextEditingController(text: text);
  }

  static double? _read(TextEditingController controller) =>
      double.tryParse(controller.text.trim());

  Future<void> _generate() async {
    final controller = widget.controller.surveyController;
    setState(() {
      _busy = true;
      _status = null;
      _statusIsError = false;
    });
    try {
      final result = _mode == _SurveyMode.corridor
          ? await _generateCorridor(controller)
          : await _generateGrid(controller);
      if (!mounted) return;
      setState(() {
        if (result == null) {
          _result = null;
          _statusIsError = true;
        } else if (result.valid) {
          _result = result;
          _statusIsError = false;
          _status =
              '已生成 ${result.waypointCount} 个航点、${result.flightLineCount} 条航线';
        } else {
          _result = null;
          _statusIsError = true;
          _status = result.message.isEmpty ? '测绘生成失败' : result.message;
        }
      });
    } on Object catch (error) {
      if (!mounted) return;
      setState(() {
        _result = null;
        _statusIsError = true;
        _status = error.toString().replaceFirst('Bad state: ', '');
      });
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<SurveyResult?> _generateGrid(SurveyController controller) {
    final camera = _camera();
    final lat = _read(_centerLat);
    final lon = _read(_centerLon);
    final width = _read(_width);
    final length = _read(_length);
    final angle = _read(_angle);
    final altitude = _read(_altitude);
    final speed = _read(_speed);
    if (camera == null ||
        lat == null ||
        lon == null ||
        width == null ||
        length == null ||
        angle == null ||
        altitude == null ||
        speed == null) {
      return Future.value(_invalid('请把区域与相机参数都填成有效数字'));
    }
    return controller.generateGrid(
      SurveyGridConfig(
        centerLat: lat,
        centerLon: lon,
        widthM: width,
        lengthM: length,
        altitudeM: altitude,
        speedMps: speed,
        angleDeg: angle,
        crossGrid: _mode == _SurveyMode.crossGrid,
        camera: camera,
      ),
    );
  }

  Future<SurveyResult?> _generateCorridor(SurveyController controller) {
    final camera = _camera();
    final startLat = _read(_startLat);
    final startLon = _read(_startLon);
    final endLat = _read(_endLat);
    final endLon = _read(_endLon);
    final width = _read(_width);
    final altitude = _read(_altitude);
    final speed = _read(_speed);
    final sideOverlap = _read(_sideOverlap);
    if (camera == null ||
        startLat == null ||
        startLon == null ||
        endLat == null ||
        endLon == null ||
        width == null ||
        altitude == null ||
        speed == null ||
        sideOverlap == null) {
      return Future.value(_invalid('请把走廊与相机参数都填成有效数字'));
    }
    return controller.generateCorridor(
      SurveyCorridorConfig(
        startLat: startLat,
        startLon: startLon,
        endLat: endLat,
        endLon: endLon,
        widthM: width,
        altitudeM: altitude,
        speedMps: speed,
        sideOverlapPct: sideOverlap,
        camera: camera,
      ),
    );
  }

  SurveyCameraSpec? _camera() {
    final focal = _read(_focalLength);
    final sensorWidth = _read(_sensorWidth);
    final sensorHeight = _read(_sensorHeight);
    final imageWidth = _read(_imageWidth);
    final imageHeight = _read(_imageHeight);
    final front = _read(_frontOverlap);
    final side = _read(_sideOverlap);
    if (focal == null ||
        sensorWidth == null ||
        sensorHeight == null ||
        imageWidth == null ||
        imageHeight == null ||
        front == null ||
        side == null) {
      return null;
    }
    return SurveyCameraSpec(
      focalLengthMm: focal,
      sensorWidthMm: sensorWidth,
      sensorHeightMm: sensorHeight,
      imageWidthPx: imageWidth,
      imageHeightPx: imageHeight,
      minOverlapPct: front,
      sideOverlapPct: side,
    );
  }

  SurveyResult _invalid(String message) =>
      SurveyResult(valid: false, message: message);

  Future<void> _applyToMission() async {
    final result = _result;
    if (result == null || !result.valid) return;
    setState(() {
      _busy = true;
      _status = null;
      _statusIsError = false;
    });
    try {
      final mission = widget.controller.missionPlanning;
      final added = await widget.controller.surveyController.applyToMission(
        mission: mission,
        result: result,
        frame: _frame,
      );
      if (!mounted) return;
      setState(() {
        _statusIsError = false;
        _status = added == 0 ? '没有可并入的航点' : '已并入任务：追加 $added 个航点';
      });
    } on Object catch (error) {
      if (!mounted) return;
      setState(() {
        _statusIsError = true;
        _status = error.toString().replaceFirst('Bad state: ', '');
      });
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final corridor = _mode == _SurveyMode.corridor;
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
          Text(
            '测绘规划',
            style: MgTypography.sans(
              MgTypography.title.copyWith(color: colors.textPrimary),
            ),
          ),
          const SizedBox(height: MgMetrics.configSectionGap),
          Center(
            child: MacosSegmentedControl(
              controller: _modeTabs,
              tabs: [
                for (final label in _modeLabels) MacosTab(label: label),
              ],
            ),
          ),
          const SizedBox(height: MgMetrics.configSectionGap),
          _section(context, corridor ? '走廊' : '区域与航线'),
          MacPanel(
            padding: const EdgeInsets.all(12),
            child: Column(
              children: [
                if (corridor) ...[
                  _row('起点纬度', _startLat),
                  _row('起点经度', _startLon),
                  _row('终点纬度', _endLat),
                  _row('终点经度', _endLon),
                ] else ...[
                  _row('中心纬度', _centerLat),
                  _row('中心经度', _centerLon),
                  _row('东西宽 (m)', _width),
                  _row('南北长 (m)', _length),
                  _row('旋转角 (°)', _angle),
                ],
                _row(
                  _frame == kMissionFrameTerrainAlt ? '航高 (m, AGL)' : '航高 (m)',
                  _altitude,
                ),
                _row('速度 (m/s)', _speed),
                _choiceRow(
                  '高度基准',
                  value: _frame,
                  options: [for (final choice in kMissionFrameChoices) choice.$1],
                  labelOf: missionFrameLabel,
                  onChanged: (frame) => setState(() => _frame = frame),
                ),
                if (_frame == kMissionFrameTerrainAlt) ...[
                  const SizedBox(height: 4),
                  Text(
                    '地形跟随由飞控按地形数据执行，需要飞控载有该区域的地形数据；'
                    'M-GCS 不替代飞控回发地形（见 survey-and-terrain.md 的 D4）。',
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(
                        color: colors.textSecondary,
                      ),
                    ),
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: MgMetrics.configSectionGap),
          _section(context, '相机与重叠'),
          MacPanel(
            padding: const EdgeInsets.all(12),
            child: Column(
              children: [
                _row('焦距 (mm)', _focalLength),
                _row('传感器宽 (mm)', _sensorWidth),
                _row('传感器高 (mm)', _sensorHeight),
                _row('照片宽 (px)', _imageWidth),
                _row('照片高 (px)', _imageHeight),
                _row('航向重叠 (%)', _frontOverlap),
                // 旁向重叠同时决定相机列距与走廊线距；用同一个输入框，
                // 不把「相机重叠」和「走廊线距重叠」拆成两个互相打架的字段。
                _row('旁向重叠 (%)', _sideOverlap),
              ],
            ),
          ),
          const SizedBox(height: MgMetrics.configSectionGap),
          Row(
            children: [
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: _busy ? null : _generate,
                child: const Text('生成预览'),
              ),
              const SizedBox(width: 10),
              if (_busy)
                const ProgressCircle(radius: 7)
              else if (_status != null)
                Expanded(
                  child: Text(
                    _status!,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(
                        color: _statusIsError ? colors.danger : colors.textSecondary,
                      ),
                    ),
                  ),
                ),
            ],
          ),
          if (_result != null) ...[
            const SizedBox(height: MgMetrics.configSectionGap),
            _section(context, '预览'),
            MacPanel(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  _stat('航点', '${_result!.waypointCount}'),
                  _stat('航线', '${_result!.flightLineCount} 条'),
                  _stat('航程', _distance(_result!.totalDistanceM)),
                  _stat('预计用时', _duration(_result!.estimatedTimeS)),
                  _stat('覆盖面积', _area(_result!.areaM2)),
                  _stat('拍照', '${_result!.photoCount} 张'),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: _busy ? null : _applyToMission,
                      child: const Text('并入任务'),
                    ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    '并入后仍要在航线规划里确认 Home 并上传到飞控；本页不会自动上传。',
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(color: colors.textSecondary),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _section(BuildContext context, String title) => Padding(
    padding: const EdgeInsets.only(bottom: MgMetrics.configHeaderGap),
    child: Text(
      title,
      style: MgTypography.sans(
        MgTypography.section.copyWith(color: context.mgColors.textPrimary),
      ),
    ),
  );

  Widget _row(String label, TextEditingController controller) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 4),
    child: AlignedFieldRow(
      label: label,
      child: MacosTextField(
        controller: controller,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
        style: MgTypography.sans(
          MgTypography.body.copyWith(color: context.mgColors.textPrimary),
        ),
      ),
    ),
  );

  Widget _choiceRow<T>(
    String label, {
    required T value,
    required List<T> options,
    required String Function(T) labelOf,
    required ValueChanged<T> onChanged,
  }) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 4),
    child: AlignedFieldRow(
      label: label,
      child: Align(
        alignment: Alignment.centerLeft,
        child: AdaptivePopupButton<T>(
          value: value,
          options: options,
          labelOf: labelOf,
          onChanged: onChanged,
        ),
      ),
    ),
  );

  Widget _stat(String label, String value) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 3),
    child: Row(
      children: [
        SizedBox(
          width: 84,
          child: Text(
            label,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: context.mgColors.textSecondary),
            ),
          ),
        ),
        Text(
          value,
          style: MgTypography.sans(
            MgTypography.data.copyWith(color: context.mgColors.textPrimary),
          ),
        ),
      ],
    ),
  );

  static String _distance(double meters) => meters >= 1000
      ? '${(meters / 1000).toStringAsFixed(2)} km'
      : '${meters.round()} m';

  static String _duration(double seconds) {
    if (seconds < 60) return '${seconds.round()} s';
    final minutes = seconds / 60;
    return minutes < 60
        ? '${minutes.toStringAsFixed(1)} min'
        : '${(minutes / 60).toStringAsFixed(1)} h';
  }

  static String _area(double squareMeters) => squareMeters >= 10000
      ? '${(squareMeters / 10000).toStringAsFixed(2)} ha'
      : '${squareMeters.round()} m²';
}
