import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';

import 'application/connection_controller.dart';
import 'application/connection_session_backend.dart';
import 'application/mgcore/mgcore_connection_client.dart';
import 'application/mgcore/mgcore_connection_session_backend.dart';
import 'application/mgcore/mgcore_ffi_api.dart';
import 'application/unavailable_connection_session_backend.dart';
import 'features/shell/mgcs_app.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (!kIsWeb && (Platform.isMacOS || Platform.isWindows)) {
    await windowManager.ensureInitialized();
    const windowOptions = WindowOptions(
      size: Size(1280, 860),
      minimumSize: Size(1040, 720),
      title: 'M-GCS',
    );
    await windowManager.waitUntilReadyToShow(windowOptions, () async {
      await windowManager.show();
      await windowManager.focus();
    });
  }
  runApp(MgcsApp(backendFactory: createConnectionSessionBackend));
}

ConnectionSessionBackend createConnectionSessionBackend() {
  try {
    return MgCoreConnectionSessionBackend(
      client: MgCoreConnectionClient(api: MgCoreFfiApi.open()),
    );
  } on Object catch (error) {
    stderr.writeln('MgCore initialization failed: $error');
    return UnavailableConnectionSessionBackend('MgCore 加载失败：$error');
  }
}

ConnectionController createConnectionController() {
  return ConnectionController(backend: createConnectionSessionBackend());
}
