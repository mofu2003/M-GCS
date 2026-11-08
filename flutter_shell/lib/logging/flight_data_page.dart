import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'file_browser_page.dart';
import 'flight_log_page.dart';
import 'onboard_log_page.dart';

enum FlightDataView {
  onboardLogs('机载日志'),
  onboardFiles('机载文件'),
  flightRecords('飞行记录');

  const FlightDataView(this.title);
  final String title;
}

/// 从飞行控制页面打开的数据工具，各页面复用已有的会话控制器。
class FlightDataPage extends StatefulWidget {
  const FlightDataPage({
    super.key,
    required this.view,
    required this.controller,
    required this.onReturnToFlight,
  });

  final FlightDataView view;
  final ConnectionController controller;
  final VoidCallback onReturnToFlight;

  @override
  State<FlightDataPage> createState() => _FlightDataPageState();
}

class _FlightDataPageState extends State<FlightDataPage> {
  late final MacosTabController _tabs = MacosTabController(
    initialIndex: widget.view.index,
    length: FlightDataView.values.length,
  )..addListener(_refreshTab);

  void _refreshTab() => setState(() {});

  @override
  void dispose() {
    _tabs.removeListener(_refreshTab);
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MacosWindow(
      child: ColoredBox(
        color: colors.workspace,
        child: Column(
          children: [
            Container(
              height: MgMetrics.toolbarHeight,
              padding: const EdgeInsets.symmetric(horizontal: 20),
              decoration: BoxDecoration(
                color: colors.toolbar,
                border: Border(bottom: BorderSide(color: colors.hairline)),
              ),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      '日志与文件',
                      style: MgTypography.sans(MgTypography.title),
                    ),
                  ),
                  PushButton(
                    controlSize: ControlSize.regular,
                    secondary: true,
                    onPressed: widget.onReturnToFlight,
                    child: const Text('返回飞行控制页面'),
                  ),
                ],
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 12),
              child: MacosSegmentedControl(
                controller: _tabs,
                tabs: [
                  for (final view in FlightDataView.values)
                    MacosTab(label: view.title),
                ],
              ),
            ),
            Expanded(
              child: switch (FlightDataView.values[_tabs.index]) {
                FlightDataView.onboardLogs => OnboardLogPage(
                  controller: widget.controller.logController,
                ),
                FlightDataView.onboardFiles => FileBrowserPage(
                  controller: widget.controller.fileController,
                ),
                FlightDataView.flightRecords => FlightLogPage(
                  controller: widget.controller.tlogController,
                ),
              },
            ),
          ],
        ),
      ),
    );
  }
}
