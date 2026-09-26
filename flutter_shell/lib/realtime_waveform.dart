import 'dart:math' as math;
import 'dart:typed_data';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'widgets/adaptive_popup_button.dart';
import 'widgets/mac_panel.dart';

/// 后端波形缓冲的点布局：相对毫秒 + 9 个轴。
const int _kFloatsPerPoint = 10;

/// 各传感器在点里的通道起始下标，和后端 WaveformChannel 顺序一致。
const int _kAccelChannel = 1;
const int _kGyroChannel = 4;
const int _kMagChannel = 7;

const List<Color> _kAccelColors = [
  Color(0xFFFF375F),
  Color(0xFF30D158),
  Color(0xFF0A84FF),
];

const List<Color> _kGyroColors = [
  Color(0xFFFF7A00),
  Color(0xFFAF52DE),
  Color(0xFF14B8C4),
];

const List<Color> _kMagColors = [
  Color(0xFFB25000),
  Color(0xFF34A853),
  Color(0xFF5E5CE6),
];

/// 一个传感器通道组。
class _SignalGroup {
  const _SignalGroup({
    required this.title,
    required this.unit,
    required this.channel,
    required this.colors,
  });

  final String title;
  final String unit;
  final int channel;
  final List<Color> colors;

  static const List<String> axes = ['X', 'Y', 'Z'];
}

const List<_SignalGroup> _kGroups = [
  _SignalGroup(
    title: '加速度计',
    unit: 'm/s²',
    channel: _kAccelChannel,
    colors: _kAccelColors,
  ),
  _SignalGroup(
    title: '陀螺仪',
    unit: '°/s',
    channel: _kGyroChannel,
    colors: _kGyroColors,
  ),
  _SignalGroup(
    title: '磁力计',
    unit: 'G',
    channel: _kMagChannel,
    colors: _kMagColors,
  ),
];

/// 传感器波形面板：加速度计、陀螺仪、磁力计三条九轴实时曲线。
///
/// 采样和抽稀都在后端完成，这里只把后端环形缓冲里的点画出来，
/// 面板本身没有采样定时器，也不攒任何历史数据。
///
/// [active] 由所在标签页控制：只有当前可见时才向后端取数。
class SensorWaveformPanel extends StatefulWidget {
  const SensorWaveformPanel({
    super.key,
    required this.controller,
    this.active = true,
  });

  final ConnectionController controller;
  final bool active;

  @override
  State<SensorWaveformPanel> createState() => _SensorWaveformPanelState();
}

class _SensorWaveformPanelState extends State<SensorWaveformPanel> {
  final Stopwatch _clock = Stopwatch()..start();
  final Map<int, _RangeTracker> _ranges = {
    for (final group in _kGroups) group.channel: _RangeTracker(),
  };
  final Map<int, LineChartData?> _charts = {};
  int _cachedRevision = -1;
  Brightness? _cachedBrightness;

  @override
  void initState() {
    super.initState();
    if (widget.active) widget.controller.setWaveformActive(true);
  }

