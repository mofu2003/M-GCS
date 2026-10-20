import 'dart:async';

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

/// 数值编辑框，提交和失焦写入去重，失败后恢复已确认的读数。
class ParameterValueWell extends StatefulWidget {
  const ParameterValueWell({
    super.key,
    required this.text,
    required this.enabled,
    required this.center,
    this.fillColor,
    this.onSubmitted,
  });

  final String text;
  final bool enabled;
  final bool center;
  final Color? fillColor;
  final Future<bool> Function(double value)? onSubmitted;

  @override
  State<ParameterValueWell> createState() => _ValueWellState();
}

class _ValueWellState extends State<ParameterValueWell> {
  late final TextEditingController _textController = TextEditingController(
    text: widget.text,
  );
  late final FocusNode _focusNode = FocusNode();
  late String _lastCommittedText = widget.text;

  @override
  void initState() {
    super.initState();
    _focusNode.addListener(_handleFocusChange);
  }

  void _handleFocusChange() {
    if (!_focusNode.hasFocus) unawaited(_commitCurrentValue());
  }

  Future<bool> _commitCurrentValue() async {
    final text = _textController.text.trim();
    if (text == _lastCommittedText) return true;
    final value = double.tryParse(text);
    final submit = widget.onSubmitted;
    if (value == null || !value.isFinite || submit == null) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
      return false;
    }

    _lastCommittedText = text;
    final success = await submit(value);
    if (!success && mounted) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
    }
    return success;
  }

  @override
  void didUpdateWidget(ParameterValueWell oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!_focusNode.hasFocus && oldWidget.text != widget.text) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
    }
  }

  @override
  void dispose() {
    _focusNode.removeListener(_handleFocusChange);
    _textController.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final textColor = colors.textPrimary;
    final textStyle = MgTypography.sans(
      // 数值不加粗：这一格在表格里成片出现，加粗会把整张表压得发闷。
      MgTypography.body.copyWith(color: textColor),
    );
    final decoration = BoxDecoration(
      color: widget.fillColor ?? colors.well,
      borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      border: Border.all(color: colors.divider),
    );
    // 不可编辑的数值格用纯文本渲染：表格里成片出现时，
    // 禁用 MacosTextField 的构建成本远高于普通 Text。
    if (!widget.enabled) {
      return Container(
        alignment: widget.center ? Alignment.center : Alignment.centerLeft,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
        decoration: decoration,
        child: Text(
          widget.text,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: textStyle,
        ),
      );
    }
    return MacosTextField(
      controller: _textController,
      focusNode: _focusNode,
      enabled: true,
      placeholder: widget.text,
      placeholderStyle: textStyle,
      style: textStyle,
      textAlign: widget.center ? TextAlign.center : TextAlign.start,
      keyboardType: const TextInputType.numberWithOptions(
        decimal: true,
        signed: true,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: decoration,
      onSubmitted: (value) {
        unawaited(
          _commitCurrentValue().whenComplete(() => _focusNode.unfocus()),
        );
      },
    );
  }
}
