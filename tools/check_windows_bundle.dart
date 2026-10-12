import 'dart:ffi';
import 'dart:io';

import 'ffi_bundle_symbols.dart';

const _requiredFiles = [
  'mgcs_frontend.exe',
  'mgcore.dll',
  'flutter_windows.dll',
  'USBBoardInfo.json',
  'SensorDeviceInfo.json',
  'ParameterDescriptors.json',
  'ParameterTranslations.zh-CN.json',
];

void main(List<String> args) {
  if (!Platform.isWindows) {
    stderr.writeln('[windows-bundle] 只在 Windows 上执行。');
    exitCode = 2;
    return;
  }
  if (args.length != 1) {
    stderr.writeln(
      '[windows-bundle] 用法：dart tools/check_windows_bundle.dart <bundle-dir>',
    );
    exitCode = 2;
    return;
  }

  final bundle = Directory(args.single).absolute;
  if (!bundle.existsSync()) {
    stderr.writeln('[windows-bundle] 目录不存在：${bundle.path}');
    exitCode = 1;
    return;
  }

  var failed = false;
  for (final name in _requiredFiles) {
    final file = File('${bundle.path}${Platform.pathSeparator}$name');
    if (!file.existsSync()) {
      stderr.writeln('[windows-bundle] 缺少文件：$name');
      failed = true;
    }
  }

  if (!failed) {
    final library = DynamicLibrary.open(
      '${bundle.path}${Platform.pathSeparator}mgcore.dll',
    );
    for (final symbol in requiredFfiSymbols) {
      try {
        library.lookup<NativeFunction<Void Function()>>(symbol);
      } on ArgumentError {
        stderr.writeln('[windows-bundle] mgcore.dll 缺少导出：$symbol');
        failed = true;
      }
    }
  }

  if (failed) {
    exitCode = 1;
    return;
  }
  stdout.writeln(
    '[windows-bundle] 通过：${_requiredFiles.length} 个文件和 '
    '${requiredFfiSymbols.length} 个 FFI 导出可用。',
  );
}
