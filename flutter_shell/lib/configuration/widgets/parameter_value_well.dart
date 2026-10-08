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
    final textColor = context.mgColors.textPrimary;
    return MacosTextField(
      controller: _textController,
      focusNode: _focusNode,
      enabled: widget.enabled,
      readOnly: !widget.enabled,
      placeholder: widget.text,
      placeholderStyle: MgTypography.sans(
        MgTypography.body.copyWith(color: textColor),
      ),
      style: MgTypography.sans(
        // 数值不加粗：这一格在表格里成片出现，加粗会把整张表压得发闷。
        MgTypography.body.copyWith(color: textColor),
      ),
      textAlign: widget.center ? TextAlign.center : TextAlign.start,
      keyboardType: const TextInputType.numberWithOptions(
        decimal: true,
        signed: true,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: widget.fillColor ?? context.mgColors.well,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: context.mgColors.divider),
      ),
      onSubmitted: (value) {
        unawaited(
          _commitCurrentValue().whenComplete(() => _focusNode.unfocus()),
        );
      },
    );
  }
}