  @override
  void didUpdateWidget(covariant SensorWaveformPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.setWaveformActive(false);
      _cachedRevision = -1;
      if (widget.active) widget.controller.setWaveformActive(true);
      return;
    }
    if (oldWidget.active != widget.active) {
      widget.controller.setWaveformActive(widget.active);
    }
  }

  @override
  void dispose() {
    widget.controller.setWaveformActive(false);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.waveformChanges,
    builder: (context, _) {
      final controller = widget.controller;
      final view = _WaveformView(controller);
      final brightness = MacosTheme.brightnessOf(context);
      // 图表只在有新数据时重建，普通重建直接复用缓存。
      if (_cachedRevision != controller.waveformChanges.value ||
          _cachedBrightness != brightness) {
        _cachedRevision = controller.waveformChanges.value;
        _cachedBrightness = brightness;
        final nowMs = _clock.elapsedMilliseconds.toDouble();
        for (final group in _kGroups) {
          _charts[group.channel] = view.isEmpty
              ? null
              : _buildChart(
                  view,
                  brightness,
                  group,
                  _ranges[group.channel]!,
                  nowMs,
                );
        }
      }

      return SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(12, 12, 12, 16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _PanelHeader(controller: controller),
            const SizedBox(height: 10),
            LayoutBuilder(
              builder: (context, constraints) {
                final columns = constraints.maxWidth >= 1180
                    ? 3
                    : (constraints.maxWidth >= 780 ? 2 : 1);
                final width =
                    (constraints.maxWidth - (columns - 1) * 12) / columns;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final group in _kGroups)
                      SizedBox(
                        width: width,
                        child: _SignalCard(
                          group: group,
                          values: view.latestValues(group.channel),
                          chart: _charts[group.channel],
                        ),
                      ),
                  ],
                );
              },
            ),
          ],
        ),
      );
    },
  );

  LineChartData _buildChart(
    _WaveformView view,
    Brightness brightness,
    _SignalGroup group,
    _RangeTracker tracker,
    double nowMs,
  ) {
    final bars = <LineChartBarData>[];
    var minValue = double.infinity;
    var maxValue = double.negativeInfinity;

    for (var axis = 0; axis < group.colors.length; axis++) {
      final index = group.channel + axis;
      final spots = <FlSpot>[];
      for (var point = 0; point < view.length; point++) {
        final value = view.valueAt(point, index);
        if (!value.isFinite) continue;
        if (value < minValue) minValue = value;
        if (value > maxValue) maxValue = value;
        spots.add(FlSpot(view.timeAt(point), value));
      }
      if (spots.isEmpty) continue;
      bars.add(
        LineChartBarData(
          color: group.colors[axis],
          barWidth: 1.6,
          isCurved: false,
          dotData: const FlDotData(show: false),
          spots: spots,
        ),
      );
    }

    final range = tracker.update(minValue, maxValue, nowMs);
    final spanMs = math.max(1.0, view.spanMs);
    final xInterval = _niceTimeStep(spanMs / 4);
    final labelDigits = xInterval < 1000 ? 1 : 0;
    return LineChartData(
      minX: 0,
      maxX: spanMs,
      minY: range.min,
      maxY: range.max,
      lineBarsData: bars,
      gridData: FlGridData(
        show: true,
        drawVerticalLine: false,
        horizontalInterval: range.step,
        getDrawingHorizontalLine: (value) => FlLine(
          color: brightness == Brightness.dark
              ? Colors.white.withValues(alpha: 0.08)
              : Colors.black.withValues(alpha: 0.08),
          strokeWidth: 1,
        ),
      ),
      borderData: FlBorderData(show: false),
      titlesData: FlTitlesData(
        topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
        rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
        leftTitles: AxisTitles(
          sideTitles: SideTitles(
            showTitles: true,
            reservedSize: 44,
            interval: range.step,
            getTitlesWidget: (value, meta) => SideTitleWidget(
              meta: meta,
              space: 6,
              fitInside: SideTitleFitInsideData.fromTitleMeta(
                meta,
                distanceFromEdge: 4,
              ),
              child: Text(
                value.toStringAsFixed(range.decimals),
                style: _axisLabelStyle(brightness),
              ),
            ),
          ),
        ),
        bottomTitles: AxisTitles(
          sideTitles: SideTitles(
            showTitles: true,
            reservedSize: 22,
            interval: xInterval,
            getTitlesWidget: (value, meta) {
              // 横轴是相对时间：0 表示现在，负值表示距现在多少秒。
              final seconds = (value - spanMs) / 1000;
              final label = seconds.abs() < xInterval / 2000
                  ? '0s'
                  : '${seconds.toStringAsFixed(labelDigits)}s';
              return SideTitleWidget(
                meta: meta,
                space: 6,
                fitInside: SideTitleFitInsideData.fromTitleMeta(
                  meta,
                  distanceFromEdge: 4,
                ),
                child: Text(label, style: _axisLabelStyle(brightness)),
              );
            },
          ),
        ),
      ),
      lineTouchData: const LineTouchData(enabled: false),
    );
  }

  TextStyle _axisLabelStyle(Brightness brightness) => TextStyle(
    fontSize: 10,
    color: brightness == Brightness.dark ? Colors.white70 : Colors.black54,
  );
}

/// 把原始极值向外取整到 1/2/5 阶梯，得到一个稳定的纵轴量程。
_AxisRange _fitRange(double minValue, double maxValue) {
  if (!minValue.isFinite || !maxValue.isFinite) {
    return const _AxisRange(min: -1, max: 1, step: 0.5, decimals: 1);
  }
  var span = maxValue - minValue;
  if (span < 1e-3) span = 1;
  final step = _niceStep(span / 4);
  final min = (minValue / step).floorToDouble() * step;
  final max = (maxValue / step).ceilToDouble() * step;
  return _AxisRange(
    min: min,
    max: max,
    step: step,
    decimals: step >= 1 ? 0 : 1,
  );
}

double _niceStep(double raw) {
  if (!raw.isFinite || raw <= 0) return 1;
  final magnitude = math
      .pow(10, (math.log(raw) / math.ln10).floor())
      .toDouble();
  final normalized = raw / magnitude;
  final factor = normalized <= 1
      ? 1.0
      : normalized <= 2
      ? 2.0
      : normalized <= 5
      ? 5.0
      : 10.0;
  return factor * magnitude;
}

/// 时间轴刻度按秒取整，保证标签是整齐的秒数。
double _niceTimeStep(double raw) {
  const candidates = <double>[
    100,
    200,
    500,
    1000,
    2000,
    5000,
    10000,
    15000,
    30000,
    60000,
    120000,
    300000,
  ];
  for (final candidate in candidates) {
    if (raw <= candidate) return candidate;
  }
  return candidates.last;
}

/// 纵轴量程跟踪：数据变大立刻跟随，数据变小要稳定一段时间才收缩，
/// 这样曲线不会因为每帧重算量程而上下跳。
class _RangeTracker {
  static const double _shrinkDelayMs = 3000;

  _AxisRange? _range;
  double _steadySinceMs = 0;

