import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../connection_panel.dart';

enum MapStyle { standard, terrain, amap, amapSatellite, amapSatelliteHybrid }

class _MapSource {
  const _MapSource({
    required this.label,
    required this.url,
    required this.attribution,
    this.subdomains = const [],
    this.maxNativeZoom = 18,
    this.fallbackUrl,
    this.overlay,
  });

  final String label;
  final String url;
  final String attribution;
  final List<String> subdomains;
  final int maxNativeZoom;
  final String? fallbackUrl;
  final _MapSource? overlay;
}

_MapSource _sourceFor(MapStyle style) => switch (style) {
  MapStyle.standard => const _MapSource(
    label: 'OpenStreetMap 标准地图',
    url: 'https://b.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    fallbackUrl: 'https://a.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    maxNativeZoom: 19,
    attribution: '© OpenStreetMap contributors | HOT',
  ),
  MapStyle.terrain => const _MapSource(
    label: 'OpenHikingMap 地形图',
    url: 'https://tile.openmaps.fr/openhikingmap/{z}/{x}/{y}.png',
    fallbackUrl: 'https://b.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    maxNativeZoom: 17,
    attribution: '© OpenStreetMap contributors | OpenHikingMap',
  ),
  MapStyle.amap => const _MapSource(
    label: '高德标准地图',
    url:
        'https://webrd0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=7',
    subdomains: ['1', '2', '3', '4'],
    attribution: '© 高德地图 | 高德开放平台',
  ),
  MapStyle.amapSatellite => const _MapSource(
    label: '高德卫星地图',
    url:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: ['1', '2', '3', '4'],
    attribution:
        '© 高德地图 | Image© DigitalGlobe & China Siwei | Airbus & EastDawn',
  ),
  MapStyle.amapSatelliteHybrid => _MapSource(
    label: '高德卫星 + 路网',
    url:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: const ['1', '2', '3', '4'],
    attribution:
        '© 高德地图 | Image© DigitalGlobe & China Siwei | Airbus & EastDawn',
    overlay: const _MapSource(
      label: '高德卫星路网标注',
      url:
          'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=8',
      subdomains: ['1', '2', '3', '4'],
      attribution: '',
    ),
  ),
};

const _defaultCenter = LatLng(31.2304, 121.4737); // Shanghai, GCJ-02.

class AMapMapView extends StatefulWidget {
  const AMapMapView({
    super.key,
    required this.connectionController,
    this.onOpenConfiguration,
  });

  final ConnectionController connectionController;
  final VoidCallback? onOpenConfiguration;

  @override
  State<AMapMapView> createState() => _AMapMapViewState();
}

class _AMapMapViewState extends State<AMapMapView> {
  final _controller = MapController();
  MapStyle _style = MapStyle.amap;
  bool _showConnectionPanel = false;
  bool _showMissionPlanning = false;

  @override
  Widget build(BuildContext context) {
    final source = _sourceFor(_style);
    return Stack(
      fit: StackFit.expand,
      children: [
        FlutterMap(
          mapController: _controller,
          options: const MapOptions(
            initialCenter: _defaultCenter,
            initialZoom: 11,
            minZoom: 3,
            maxZoom: 19,
          ),
          children: [
            TileLayer(
              urlTemplate: source.url,
              fallbackUrl: source.fallbackUrl,
              subdomains: source.subdomains,
              maxNativeZoom: source.maxNativeZoom,
              userAgentPackageName: 'com.mgcs.frontend',
            ),
            if (source.overlay != null)
              TileLayer(
                urlTemplate: source.overlay!.url,
                subdomains: source.overlay!.subdomains,
                maxNativeZoom: source.overlay!.maxNativeZoom,
                userAgentPackageName: 'com.mgcs.frontend',
              ),
          ],
        ),
        Positioned(
          right: 18,
          bottom: 18,
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              MacosPulldownButton(
                title: source.label,
                menuAlignment: PulldownMenuAlignment.right,
                items: [
                  for (final style in MapStyle.values)
                    MacosPulldownMenuItem(
                      label: _sourceFor(style).label,
                      title: Text(_sourceFor(style).label),
                      onTap: () => setState(() => _style = style),
                    ),
                ],
              ),
              const SizedBox(width: 10),
              HelpButton(
                semanticLabel: '地图数据来源',
                onPressed: () => _showAttribution(context, source),
              ),
            ],
          ),
        ),
        Positioned(
          left: 18,
          bottom: 18,
          child: _MapControls(
            onZoomIn: () => _controller.move(
              _controller.camera.center,
              _controller.camera.zoom + 1,
            ),
            onZoomOut: () => _controller.move(
              _controller.camera.center,
              _controller.camera.zoom - 1,
            ),
            onReset: () => _controller.move(_defaultCenter, 11),
            onOpenConfiguration: widget.onOpenConfiguration,
            onOpenConnection: () => setState(() {
              _showMissionPlanning = false;
              _showConnectionPanel = !_showConnectionPanel;
            }),
            onOpenMissionPlanning: () => setState(() {
              _showConnectionPanel = false;
              _showMissionPlanning = !_showMissionPlanning;
            }),
          ),
        ),
        if (_showConnectionPanel)
          Positioned(
            left: 70,
            right: 18,
            bottom: 18,
            child: Align(
              alignment: Alignment.bottomLeft,
              child: ConnectionFloatingPanel(
                controller: widget.connectionController,
                onClose: () => setState(() => _showConnectionPanel = false),
              ),
            ),
          ),
        if (_showMissionPlanning)
          Positioned.fill(
            child: Align(
              alignment: Alignment.center,
              child: MissionPlanningFloatingPanel(
                onClose: () => setState(() => _showMissionPlanning = false),
              ),
            ),
          ),
      ],
    );
  }

  void _showAttribution(BuildContext context, _MapSource source) {
    showMacosAlertDialog<void>(
      context: context,
      builder: (dialogContext) => MacosAlertDialog(
        appIcon: const Icon(
          Icons.copyright_outlined,
          size: 64,
          color: MacosColors.systemBlueColor,
        ),
        title: Text(source.label),
        message: Text(
          source.attribution.isEmpty ? '本图层无署名要求。' : source.attribution,
        ),
        primaryButton: PushButton(
          controlSize: ControlSize.large,
          onPressed: () => Navigator.of(dialogContext).pop(),
          child: const Text('好'),
        ),
      ),
    );
  }
}

