import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../connection_panel.dart';
import 'gcj_coordinates.dart';
import 'map_preferences.dart';

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
    this.gcj02 = false,
  });

  final String label;
  final String url;
  final String attribution;
  final List<String> subdomains;
  final int maxNativeZoom;
  final String? fallbackUrl;
  final _MapSource? overlay;

  /// 瓦片是否为 GCJ-02 坐标系。高德是，OSM/HOT/OpenHikingMap 是 WGS-84。
  final bool gcj02;
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
    gcj02: true,
  ),
  MapStyle.amapSatellite => const _MapSource(
    label: '高德卫星地图',
    url:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: ['1', '2', '3', '4'],
    attribution:
        '© 高德地图 | Image© DigitalGlobe & China Siwei | Airbus & EastDawn',
    gcj02: true,
  ),
  MapStyle.amapSatelliteHybrid => _MapSource(
    label: '高德卫星 + 路网',
    url:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: const ['1', '2', '3', '4'],
    attribution:
        '© 高德地图 | Image© DigitalGlobe & China Siwei | Airbus & EastDawn',
    gcj02: true,
    overlay: const _MapSource(
      label: '高德卫星路网标注',
      url:
          'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=8',
      subdomains: ['1', '2', '3', '4'],
      attribution: '',
    ),
  ),
};

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
  double _zoom = kDefaultMapZoom;
  LatLng _center = kDefaultMapCenter;

  /// 偏好读完之前不写盘，否则会把默认值覆盖掉用户上次的选择。
  bool _preferencesLoaded = false;
  Timer? _saveTimer;
  bool _showConnectionPanel = false;
  bool _showMissionPlanning = false;

  @override
  void initState() {
    super.initState();
    unawaited(_restorePreferences());
  }

  Future<void> _restorePreferences() async {
    final preferences = await MapPreferences.load();
    if (!mounted) return;
    setState(() {
      _preferencesLoaded = true;
      _zoom = preferences.zoom;
      _center = preferences.center;
      _style = MapStyle.values.firstWhere(
        (value) => value.name == preferences.styleName,
        orElse: () => MapStyle.amap,
      );
    });
    // 地图可能已经用默认中心建好了，读完偏好再挪一次。
    // `_center` 存的是 WGS-84，落位时要转到当前瓦片的坐标系。
    _controller.move(_toTileFrame(_center.latitude, _center.longitude), _zoom);
  }

  /// 缩放/中心变化很密集，合并成一次写盘。
  void _scheduleSave(LatLng center, double zoom) {
    _center = _toWgs84(center);
    _zoom = zoom;
    if (!_preferencesLoaded) return;
    _saveTimer?.cancel();
    _saveTimer = Timer(const Duration(milliseconds: 600), () {
      unawaited(
        MapPreferences(
          styleName: _style.name,
          zoom: _zoom,
          center: _center,
        ).save(),
      );
    });
  }

  void _selectStyle(MapStyle style) {
    setState(() => _style = style);
    // 坐标系可能变了（高德 GCJ-02 ↔ OSM WGS-84），按同一地理位置重新定位，
    // 否则切一次地图源画面会整体平移几百米。
    _controller.move(_toTileFrame(_center.latitude, _center.longitude), _zoom);
    if (!_preferencesLoaded) return;
    unawaited(
      MapPreferences(
        styleName: style.name,
        zoom: _zoom,
        center: _center,
      ).save(),
    );
  }

  @override
  void dispose() {
    _saveTimer?.cancel();
    super.dispose();
  }

  /// Home 点与飞机当前位置。
  ///
  /// 坐标一律先转到当前瓦片所在的坐标系：飞控给的是 WGS-84，高德瓦片是
  /// GCJ-02，不转换会整体偏 100–700 米（Mission Planner 也是这么处理的）。
  LatLng _toTileFrame(double latitude, double longitude) {
    if (!_sourceFor(_style).gcj02) return LatLng(latitude, longitude);
    final (lat, lon) = GcjCoordinates.wgs84ToGcj02(latitude, longitude);
    return LatLng(lat, lon);
  }

  /// 反向：把瓦片坐标系里的点还原成 WGS-84。
  ///
  /// 相机中心始终按 WGS-84 保存，这样切换地图源、重启后恢复视图都不会平移。
  LatLng _toWgs84(LatLng point) {
    if (!_sourceFor(_style).gcj02) return point;
    final (lat, lon) = GcjCoordinates.gcj02ToWgs84(
      point.latitude,
      point.longitude,
    );
    return LatLng(lat, lon);
  }

  ///
  /// 数据来自后端缓存的 `HOME_POSITION` / `GLOBAL_POSITION_INT`，两个都是
  /// `valid` 才画——定位还没锁定时不画一个假的点在地图中心。
  List<Marker> _buildMarkers(MgColors colors) {
    final connection = widget.connectionController;
    final markers = <Marker>[];
    final home = connection.homePosition;
    if (connection.homePositionValid && home != null) {
      markers.add(
        Marker(
          point: _toTileFrame(home.latitude, home.longitude),
          width: 28,
          height: 28,
          child: _MapMarker(
            icon: CupertinoIcons.house_fill,
            // Home 用成功色，飞机用强调色，和界面其它状态色保持同一套语义。
            fill: colors.surface,
            border: colors.success,
            iconColor: colors.success,
          ),
        ),
      );
    }
    final fix = connection.vehiclePosition;
    if (connection.vehiclePositionValid && fix != null) {
      markers.add(
        Marker(
          point: _toTileFrame(fix.latitude, fix.longitude),
          width: 36,
          height: 36,
          child: _MapMarker(
            icon: CupertinoIcons.paperplane_fill,
            fill: colors.accent,
            border: colors.surface,
            iconColor: colors.onAccent,
            headingDegrees: fix.headingDegrees,
          ),
        ),
      );
    }
    return markers;
  }

  @override
  Widget build(BuildContext context) {
    final source = _sourceFor(_style);
    return Stack(
      fit: StackFit.expand,
      children: [
        FlutterMap(
          mapController: _controller,
          options: MapOptions(
            initialCenter: _center,
            initialZoom: _zoom,
            minZoom: kMinMapZoom,
            maxZoom: kMaxMapZoom,
            onPositionChanged: (camera, hasGesture) =>
                _scheduleSave(camera.center, camera.zoom),
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
            // 位置每 100ms 更新一次，只重画标记层，不重建整张地图。
            AnimatedBuilder(
              animation: widget.connectionController.telemetryChanges,
              builder: (context, _) =>
                  MarkerLayer(markers: _buildMarkers(context.mgColors)),
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
                      onTap: () => _selectStyle(style),
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
            // 有机上定位就回到飞机，否则回默认中心。
            onReset: () {
              final fix = widget.connectionController.vehiclePosition;
              final valid = widget.connectionController.vehiclePositionValid;
              _controller.move(
                valid && fix != null
                    ? _toTileFrame(fix.latitude, fix.longitude)
                    : _toTileFrame(
                        kDefaultMapCenter.latitude,
                        kDefaultMapCenter.longitude,
                      ),
                valid ? 15 : kDefaultMapZoom,
              );
            },
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
        appIcon: Icon(
          Icons.copyright_outlined,
          size: 64,
          color: MgColors.of(context).accent,
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

/// 地图上的圆点标记：Home 点用房子图标，飞机用按航向旋转的纸飞机。
///
/// 与 Mission Planner / QGC 的做法一致——Home 是静态图标，飞机标记跟随航向旋转，
/// 这样不用看数字也能判断机头朝向。
class _MapMarker extends StatelessWidget {
  const _MapMarker({
    required this.icon,
    required this.fill,
    required this.border,
    required this.iconColor,
    this.headingDegrees,
  });

  final IconData icon;
  final Color fill;
  final Color border;
  final Color iconColor;
  final double? headingDegrees;

  @override
  Widget build(BuildContext context) {
    final badge = Container(
      decoration: BoxDecoration(
        color: fill,
        shape: BoxShape.circle,
        border: Border.all(color: border, width: 2),
      ),
      child: Center(child: Icon(icon, size: 15, color: iconColor)),
    );
    final heading = headingDegrees;
    if (heading == null || !heading.isFinite) return badge;
    // 纸飞机图标默认朝右，而航向 0° 是正北，所以额外减 90°。
    return Transform.rotate(
      angle: (heading - 90) * math.pi / 180,
      child: badge,
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
      color: MgColors.of(context).mapControlSurface,
      borderRadius: BorderRadius.circular(9),
      border: Border.all(color: MgColors.of(context).divider),
      boxShadow: [
        BoxShadow(
          color: MgColors.of(context).shadow,
          blurRadius: 10,
          offset: const Offset(0, 3),
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
          child: Container(height: 1, color: MgColors.of(context).divider),
        ),
        _MapIconButton(
          icon: CupertinoIcons.slider_horizontal_3,
          tooltip: '参数配置',
          onPressed: onOpenConfiguration ?? () {},
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 9),
          child: Container(height: 1, color: MgColors.of(context).divider),
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
    final colors = MgColors.of(context);
    return MacosTooltip(
      message: tooltip,
      child: MacosIconButton(
        icon: Icon(icon, size: 18, color: colors.mapControlIcon),
        semanticLabel: tooltip,
        boxConstraints: const BoxConstraints.tightFor(width: 38, height: 38),
        padding: EdgeInsets.zero,
        backgroundColor: colors.transparent,
        hoverColor: colors.mapControlHover,
        onPressed: onPressed,
      ),
    );
  }
}
