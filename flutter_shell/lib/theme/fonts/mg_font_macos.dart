import 'package:flutter/material.dart';

/// macOS 平台字体唯一调参入口。
///
/// macOS 的字体族、回退链以及后续的字号/行高微调都只改这个文件，
/// 不要修改共享字号 token 或 Windows 配置。
class MgFontMacos {
  const MgFontMacos._();

  static const String family = 'MiSans';
  static const List<String> fallback = ['PingFang SC'];

  static TextStyle apply(TextStyle style) =>
      style.copyWith(fontFamily: family, fontFamilyFallback: fallback);
}
