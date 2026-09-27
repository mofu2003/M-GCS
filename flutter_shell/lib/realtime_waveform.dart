import 'dart:math' as math;
import 'dart:typed_data';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'theme/mg_colors.dart';
import 'widgets/mac_panel.dart';

/// 后端波形缓冲的点布局：相对毫秒 + 9 个轴。
const int _kFloatsPerPoint = 10;

/// 各传感器在点里的通道起始下标，和后端 WaveformChannel 顺序一致。
const int _kAccelChannel = 1;
const int _kGyroChannel = 4;
const int _kMagChannel = 7;

/// 信号卡片的内容内边距。实例选择器要按同样的缩进，才能和波形图左端对齐。
const double _kCardPadding = 14;

/// 面板头部固定高度。
///
/// 头部可能只放状态标签（磁力计页），也可能放实例选择器（IMU 页）。
/// 两者的实际高度不同，会让面板内容整体上下浮动、标签卡下沿对不齐，
/// 所以这里固定行高：取值需 >= 分段控件高度（23）。
const double _kPanelHeaderHeight = 24;

/// 波形内容区左右内边距。
///
/// 需要和波形卡片左右边界对齐的页面（例如校准页的卡片行）复用这个值，
/// 不要各写一套，否则两侧边界会对不齐。
const double kWaveformContentInset = 12;

/// 同一行相邻卡片之间的水平间距，含义同 [kWaveformContentInset]。
const double kWaveformCardGap = 12;

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

/// 面板里可以显示哪几组传感器。
enum SensorWaveformGroup { accel, gyro, mag }

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

const Map<SensorWaveformGroup, _SignalGroup> _kGroupConfig = {
  SensorWaveformGroup.accel: _SignalGroup(
    title: '加速度计',
    unit: 'm/s²',
    channel: _kAccelChannel,
    colors: _kAccelColors,
  ),
  SensorWaveformGroup.gyro: _SignalGroup(
    title: '陀螺仪',
    unit: '°/s',
    channel: _kGyroChannel,
    colors: _kGyroColors,
  ),
  SensorWaveformGroup.mag: _SignalGroup(
    title: '磁力计',
    unit: 'G',
    channel: _kMagChannel,
    colors: _kMagColors,
  ),
};

/// 传感器波形面板：加速度计、陀螺仪、磁力计三条九轴实时曲线。
///
/// 采样和抽稀都在后端完成，这里只把后端环形缓冲里的点画出来，
/// 面板本身没有采样定时器，也不攒任何历史数据。
///
/// [instanceKind] 决定面板跟着哪一类实例走：加速度计/陀螺仪用 IMU 实例，
/// 磁力计用磁力计实例，两者的数量由后端按实际收到的报文识别。
///
/// 取数开关由外层页面控制（[ConnectionController.setWaveformActive]），
/// 因为同一个页面里可能同时挂着多个面板，由面板各自开关会互相覆盖。
class SensorWaveformPanel extends StatefulWidget {
  const SensorWaveformPanel({
    super.key,
    required this.controller,
    required this.groups,
    required this.instanceKind,
  });

  final ConnectionController controller;
  final List<SensorWaveformGroup> groups;
  final WaveformInstanceKind instanceKind;

  @override
  State<SensorWaveformPanel> createState() => _SensorWaveformPanelState();
}

class _SensorWaveformPanelState extends State<SensorWaveformPanel> {
  final Stopwatch _clock = Stopwatch()..start();
  final Map<int, _RangeTracker> _ranges = {};
  final Map<int, LineChartData?> _charts = {};
  int _cachedRevision = -1;
  int _cachedInstance = -1;
  Brightness? _cachedBrightness;

  List<_SignalGroup> get _groups => [
    for (final group in widget.groups) _kGroupConfig[group]!,
  ];

  _RangeTracker _trackerFor(int channel) =>
      _ranges.putIfAbsent(channel, _RangeTracker.new);