  _AxisRange update(double minValue, double maxValue, double nowMs) {
    final fit = _fitRange(minValue, maxValue);
    final current = _range;
    final outOfRange =
        current == null ||
        fit.min < current.min - 1e-9 ||
        fit.max > current.max + 1e-9;
    if (outOfRange) {
      _range = fit;
      _steadySinceMs = nowMs;
      return fit;
    }
    final comfortablyInside =
        (fit.max - fit.min) < (current.max - current.min) * 0.6;
    if (!comfortablyInside) {
      _steadySinceMs = nowMs;
      return current;
    }
    if (nowMs - _steadySinceMs >= _shrinkDelayMs) {
      _range = fit;
      _steadySinceMs = nowMs;
      return fit;
    }
    return current;
  }
}

class _AxisRange {
  const _AxisRange({
    required this.min,
    required this.max,
    required this.step,
    required this.decimals,
  });

  final double min;
  final double max;
  final double step;
  final int decimals;
}

/// 后端波形缓冲的只读视图，面板不再拷贝一份数据。
class _WaveformView {
  _WaveformView(ConnectionController controller)
    : _data = controller.waveformPoints,
      length = controller.waveformPointCount;

  final Float32List _data;
  final int length;

  bool get isEmpty => length == 0;

  double timeAt(int point) => _data[point * _kFloatsPerPoint];

  double valueAt(int point, int channel) =>
      _data[point * _kFloatsPerPoint + channel];

  double get spanMs => length == 0 ? 0 : timeAt(length - 1);

  List<double> latestValues(int firstChannel) => [
    for (var axis = 0; axis < _SignalGroup.axes.length; axis++)
      length == 0 ? double.nan : valueAt(length - 1, firstChannel + axis),
  ];
}

/// 页面标题由工作区顶栏提供，这里只放刷新率和数据状态。
class _PanelHeader extends StatelessWidget {
  const _PanelHeader({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final typography = MacosTheme.of(context).typography;
    return Row(
      children: [
        Text('刷新率', style: typography.caption1),
        const SizedBox(width: 8),
        SizedBox(
          width: 96,
          child: AdaptivePopupButton<int>(
            value: controller.waveformRateHz,
            options: ConnectionController.waveformRateOptions,
            labelOf: (rate) => '$rate Hz',
            onChanged: controller.setWaveformRateHz,
          ),
        ),
        const Spacer(),
        Label(
          icon: MacosIcon(
            controller.isConnected
                ? CupertinoIcons.dot_radiowaves_left_right
                : CupertinoIcons.pause,
            size: 14,
          ),
          text: Text(controller.isConnected ? '实时数据' : '等待连接'),
        ),
      ],
    );
  }
}

class _SignalCard extends StatelessWidget {
  const _SignalCard({
    required this.group,
    required this.values,
    required this.chart,
  });

  final _SignalGroup group;
  final List<double> values;
  final LineChartData? chart;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final brightness = MacosTheme.brightnessOf(context);
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text(group.title, style: theme.typography.title3),
              const SizedBox(width: 8),
              Label(
                text: Text(
                  group.unit,
                  style: const TextStyle(color: MacosColors.systemBlueColor),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              for (var axis = 0; axis < _SignalGroup.axes.length; axis++)
                Expanded(
                  child: Padding(
                    padding: EdgeInsets.only(
                      right: axis == _SignalGroup.axes.length - 1 ? 0 : 8,
                    ),
                    child: _CurrentValueTile(
                      label: _SignalGroup.axes[axis],
                      value: values[axis],
                      color: group.colors[axis],
                    ),
                  ),
                ),
            ],
          ),
          const SizedBox(height: 10),
          SizedBox(
            height: 170,
            child: DecoratedBox(
              decoration: BoxDecoration(
                color: brightness.resolve(
                  const Color(0xFFFDFDFD),
                  const Color(0xFF202124),
                ),
                border: Border.all(color: theme.dividerColor),
                borderRadius: BorderRadius.circular(10),
              ),
              child: chart == null
                  ? Center(
                      child: Text('等待飞控数据', style: theme.typography.caption1),
                    )
                  : Padding(
                      padding: const EdgeInsets.fromLTRB(8, 8, 14, 6),
                      // 波形是流式数据，关掉切换动画，避免动画叠加造成抖动和卡顿。
                      child: LineChart(chart!, duration: Duration.zero),
                    ),
            ),
          ),
        ],
      ),
    );
  }
}

class _CurrentValueTile extends StatelessWidget {
  const _CurrentValueTile({
    required this.label,
    required this.value,
    required this.color,
  });

  final String label;
  final double value;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 9),
    decoration: BoxDecoration(
      color: MacosTheme.brightnessOf(
        context,
      ).resolve(const Color(0xFFFFFFFF), const Color(0xFF303136)),
      border: Border(left: BorderSide(color: color, width: 4)),
      borderRadius: BorderRadius.circular(9),
    ),
    child: Row(
      children: [
        Text(label, style: const TextStyle(fontWeight: FontWeight.w700)),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            value.isFinite ? value.toStringAsFixed(2) : '—',
            textAlign: TextAlign.right,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(fontWeight: FontWeight.w700),
          ),
        ),
      ],
    ),
  );
}
