import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart' show Divider, Form, FormField;
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/adaptive_popup_button.dart';
import '../widgets/mg_floating_panel.dart';
import 'mission_document_models.dart';
import 'quick_mission_draft.dart';
import 'quick_mission_geometry.dart';

class QuickMissionPanel extends StatefulWidget {
  const QuickMissionPanel({
    super.key,
    required this.draft,
    required this.onGenerate,
    required this.onCancel,
  });
  final QuickMissionDraft draft;
  final Future<void> Function() onGenerate;
  final VoidCallback onCancel;

  @override
  State<QuickMissionPanel> createState() => _QuickMissionPanelState();
}

class _QuickMissionPanelState extends State<QuickMissionPanel> {
  final _form = GlobalKey<FormState>();
  final _inputs = <String, TextEditingController>{};
  final _radius = TextEditingController();
  final _radiusFocus = FocusNode();
  final _bearing = TextEditingController();
  final _bearingFocus = FocusNode();
  bool _generating = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _syncCircleControls();
    widget.draft.addListener(_syncCircleControls);
    _radiusFocus.addListener(_syncCircleControls);
    _bearingFocus.addListener(_syncCircleControls);
  }

  void _syncCircleControls() {
    if (!_bearingFocus.hasFocus) {
      _bearing.text = widget.draft.startBearing.toStringAsFixed(1);
    }
    if (!_radiusFocus.hasFocus) {
      _radius.text = widget.draft.radiusMeters.toStringAsFixed(1);
    }
  }

  @override
  void dispose() {
    widget.draft.removeListener(_syncCircleControls);
    _radiusFocus.removeListener(_syncCircleControls);
    _radiusFocus.dispose();
    _radius.dispose();
    _bearingFocus.removeListener(_syncCircleControls);
    _bearingFocus.dispose();
    _bearing.dispose();
    for (final input in _inputs.values) {
      input.dispose();
    }
    super.dispose();
  }

  Widget _row(
    String label,
    Widget control, {
    bool compact = false,
    bool stacked = false,
  }) {
    if (stacked) {
      return Padding(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 1),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(label, style: MgTypography.sans(MgTypography.caption)),
            const SizedBox(height: 2),
            ConstrainedBox(
              constraints: const BoxConstraints(minHeight: 28),
              child: control,
            ),
          ],
        ),
      );
    }
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 5),
      child: ConstrainedBox(
        constraints: const BoxConstraints(
          minHeight: MgMetrics.controlHeight + 2,
        ),
        child: Row(
          children: [
            SizedBox(width: compact ? 56 : 100, child: Text(label)),
            const SizedBox(width: 8),
            Expanded(child: control),
          ],
        ),
      ),
    );
  }

  Widget _pair(Widget first, Widget second) => Row(
    children: [
      Expanded(child: first),
      Expanded(child: second),
    ],
  );

  Widget _group(String title, List<Widget> rows) => Column(
    mainAxisSize: MainAxisSize.min,
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      Padding(
        padding: const EdgeInsets.only(left: 2, bottom: 6),
        child: Text(
          title,
          style: MgTypography.sans(
            MgTypography.caption.copyWith(
              color: context.mgColors.textSecondary,
            ),
          ),
        ),
      ),
      _settings(rows),
    ],
  );

  Widget _settings(List<Widget> rows) {
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surfaceMuted,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: colors.hairline),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          for (var index = 0; index < rows.length; index++) ...[
            if (index > 0)
              Divider(
                height: 1,
                indent: 10,
                endIndent: 10,
                color: colors.hairline,
              ),
            rows[index],
          ],
        ],
      ),
    );
  }

  Widget number(
    String label,
    double value,
    void Function(double) assign, {
    String? unit,
    bool compact = false,
    bool stacked = false,
    bool positive = false,
    bool integer = false,
    TextEditingController? controller,
    FocusNode? focusNode,
  }) {
    final input =
        controller ??
        _inputs.putIfAbsent(
          label,
          () => TextEditingController(
            text: integer ? value.toInt().toString() : value.toString(),
          ),
        );
    return _row(
      label,
      FormField<String>(
        validator: (_) {
          final parsed = double.tryParse(input.text.trim());
          if (parsed == null ||
              !parsed.isFinite ||
              (positive && parsed <= 0) ||
              (integer && parsed != parsed.roundToDouble())) {
            return '请输入${positive ? '大于 0 的' : '有效'}${integer ? '整数' : '数字'}';
          }
          return null;
        },
        builder: (field) => Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Semantics(
              label: label,
              child: MacosTextField(
                controller: input,
                focusNode: focusNode,
                enabled: !_generating,
                textAlign: TextAlign.end,
                style: MgTypography.sans(MgTypography.data),
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
                suffix: Padding(
                  padding: const EdgeInsets.only(right: 6),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      if (unit != null)
                        Text(
                          unit,
                          style: MgTypography.sans(
                            MgTypography.caption.copyWith(
                              color: context.mgColors.textSecondary,
                            ),
                          ),
                        ),
                      if (stacked && field.hasError)
                        MacosTooltip(
                          message: field.errorText!,
                          child: Icon(
                            CupertinoIcons.exclamationmark_circle,
                            size: 14,
                            color: context.mgColors.danger,
                            semanticLabel: field.errorText,
                          ),
                        ),
                    ],
                  ),
                ),
                keyboardType: const TextInputType.numberWithOptions(
                  decimal: true,
                  signed: true,
                ),
                onChanged: (text) {
                  field.didChange(text);
                  final parsed = double.tryParse(text.trim());
                  if (parsed == null ||
                      !parsed.isFinite ||
                      (positive && parsed <= 0) ||
                      (integer && parsed != parsed.roundToDouble())) {
                    return;
                  }
                  assign(parsed);
                  widget.draft.update();
                },
              ),
            ),
            if (field.hasError && !stacked)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  field.errorText!,
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(
                      color: context.mgColors.danger,
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
      compact: compact,
      stacked: stacked,
    );
  }

  Future<void> _generate() async {
    if (_generating || !_form.currentState!.validate()) return;
    setState(() {
      _generating = true;
      _error = null;
    });
    try {
      await widget.onGenerate();
    } on Object catch (error) {
      if (mounted) setState(() => _error = error.toString());
    } finally {
      if (mounted) setState(() => _generating = false);
    }
  }

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) => AnimatedBuilder(
      animation: widget.draft,
      builder: (context, _) {
        final draft = widget.draft;
        final colors = context.mgColors;
        final dense = constraints.maxHeight < 300;
        final roomy = constraints.maxHeight >= 440;
        int? count;
        String? validation;
        try {
          count = draft.generate().length;
        } on ArgumentError catch (error) {
          validation = error.message.toString();
        }
        final frames = [
          if (!kMissionFrameChoices.any((choice) => choice.$1 == draft.frame))
            draft.frame,
          for (final choice in kMissionFrameChoices) choice.$1,
        ];
        final altitude = number(
          '统一高度',
          draft.altitudeMeters,
          (value) => draft.altitudeMeters = value,
          unit: '米',
          compact: !roomy,
          stacked: dense,
        );
        final frame = _row(
          '高度基准',
          AdaptivePopupButton<int>(
            value: draft.frame,
            options: frames,
            labelOf: (value) =>
                kMissionFrameChoices
                    .where((choice) => choice.$1 == value)
                    .map((choice) => choice.$2)
                    .firstOrNull ??
                '任务默认基准（$value）',
            onChanged: _generating
                ? null
                : (value) {
                    draft.frame = value;
                    draft.update();
                  },
          ),
          stacked: dense,
        );
        final closed = _row(
          '闭合航线',
          Align(
            alignment: dense ? Alignment.centerLeft : Alignment.centerRight,
            child: MacosTooltip(
              message: '闭合时追加首点作为末点',
              child: MacosSwitch(
                value: draft.closed,
                onChanged: _generating
                    ? null
                    : (value) {
                        draft.closed = value;
                        draft.update();
                      },
              ),
            ),
          ),
          compact: !roomy,
          stacked: dense,
        );
        final rows = <Widget>[];
        if (draft.shape == QuickMissionShape.circle) {
          final radius = number(
            '半径',
            draft.radiusMeters,
            (value) => draft.radiusMeters = value,
            unit: '米',
            positive: true,
            compact: true,
            stacked: dense,
            controller: _radius,
            focusNode: _radiusFocus,
          );
          final points = number(
            '圆周点数',
            draft.circlePoints.toDouble(),
            (value) => draft.circlePoints = value.toInt(),
            unit: '个',
            positive: true,
            integer: true,
            compact: true,
            stacked: dense,
          );
          final angle = number(
            '首点角度',
            draft.startBearing,
            (value) => draft.startBearing = value,
            unit: '°',
            compact: true,
            stacked: dense,
            controller: _bearing,
            focusNode: _bearingFocus,
          );
          final direction = _row(
            '方向',
            AdaptivePopupButton<bool>(
              value: draft.clockwise,
              options: const [true, false],
              labelOf: (value) => value ? '顺时针' : '逆时针',
              onChanged: _generating
                  ? null
                  : (value) {
                      draft.clockwise = value;
                      draft.update();
                    },
            ),
            compact: true,
            stacked: dense,
          );
          rows.addAll(
            dense
                ? [
                    Row(
                      children: [
                        Expanded(child: radius),
                        Expanded(child: points),
                        Expanded(child: altitude),
                        Expanded(child: closed),
                      ],
                    ),
                    Row(
                      children: [
                        Expanded(child: angle),
                        Expanded(child: direction),
                        Expanded(flex: 2, child: frame),
                      ],
                    ),
                  ]
                : [
                    _pair(radius, points),
                    _pair(angle, direction),
                    if (roomy) closed else _pair(altitude, closed),
                    if (!roomy) frame,
                  ],
          );
        } else {
          final spacing = number(
            '最大间距',
            draft.spacingMeters,
            (value) => draft.spacingMeters = value,
            unit: '米',
            positive: true,
            compact: true,
            stacked: dense,
          );
          rows.addAll(
            dense
                ? [_pair(spacing, altitude), _pair(closed, frame)]
                : [
                    if (roomy) spacing else _pair(spacing, altitude),
                    closed,
                    if (!roomy) frame,
                  ],
          );
        }
        final notice = _error ?? validation ?? draft.instruction;
        return MgFloatingPanel(
          child: DefaultTextStyle(
            style: MgTypography.sans(
              MgTypography.body.copyWith(color: colors.textPrimary),
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Padding(
                  padding: EdgeInsets.fromLTRB(
                    14,
                    dense ? 2 : 12,
                    8,
                    dense ? 2 : 12,
                  ),
                  child: Row(
                    children: [
                      Expanded(
                        child: Text(
                          '快速绘制',
                          style: MacosTheme.of(context).typography.title3,
                        ),
                      ),
                      MacosTooltip(
                        message: draft.instruction,
                        child: Text(
                          draft.shape.label,
                          style: MgTypography.sans(
                            MgTypography.caption.copyWith(
                              color: colors.textSecondary,
                            ),
                          ),
                        ),
                      ),
                      const SizedBox(width: 8),
                      MacosTooltip(
                        message: '取消快速绘制',
                        child: MacosIconButton(
                          semanticLabel: '取消快速绘制',
                          boxConstraints: const BoxConstraints.tightFor(
                            width: 28,
                            height: 28,
                          ),
                          padding: EdgeInsets.zero,
                          onPressed: _generating ? null : widget.onCancel,
                          icon: const MacosIcon(CupertinoIcons.xmark, size: 14),
                        ),
                      ),
                    ],
                  ),
                ),
                Divider(height: 1, color: colors.hairline),
                Padding(
                  padding: EdgeInsets.symmetric(
                    horizontal: dense ? 8 : 14,
                    vertical: dense ? 2 : 12,
                  ),
                  child: Form(
                    key: _form,
                    child: roomy
                        ? Column(
                            mainAxisSize: MainAxisSize.min,
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              _group('形状参数', rows),
                              const SizedBox(height: 14),
                              _group('飞行高度', [altitude, frame]),
                            ],
                          )
                        : _settings(rows),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 14),
                  child: MacosTooltip(
                    message: notice,
                    child: Text(
                      notice,
                      maxLines: dense ? 1 : 2,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: _error == null
                              ? colors.textSecondary
                              : colors.danger,
                        ),
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 4),
                Divider(height: 1, color: colors.hairline),
                Padding(
                  padding: EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: dense ? 4 : 12,
                  ),
                  child: Row(
                    children: [
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            MacosTooltip(
                              message: '已选 ${draft.controls.length} 个控制点',
                              child: Text(
                                count == null ? '等待绘制' : '将生成 $count 个航点',
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: MgTypography.sans(MgTypography.caption),
                              ),
                            ),
                            if (!dense)
                              Text(
                                '${draft.controls.length} 个控制点',
                                style: MgTypography.sans(
                                  MgTypography.caption.copyWith(
                                    color: colors.textSecondary,
                                  ),
                                ),
                              ),
                          ],
                        ),
                      ),
                      MacosTooltip(
                        message: '撤回最后一个控制点',
                        child: MacosIconButton(
                          semanticLabel: '撤回最后一个控制点',
                          icon: const MacosIcon(
                            CupertinoIcons.arrow_uturn_left,
                            size: 15,
                          ),
                          onPressed: _generating || draft.controls.isEmpty
                              ? null
                              : draft.removeLastPoint,
                        ),
                      ),
                      const SizedBox(width: 8),
                      PushButton(
                        controlSize: ControlSize.regular,
                        secondary: true,
                        onPressed: _generating ? null : widget.onCancel,
                        child: const Text('取消'),
                      ),
                      const SizedBox(width: 8),
                      PushButton(
                        controlSize: ControlSize.regular,
                        onPressed: count == null || _generating
                            ? null
                            : _generate,
                        child: Text(_generating ? '正在生成…' : '生成航点'),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        );
      },
    ),
  );
}
