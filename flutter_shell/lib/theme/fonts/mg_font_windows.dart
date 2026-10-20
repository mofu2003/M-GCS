import 'package:flutter/material.dart';

/// Windows 平台字体唯一调参入口。
///
/// Windows 的字体族、回退链以及后续的字号/行高微调都只改这个文件，
/// 不要修改共享字号 token 或 macOS 配置。
class MgFontWindows {
  const MgFontWindows._();

  static const String family = 'MiSans';
  static const List<String> fallback = ['Microsoft YaHei UI'];

  static TextStyle apply(TextStyle style) =>
      style.copyWith(fontFamily: family, fontFamilyFallback: fallback);
}