  @override
  void didUpdateWidget(covariant SensorWaveformPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    // 数据源或通道组变了就丢掉旧图，避免沿用上一组传感器的量程。
    if (oldWidget.controller != widget.controller ||
        oldWidget.groups != widget.groups ||
        oldWidget.instanceKind != widget.instanceKind) {
      _ranges.clear();
      _charts.clear();
      _cachedRevision = -1;
      _cachedInstance = -1;
    }
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.waveformChanges,
    builder: (context, _) {
      final controller = widget.controller;
      final groups = _groups;
      final instances = controller.waveformInstances(widget.instanceKind);
      final selected = controller.waveformInstanceIndex(widget.instanceKind);
      // 换了实例就整体重来：量程和曲线都属于上一个传感器，不能沿用。
      if (_cachedInstance != selected) {
        _cachedInstance = selected;
        _ranges.clear();
        _charts.clear();
        _cachedRevision = -1;
      }
      final view = _WaveformView(controller, widget.instanceKind);
      final brightness = MacosTheme.brightnessOf(context);
      final colors = MgColors.of(context);
      // 图表只在有新数据时重建，普通重建直接复用缓存。
      if (_cachedRevision != controller.waveformChanges.value ||
          _cachedBrightness != brightness) {
        _cachedRevision = controller.waveformChanges.value;
        _cachedBrightness = brightness;
        final nowMs = _clock.elapsedMilliseconds.toDouble();
        for (final group in groups) {
          _charts[group.channel] = view.isEmpty
              ? null
              : _buildChart(
                  view,
                  colors,
                  group,
                  _trackerFor(group.channel),
                  nowMs,
                );
        }
      }

      return SingleChildScrollView(
        // 底部不留内边距：波形区下面紧接的内容（校准卡片）自己控制两者间距，
        // 否则面板内边距和页面间距会叠加成一大段空白。
        padding: const EdgeInsets.fromLTRB(
          kWaveformContentInset,
          12,
          kWaveformContentInset,
          0,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _PanelHeader(
              controller: controller,
              kind: widget.instanceKind,
              instances: instances,
              selected: selected,
            ),
            const SizedBox(height: 10),
            LayoutBuilder(
              builder: (context, constraints) {
                final maxColumns = constraints.maxWidth >= 1180
                    ? 3
                    : (constraints.maxWidth >= 780 ? 2 : 1);
                // 只有一组时铺满整行，不必留出多余的空位。
                final columns = math.min(maxColumns, groups.length);
                final width =
                    (constraints.maxWidth - (columns - 1) * kWaveformCardGap) /
                    columns;
                return Wrap(
                  spacing: kWaveformCardGap,
                  runSpacing: 12,
                  children: [
                    for (final group in groups)
                      SizedBox(
                        width: width,
                        child: _SignalCard(
                          group: group,
                          values: view.latestValues(group.channel),
                          chart: _charts[group.channel],
                          sensorName: _sensorName(instances, selected),
                          sensorMounting:
                              widget.instanceKind == WaveformInstanceKind.mag
                              ? _magnetometerMounting(controller, selected)
                              : '',
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
    MgColors colors,
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
        getDrawingHorizontalLine: (value) =>
            FlLine(color: colors.hairline, strokeWidth: 1),
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
                style: _axisLabelStyle(colors),
              ),
            ),
          ),
        ),
        bottomTitles: AxisTitles(
          // 只保留时间窗口，不画横轴刻度标签。
          sideTitles: SideTitles(showTitles: false, reservedSize: 0),
        ),
      ),
      lineTouchData: const LineTouchData(enabled: false),
    );
  }

  TextStyle _axisLabelStyle(MgColors colors) =>
      TextStyle(fontSize: 10, color: colors.textTertiary);

  /// 当前选中实例的传感器型号，后端还没解出型号时返回空串。
  static String _sensorName(List<WaveformInstance> instances, int selected) {
    for (final instance in instances) {
      if (instance.index == selected) return instance.name;
    }
    return '';
  }

  static String _magnetometerMounting(
    ConnectionController controller,
    int selected,
  ) {
    final suffix = selected == 0 ? '' : '${selected + 1}';
    final value = controller
        .parameter('COMPASS_EXTERNAL$suffix')
        ?.value
        .round();
    return switch (value) {
      1 => '外置',
      2 => '内置',
      _ => '',
    };
  }
}

/// 把原始极值向外取整到 1/2/5 阶梯，得到一个稳定的纵轴量程。
///
/// [headroom] 会先把极值向外放宽这么多比例再取整。放宽值参与量程计算但不改数据，
/// 作用是对"随后的小幅增长"留出余量，避免量程被一点点推着连续变化。
_AxisRange _fitRange(double minValue, double maxValue, {double headroom = 0}) {
  if (!minValue.isFinite || !maxValue.isFinite) {
    return const _AxisRange(min: -1, max: 1, step: 0.5, decimals: 1);
  }
  var span = maxValue - minValue;
  if (span < 1e-3) span = 1;
  if (headroom > 0) {
    final padding = span * headroom;
    minValue -= padding;
    maxValue += padding;
    span = maxValue - minValue;
  }
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

/// 纵轴量程跟踪。
///
/// 工业趋势图的量程必须能"稳住"：量程每变一次，整条曲线连刻度都会跟着缩放一次，
/// 操作员就没法对同一段曲线做跨时间比较，看起来也像数据在乱跳。所以策略是：
///
/// - 数据真的超出当前量程时立刻扩量程，绝不裁掉数据（ADR 0006 的硬要求）；
/// - 扩量程时按比例预留余量，随后的小幅波动不会再触发一次量程变化；
/// - 收缩要同时满足"新量程明显更小"和"已经稳定足够久"，避免量程来回跳。
class _RangeTracker {
  /// 扩量程时预留的余量比例。
  static const double _headroom = 0.1;

  /// 新量程小于当前量程这个比例时，才算"明显更小"，可以考虑收缩。
  static const double _shrinkRatio = 0.4;

  /// 收缩前需要保持稳定的时长。
  static const double _shrinkDelayMs = 15000;

  _AxisRange? _range;
  double _steadySinceMs = 0;

  _AxisRange update(double minValue, double maxValue, double nowMs) {
    final fit = _fitRange(minValue, maxValue, headroom: _headroom);
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
        (fit.max - fit.min) < (current.max - current.min) * _shrinkRatio;
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
  _WaveformView(ConnectionController controller, WaveformInstanceKind kind)
    : _data = controller.waveformPoints(kind),
      length = controller.waveformPointCount(kind);

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

/// 页面标题由工作区顶栏提供，这里只放实例选择器和数据状态。
class _PanelHeader extends StatelessWidget {
  const _PanelHeader({
    required this.controller,
    required this.kind,
    required this.instances,
    required this.selected,
  });

  final ConnectionController controller;
  final WaveformInstanceKind kind;
  final List<WaveformInstance> instances;
  final int selected;

  @override
  Widget build(BuildContext context) {
    final prefix = kind == WaveformInstanceKind.mag ? '磁力计' : 'IMU';
    return SizedBox(
      height: _kPanelHeaderHeight,
      child: Row(
        children: [
          // 只有一个实例时没有可选项，不摆一个点了也没反应的分段控件。
          // 左边补上卡片的内边距，让选择器最左端和波形图最左端落在同一条竖线上。
          if (instances.length > 1)
            Padding(
              padding: const EdgeInsets.only(left: _kCardPadding),
              child: _InstanceSelector(
                labels: [
                  for (final instance in instances) instance.label(prefix),
                ],
                selected: instances.indexWhere(
                  (instance) => instance.index == selected,
                ),
                onChanged: (position) => controller.setWaveformInstance(
                  kind,
                  instances[position].index,
                ),
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
      ),
    );
  }
}

/// 实例选择器。用 macos_ui 的分段控件，和校准页顶部标签保持同一种外观。
class _InstanceSelector extends StatefulWidget {
  const _InstanceSelector({
    required this.labels,
    required this.selected,
    required this.onChanged,
  });

  final List<String> labels;

  /// 当前选中项在 [labels] 里的下标。
  final int selected;
  final ValueChanged<int> onChanged;

  @override
  State<_InstanceSelector> createState() => _InstanceSelectorState();
}

class _InstanceSelectorState extends State<_InstanceSelector> {
  late MacosTabController _tabs = _createController();

  MacosTabController _createController() {
    final controller = MacosTabController(
      initialIndex: widget.selected < 0 ? 0 : widget.selected,
      length: widget.labels.length,
    );
    controller.addListener(_handleChanged);
    return controller;
  }

  void _handleChanged() => widget.onChanged(_tabs.index);

  @override
  void didUpdateWidget(covariant _InstanceSelector oldWidget) {
    super.didUpdateWidget(oldWidget);
    // MacosTabController.length 是 final，实例数量变了只能换一个控制器。
    if (oldWidget.labels.length != widget.labels.length) {
      _tabs.removeListener(_handleChanged);
      _tabs.dispose();
      _tabs = _createController();
      return;
    }
    if (widget.selected >= 0 && _tabs.index != widget.selected) {
      _tabs.index = widget.selected;
    }
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleChanged);
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MacosSegmentedControl(
    controller: _tabs,
    tabs: [for (final label in widget.labels) MacosTab(label: label)],
  );
}

class _SignalCard extends StatelessWidget {
  const _SignalCard({
    required this.group,
    required this.values,
    required this.chart,
    required this.sensorName,
    required this.sensorMounting,
  });

  final _SignalGroup group;

  /// 各轴最新一帧的瞬时值，曲线用的也是这些原始点。
  final List<double> values;
  final LineChartData? chart;

  /// 当前实例的传感器型号，未知时是空串。
  final String sensorName;
  final String sensorMounting;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = MgColors.of(context);
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(_kCardPadding, 12, _kCardPadding, 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text(group.title, style: theme.typography.title3),
              if (sensorName.isNotEmpty) ...[
                const SizedBox(width: 8),
                _SensorBadge(name: sensorName),
              ],
              if (sensorMounting.isNotEmpty) ...[
                const SizedBox(width: 8),
                _SensorBadge(name: sensorMounting),
              ],
              const Spacer(),
              Label(
                text: Text(group.unit, style: TextStyle(color: colors.accent)),
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
                color: colors.chartSurface,
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

/// 传感器型号胶囊，放在卡片标题右侧，只做标识不做交互。
class _SensorBadge extends StatelessWidget {
  const _SensorBadge({required this.name});

  final String name;

  @override
  Widget build(BuildContext context) {
    final typography = MacosTheme.of(context).typography;
    final colors = MgColors.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
      decoration: BoxDecoration(
        color: colors.accentSurface,
        borderRadius: BorderRadius.circular(6),
      ),
      child: Text(
        name,
        style: typography.caption1.copyWith(
          color: colors.accent,
          fontWeight: FontWeight.w600,
        ),
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

  /// 最新一帧的瞬时值。
  final double value;
  final Color color;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: colors.valueTile,
        border: Border(left: BorderSide(color: color, width: 4)),
        borderRadius: BorderRadius.circular(9),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text(label, style: const TextStyle(fontWeight: FontWeight.w700)),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  _format(value),
                  textAlign: TextAlign.right,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  // 等宽数字：位数变化时读数不会左右抖动。
                  style: const TextStyle(
                    fontWeight: FontWeight.w700,
                    fontFeatures: [FontFeature.tabularFigures()],
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  static String _format(double number) =>
      number.isFinite ? number.toStringAsFixed(2) : '—';
}
