import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../connection_panel.dart';
import '../flight/flight_message_panel.dart';
import '../flight/flight_mode_panel.dart';
import '../mission/mission_document_controller.dart';
import '../mission/mission_document_models.dart';
import '../mission/mission_geo.dart';
import '../mission/mission_planning_panel.dart';
import 'flight_telemetry_overlay.dart';
import 'gcj_coordinates.dart';
import 'vehicle_marker_icon.dart';
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
  /// 航线规划浮层距内容区顶边的间距。原先贴到顶边（18）显得压在工具栏下面，
  /// 这里整体下移，单独留一个常量方便继续微调。
  static const double _missionPanelTopInset = 60;

  final _controller = MapController();
  final _mapKey = GlobalKey();
  MapStyle _style = MapStyle.amap;
  double _zoom = kDefaultMapZoom;
  LatLng _center = kDefaultMapCenter;

  /// 偏好读完之前不写盘，否则会把默认值覆盖掉用户上次的选择。
  bool _preferencesLoaded = false;
  /// 本会话是否已经因为首次定位移动过视角。
  bool _centeredOnVehicle = false;
  Timer? _saveTimer;
  bool _showConnectionPanel = false;
  bool _showFlightModePanel = false;
  bool _showFlightMessagePanel = false;
  bool _showMissionPlanning = false;
  OverlayEntry? _transientMessage;
  String? _draggingEntityId;
  LatLng? _dragPreviewWgs84;

  /// 地图标记要同时跟着遥测和任务文档变化重画。
  late final Listenable _markerSources = Listenable.merge([
    widget.connectionController.telemetryChanges,
    widget.connectionController.missionPlanning,
  ]);

  @override
  void initState() {
    super.initState();
    widget.connectionController.addListener(_centerOnVehicleOnce);
    unawaited(_restorePreferences());
  }

  /// 首次拿到有效定位时把视角拉回飞机。
  ///
  /// 地图中心是跨会话保存的，上次可能停在别的地方（例如默认的上海），
  /// 而飞机在几千公里外，连上了却看不到飞机。只有距离超过 20 km 才动视角，
  /// 免得覆盖用户正常操作；每个会话只做一次。
  void _centerOnVehicleOnce() {
    if (_centeredOnVehicle || !mounted) return;
    final connection = widget.connectionController;
    if (!connection.vehiclePositionValid) return;
    final fix = connection.vehiclePosition;
    if (fix == null) return;
    _centeredOnVehicle = true;
    final distance = wgs84DistanceMeters(
      _center,
      LatLng(fix.latitude, fix.longitude),
    );
    if (distance < 20000) return;
    _controller.move(
      _toTileFrame(fix.latitude, fix.longitude),
      _zoom < 13 ? 15 : _zoom,
    );
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
    widget.connectionController.removeListener(_centerOnVehicleOnce);
    _saveTimer?.cancel();
    _transientMessage?.remove();
    _transientMessage = null;
    super.dispose();
  }

  void _showTransientMessage(String message) {
    final overlay = Overlay.of(context, rootOverlay: true);
    _transientMessage?.remove();

    late final OverlayEntry entry;
    entry = OverlayEntry(
      builder: (overlayContext) => _TransientMapMessage(
        message: message,
        onDismissed: () {
          if (_transientMessage == entry) {
            _transientMessage?.remove();
            _transientMessage = null;
          }
        },
      ),
    );
    _transientMessage = entry;
    overlay.insert(entry);
  }

  /// Home 点与飞机当前位置。
  ///
  /// 坐标一律先转到当前瓦片所在的坐标系：飞控给的是 WGS-84，高德瓦片是
  /// GCJ-02，不转换会整体偏 100–700 米（Mission Planner 也是这么处理的）。
  /// 单击地图：规划页开着就新增航点并打开编辑框，否则弹引导操作。
  ///
  /// 与 MP 的右键菜单 / QGC 的 Guided Actions 一致，都是“先选位置再选动作”。
  Future<void> _handleMapTap(LatLng point) async {
    // 瓦片坐标（可能是 GCJ-02）换算回 WGS-84 再交给文档与飞控。
    final wgs84 = _toWgs84(point);
    final controller = widget.connectionController;
    final mission = controller.missionPlanning;
    await mission.initialize();
    if (!mounted) return;

    if (_showMissionPlanning) {
      await showMissionEntityDialog(
        context,
        controller: controller,
        catalog: mission.commandCatalog,
        initialPosition: wgs84,
        onSave: (entity) async {
          await mission.addEntity(entity);
        },
      );
      return;
    }

    if (!controller.isConnected) {
      _showTransientMessage('连接飞控后才能在这里发送引导指令。');
      return;
    }

    await showGuidedActionsDialog(
      context,
      controller: controller,
      position: wgs84,
    );
  }

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
  List<Marker> _buildMarkers(BuildContext context, MgColors colors) {
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
          width: 42,
          height: 42,
          // 按机型画机体剪影（多旋翼/固定翼/VTOL），不再用统一的纸飞机图标。
          child: VehicleMarkerIcon(
            kind: vehicleMarkerKindFor(
              connection.controlProfile,
              connection.vehicleType,
            ),
            fill: colors.accent,
            outline: colors.surface,
            headingDegrees: fix.headingDegrees,
          ),
        ),
      );
    }
    // 任务文档里的航点：画成编号标记，点击打开编辑框。
    final mission = connection.missionPlanning;
    final entities = mission.document?.entities ?? const <MissionEntity>[];
    final dragEnabled = !mission.busy && !mission.transferActive;
    for (var index = 0; index < entities.length; index++) {
      final entity = entities[index];
      final dragging = entity.id == _draggingEntityId;
      final preview = dragging ? _dragPreviewWgs84 : null;
      final latitude = preview?.latitude ?? entity.latitude;
      final longitude = preview?.longitude ?? entity.longitude;
      if (latitude == null || longitude == null) continue;
      markers.add(
        Marker(
          point: _toTileFrame(latitude, longitude),
          width: 26,
          height: 26,
          child: MouseRegion(
            cursor: dragging
                ? SystemMouseCursors.grabbing
                : SystemMouseCursors.move,
            child: GestureDetector(
              onTap: () => mission.selectEntity(index),
              onDoubleTap: () => unawaited(_editEntity(mission, entity)),
              onPanStart: dragEnabled
                  ? (_) => _beginEntityDrag(index, entity)
                  : null,
              onPanUpdate: dragEnabled
                  ? (details) => _updateEntityDrag(details.globalPosition)
                  : null,
              onPanEnd: dragEnabled
                  ? (_) => unawaited(_finishEntityDrag(mission, entity))
                  : null,
              onPanCancel: dragEnabled ? _cancelEntityDrag : null,
              child: _WaypointMarker(
                position: index + 1,
                color: colors.accent,
                selected: mission.selectedEntityIndex == index,
                dragging: dragging,
              ),
            ),
          ),
        ),
      );
    }

    return markers;
  }

  void _beginEntityDrag(int index, MissionEntity entity) {
    final latitude = entity.latitude;
    final longitude = entity.longitude;
    if (latitude == null || longitude == null) return;
    widget.connectionController.missionPlanning.selectEntity(index);
    setState(() {
      _draggingEntityId = entity.id;
      _dragPreviewWgs84 = LatLng(latitude, longitude);
    });
  }

  void _updateEntityDrag(Offset globalPosition) {
    if (_draggingEntityId == null) return;
    final mapContext = _mapKey.currentContext;
    final renderObject = mapContext?.findRenderObject();
    if (mapContext == null || renderObject is! RenderBox) return;
    final localPosition = renderObject.globalToLocal(globalPosition);
    final tilePoint = _controller.camera.screenOffsetToLatLng(localPosition);
    setState(() => _dragPreviewWgs84 = _toWgs84(tilePoint));
  }

  Future<void> _finishEntityDrag(
    MissionDocumentController mission,
    MissionEntity entity,
  ) async {
    final preview = _dragPreviewWgs84;
    if (_draggingEntityId != entity.id || preview == null) {
      _cancelEntityDrag();
      return;
    }
    try {
      await mission.updateEntity(
        entity.copyWith(
          latitude: preview.latitude,
          longitude: preview.longitude,
        ),
      );
    } finally {
      if (mounted) _cancelEntityDrag();
    }
  }

  void _cancelEntityDrag() {
    if (!mounted) return;
    setState(() {
      _draggingEntityId = null;
      _dragPreviewWgs84 = null;
    });
  }

  List<_MissionRouteVertex> _missionRouteVertices() {
    final mission = widget.connectionController.missionPlanning;
    final entities = mission.document?.entities ?? const <MissionEntity>[];
    final vertices = <_MissionRouteVertex>[];
    for (final entity in entities) {
      if (!entity.enabled) continue;
      final dragging = entity.id == _draggingEntityId;
      final preview = dragging ? _dragPreviewWgs84 : null;
      final latitude = preview?.latitude ?? entity.latitude;
      final longitude = preview?.longitude ?? entity.longitude;
      if (latitude == null ||
          longitude == null ||
          !latitude.isFinite ||
          !longitude.isFinite) {
        continue;
      }
      final wgs84 = LatLng(latitude, longitude);
      vertices.add(
        _MissionRouteVertex(
          wgs84: wgs84,
          tileFrame: _toTileFrame(latitude, longitude),
        ),
      );
    }
    return vertices;
  }

  List<Polyline> _buildMissionRouteLines(BuildContext context) {
    final vertices = _missionRouteVertices();
    if (vertices.length < 2) return const [];
    final colors = context.mgColors;
    return [
      Polyline(
        points: [for (final vertex in vertices) vertex.tileFrame],
        color: colors.accent.withValues(alpha: 0.82),
        strokeWidth: 2.2,
        pattern: StrokePattern.dashed(segments: const [8, 6]),
      ),
    ];
  }

  List<Marker> _buildMissionDistanceLabels(BuildContext context) {
    final vertices = _missionRouteVertices();
    if (vertices.length < 2) return const [];
    final labels = <Marker>[];
    for (var index = 0; index < vertices.length - 1; index++) {
      final start = vertices[index];
      final end = vertices[index + 1];
      final distance = wgs84DistanceMeters(start.wgs84, end.wgs84);
      final midpoint = LatLng(
        (start.tileFrame.latitude + end.tileFrame.latitude) / 2,
        (start.tileFrame.longitude + end.tileFrame.longitude) / 2,
      );
      labels.add(
        Marker(
          point: midpoint,
          width: 86,
          height: 22,
          child: IgnorePointer(
            child: _DistanceLabel(text: formatMissionDistance(distance)),
          ),
        ),
      );
    }
    return labels;
  }

  /// 飞机到 Home 的虚线。
  ///
  /// 距离用 WGS-84 算（和任务航段一致），只有显示用的坐标才走瓦片坐标系换算。
  /// 样式刻意和任务航线不同：更细、点划更密、成功色。
  /// 贴地时飞机就在 Home 上，零长度连线只会让标签压住图标，所以低于
  /// [kHomeLineMinDistanceMeters] 不画。
  static const double kHomeLineMinDistanceMeters = 5.0;

  bool _homeLineVisible() {
    final connection = widget.connectionController;
    final home = connection.homePosition;
    final fix = connection.vehiclePosition;
    if (!connection.homePositionValid || home == null) return false;
    if (!connection.vehiclePositionValid || fix == null) return false;
    return wgs84DistanceMeters(
          LatLng(fix.latitude, fix.longitude),
          LatLng(home.latitude, home.longitude),
        ) >=
        kHomeLineMinDistanceMeters;
  }

  List<Polyline> _buildHomeLine(BuildContext context) {
    final connection = widget.connectionController;
    final home = connection.homePosition;
    final fix = connection.vehiclePosition;
    if (!_homeLineVisible() || home == null || fix == null) return const [];
    final colors = context.mgColors;
    return [
      Polyline(
        points: [
          _toTileFrame(fix.latitude, fix.longitude),
          _toTileFrame(home.latitude, home.longitude),
        ],
        color: colors.success.withValues(alpha: 0.72),
        strokeWidth: 1.6,
        pattern: StrokePattern.dashed(segments: const [5, 5]),
      ),
    ];
  }

  /// 飞机到 Home 的距离标签，放在连线中点。
  List<Marker> _buildHomeDistanceLabel(BuildContext context) {
    final connection = widget.connectionController;
    final home = connection.homePosition;
    final fix = connection.vehiclePosition;
    if (!_homeLineVisible() || home == null || fix == null) return const [];
    final start = _toTileFrame(fix.latitude, fix.longitude);
    final end = _toTileFrame(home.latitude, home.longitude);
    final distance = wgs84DistanceMeters(
      LatLng(fix.latitude, fix.longitude),
      LatLng(home.latitude, home.longitude),
    );
    return [
      Marker(
        point: LatLng(
          (start.latitude + end.latitude) / 2,
          (start.longitude + end.longitude) / 2,
        ),
        width: 120,
        height: 22,
        child: IgnorePointer(
          child: _DistanceLabel(
            text: '距 Home ${formatMissionDistance(distance)}',
          ),
        ),
      ),
    ];
  }

  /// 编辑已有航点：与地图单击新增走同一个编辑框。
  Future<void> _editEntity(
    MissionDocumentController mission,
    MissionEntity entity,
  ) async {
    await showMissionEntityDialog(
      context,
      controller: widget.connectionController,
      catalog: mission.commandCatalog,
      entity: entity,
      onSave: (updated) async {
        await mission.updateEntity(updated);
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final source = _sourceFor(_style);
    return Stack(
      fit: StackFit.expand,
      children: [
        FlutterMap(
          key: _mapKey,
          mapController: _controller,
          options: MapOptions(
            initialCenter: _center,
            initialZoom: _zoom,
            minZoom: kMinMapZoom,
            maxZoom: kMaxMapZoom,
            // 单击地图：规划页开着就加航点，否则给引导操作。
            onTap: (_, point) => unawaited(_handleMapTap(point)),
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
              animation: _markerSources,
              builder: (context, _) => PolylineLayer(
                polylines: [
                  ..._buildHomeLine(context),
                  ..._buildMissionRouteLines(context),
                ],
              ),
            ),
            AnimatedBuilder(
              animation: _markerSources,
              builder: (context, _) => MarkerLayer(
                markers: [
                  ..._buildHomeDistanceLabel(context),
                  ..._buildMissionDistanceLabels(context),
                  ..._buildMarkers(context, context.mgColors),
                ],
              ),
            ),
          ],
        ),
        Positioned(
          top: 16,
          left: 70,
          child: LayoutBuilder(
            builder: (context, constraints) {
              final rightInset = _showMissionPlanning
                  ? kMissionPanelWidth + 36
                  : 18;
              final maxWidth = constraints.maxWidth.isFinite
                  ? math.min(
                      680.0,
                      math.max(260.0, constraints.maxWidth - rightInset),
                    )
                  : 680.0;
              return IgnorePointer(
                child: FlightTelemetryOverlay(
                  controller: widget.connectionController,
                  maxWidth: maxWidth,
                ),
              );
            },
          ),
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
            // 三个浮层互不排斥，可以同时开着（连接 / 航线规划 / 飞行模式）。
            onOpenConnection: () =>
                setState(() => _showConnectionPanel = !_showConnectionPanel),
            onOpenMissionPlanning: () =>
                setState(() => _showMissionPlanning = !_showMissionPlanning),
            onOpenFlightModes: () =>
                setState(() => _showFlightModePanel = !_showFlightModePanel),
            onOpenFlightMessages: () => setState(
              () => _showFlightMessagePanel = !_showFlightMessagePanel,
            ),
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
          // 贴右边缘悬浮：高度由 top/bottom 约束决定，所以窗口缩放时自动跟随；
          // 不给 left，宽度交给面板自己决定（展开 / 折叠成箭头按钮）。
          Positioned(
            top: _missionPanelTopInset,
            // 让开右下角的底图选择 / 帮助按钮行，避免叠在它们上面。
            bottom: 18 + MgMetrics.controlHeight + 12,
            right: 18,
            child: MissionPlanningFloatingPanel(
              controller: widget.connectionController,
              onClose: () => setState(() => _showMissionPlanning = false),
            ),
          ),
        if (_showFlightMessagePanel)
          // 紧挨飞行模式面板右侧；飞行模式面板收起时补到最左侧。上下留白与它一致，
          // 这样两个浮层和连接面板可以同时开着且互不遮挡。
          Positioned(
            left: _showFlightModePanel ? 70 + kFlightModePanelWidth + 12 : 70,
            top: 170,
            bottom: _showConnectionPanel ? 330 : 90,
            child: FlightMessageFloatingPanel(
              controller: widget.connectionController,
              onClose: () => setState(() => _showFlightMessagePanel = false),
            ),
          ),
        if (_showFlightModePanel)
          // 左侧空带：上方让开飞行遥测条，下方按连接面板是否打开让位。
          // top/bottom 同时给出紧约束，面板内部才能用 Expanded 撑满并滚动。
          Positioned(
            left: 70,
            top: 170,
            bottom: _showConnectionPanel ? 330 : 90,
            child: FlightModeFloatingPanel(
              controller: widget.connectionController,
              onClose: () => setState(() => _showFlightModePanel = false),
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

class _TransientMapMessage extends StatefulWidget {
  const _TransientMapMessage({
    required this.message,
    required this.onDismissed,
  });

  final String message;
  final VoidCallback onDismissed;

  @override
  State<_TransientMapMessage> createState() => _TransientMapMessageState();
}

class _TransientMapMessageState extends State<_TransientMapMessage>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 180),
  );
  late final Animation<double> _opacity = CurvedAnimation(
    parent: _controller,
    curve: Curves.easeOut,
  );
  late final Animation<Offset> _offset = Tween<Offset>(
    begin: const Offset(0, 0.18),
    end: Offset.zero,
  ).animate(_opacity);
  Timer? _dismissTimer;

  @override
  void initState() {
    super.initState();
    _controller.forward();
    _dismissTimer = Timer(const Duration(seconds: 2), _dismiss);
  }

  Future<void> _dismiss() async {
    if (!mounted) return;
    await _controller.reverse();
    if (mounted) widget.onDismissed();
  }

  @override
  void dispose() {
    _dismissTimer?.cancel();
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Positioned(
      left: 0,
      right: 0,
      bottom: MediaQuery.of(context).padding.bottom + 82,
      child: IgnorePointer(
        child: Center(
          child: FadeTransition(
            opacity: _opacity,
            child: SlideTransition(
              position: _offset,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 360),
                child: Container(
                  margin: const EdgeInsets.symmetric(horizontal: 18),
                  padding: const EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: 9,
                  ),
                  decoration: BoxDecoration(
                    color: colors.surface,
                    border: Border.all(color: colors.divider),
                    borderRadius: BorderRadius.circular(
                      MgMetrics.controlRadius,
                    ),
                    boxShadow: [
                      BoxShadow(
                        color: colors.shadow,
                        blurRadius: 12,
                        offset: const Offset(0, 4),
                      ),
                    ],
                  ),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      MacosIcon(
                        CupertinoIcons.bolt_horizontal_circle,
                        size: 15,
                        color: colors.warning,
                      ),
                      const SizedBox(width: 8),
                      Flexible(
                        child: Text(
                          widget.message,
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                          style: MacosTheme.of(context).typography.caption1,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _MissionRouteVertex {
  const _MissionRouteVertex({required this.wgs84, required this.tileFrame});

  final LatLng wgs84;
  final LatLng tileFrame;
}

class _DistanceLabel extends StatelessWidget {
  const _DistanceLabel({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Center(
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: colors.surface.withValues(alpha: 0.9),
          borderRadius: BorderRadius.circular(4),
          border: Border.all(color: colors.divider),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1),
          child: Text(
            text,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(
              context,
            ).typography.caption1.copyWith(color: colors.textPrimary),
          ),
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
  });

  final IconData icon;
  final Color fill;
  final Color border;
  final Color iconColor;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: fill,
        shape: BoxShape.circle,
        border: Border.all(color: border, width: 2),
      ),
      child: Center(child: Icon(icon, size: 15, color: iconColor)),
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
    this.onOpenFlightModes,
    this.onOpenFlightMessages,
  });

  final VoidCallback onZoomIn;
  final VoidCallback onZoomOut;
  final VoidCallback onReset;
  final VoidCallback? onOpenConfiguration;
  final VoidCallback onOpenConnection;
  final VoidCallback onOpenMissionPlanning;
  final VoidCallback? onOpenFlightModes;
  final VoidCallback? onOpenFlightMessages;

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
        _MapIconButton(
          icon: CupertinoIcons.airplane,
          tooltip: '飞行模式',
          onPressed: onOpenFlightModes ?? () {},
        ),
        _MapIconButton(
          icon: CupertinoIcons.text_bubble,
          tooltip: '飞控消息',
          onPressed: onOpenFlightMessages ?? () {},
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

/// 地图上的航点标记：一个带编号的小圆点。
class _WaypointMarker extends StatelessWidget {
  const _WaypointMarker({
    required this.position,
    required this.color,
    this.selected = false,
    this.dragging = false,
  });

  final int position;
  final Color color;
  final bool selected;
  final bool dragging;

  @override
  Widget build(BuildContext context) => Transform.scale(
    scale: dragging ? 1.28 : (selected ? 1.18 : 1),
    child: Container(
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: color,
        shape: BoxShape.circle,
        border: Border.all(
          color: selected ? context.mgColors.warning : context.mgColors.surface,
          width: selected ? 2.5 : 1.5,
        ),
      ),
      child: Text(
        '$position',
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(color: context.mgColors.onAccent),
      ),
    ),
  );
}