class _MapControls extends StatelessWidget {
  const _MapControls({
    required this.onZoomIn,
    required this.onZoomOut,
    required this.onReset,
    this.onOpenConfiguration,
    required this.onOpenConnection,
    required this.onOpenMissionPlanning,
  });

  final VoidCallback onZoomIn;
  final VoidCallback onZoomOut;
  final VoidCallback onReset;
  final VoidCallback? onOpenConfiguration;
  final VoidCallback onOpenConnection;
  final VoidCallback onOpenMissionPlanning;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: MacosTheme.brightnessOf(
        context,
      ).resolve(const Color(0xF5FFFFFF), const Color(0xF52A2B2E)),
      borderRadius: BorderRadius.circular(9),
      border: Border.all(color: MacosTheme.of(context).dividerColor),
      boxShadow: const [
        BoxShadow(
          color: Color(0x22000000),
          blurRadius: 10,
          offset: Offset(0, 3),
        ),
      ],
    ),
    child: Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        _MapIconButton(icon: Icons.add, tooltip: '放大', onPressed: onZoomIn),
        _MapIconButton(icon: Icons.remove, tooltip: '缩小', onPressed: onZoomOut),
        _MapIconButton(
          icon: Icons.my_location,
          tooltip: '回到默认位置',
          onPressed: onReset,
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 9),
          child: Container(
            height: 1,
            color: MacosTheme.of(context).dividerColor,
          ),
        ),
        _MapIconButton(
          icon: CupertinoIcons.slider_horizontal_3,
          tooltip: '参数配置',
          onPressed: onOpenConfiguration ?? () {},
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 9),
          child: Container(
            height: 1,
            color: MacosTheme.of(context).dividerColor,
          ),
        ),
        _MapIconButton(
          icon: CupertinoIcons.link,
          tooltip: '连接飞控',
          onPressed: onOpenConnection,
        ),
        _MapIconButton(
          icon: CupertinoIcons.map,
          tooltip: '航线规划',
          onPressed: onOpenMissionPlanning,
        ),
      ],
    ),
  );
}

class _MapIconButton extends StatelessWidget {
  const _MapIconButton({
    required this.icon,
    required this.tooltip,
    required this.onPressed,
  });

  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final brightness = MacosTheme.brightnessOf(context);
    return MacosTooltip(
      message: tooltip,
      child: MacosIconButton(
        icon: Icon(
          icon,
          size: 18,
          color: brightness.resolve(
            const Color(0xFF3B3B3B),
            const Color(0xFFE5E5E5),
          ),
        ),
        semanticLabel: tooltip,
        boxConstraints: const BoxConstraints.tightFor(width: 38, height: 38),
        padding: EdgeInsets.zero,
        backgroundColor: Colors.transparent,
        hoverColor: brightness.resolve(
          const Color(0x11000000),
          const Color(0x22FFFFFF),
        ),
        onPressed: onPressed,
      ),
    );
  }
}
