import '../mission/quick_mission_geometry.dart';
import '../mission/quick_mission_draft.dart';
import '../mission/quick_mission_panel.dart';
import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../connection_panel.dart';
import '../fleet/fleet_models.dart';
import '../fleet/squad_floating_panel.dart';
import '../flight/flight_message_panel.dart';
import '../mission/mission_document_controller.dart';
import '../mission/mission_document_models.dart';
import '../mission/mission_geo.dart';
import '../mission/geofence_floating_panel.dart';
import '../mission/mission_planning_panel.dart';
import '../widgets/mg_fleet_batch_panel.dart';
import '../widgets/mg_map_action_menu.dart';
import '../widgets/mg_command_confirmation.dart';
import '../widgets/mg_mac_glass_attitude_panel.dart';
import '../widgets/mg_floating_panel.dart';
import '../widgets/mg_telemetry_dashboard.dart';

import 'gcj_coordinates.dart';
import 'vehicle_marker_icon.dart';
import 'map_preferences.dart';

enum MapStyle {
  standard,
  terrain,
  amap,
  amapSatellite,
  amapSatelliteHybrid,
  custom,
}

/// 地图源显示名（地面站配置页也要用，所以公开）。
String mapStyleLabel(MapStyle style, {String customTileUrl = ''}) =>
    _sourceFor(style, customTileUrl: customTileUrl).label;

/// 地图源署名要求（原先是地图右下角“?”里的内容，现在搬到配置页）。
String mapStyleAttribution(MapStyle style, {String customTileUrl = ''}) =>
    _sourceFor(style, customTileUrl: customTileUrl).attribution;

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

/// 自定义瓦片源（KD-37）：地址由使用者在设置页填写，支持 `{z}/{x}/{y}` 占位符。
///
/// 坐标按 WGS-84 处理——自定义地址无法自动判断坐标系，填高德一类的 GCJ-02 源
/// 会出现偏移；地址还没配好时退回标准底图，保证地图不会白屏。
_MapSource _customSource(String raw) {
  if (!isCustomTileUrlUsable(raw)) {
    return const _MapSource(
      label: '自定义瓦片源（未配置，暂用标准底图）',
      url: 'https://b.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
      fallbackUrl: 'https://a.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
      maxNativeZoom: 19,
      attribution: '© OpenStreetMap contributors | HOT',
    );
  }
  return _MapSource(
    label: '自定义瓦片源',
    url: raw.trim(),
    maxNativeZoom: 19,
    attribution: '瓦片由自定义地址提供；署名与使用条款由使用者负责。',
  );
}

_MapSource _sourceFor(
  MapStyle style, {
  String customTileUrl = '',
}) => switch (style) {
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
  MapStyle.custom => _customSource(customTileUrl),
};

/// 地图右侧的主面板。多机面板是独立的次级 inspector。
enum MapRightPanel { none, connection, messages, mission, geofence, squads }

const double kMapPanelEdgeInset = 18;
const double kMapPanelGap = 12;

MapRightPanel nextMapRightPanel(
  MapRightPanel current,
  MapRightPanel requested,
) => requested;

double mapRightPanelWidth(MapRightPanel panel) => switch (panel) {
  MapRightPanel.connection => kConnectionPanelMaxWidth,
  MapRightPanel.messages => kFlightMessagePanelWidth,
  MapRightPanel.mission => kMissionPanelWidth,
  MapRightPanel.geofence => kGeofencePanelWidth,
  MapRightPanel.squads => kSquadPanelWidth,
  MapRightPanel.none => 0,
};

double fleetPanelRightInset(MapRightPanel primary) {
  final width = mapRightPanelWidth(primary);
  return kMapPanelEdgeInset + (width > 0 ? width + kMapPanelGap : 0);
}

bool canShowFleetAlongside(double availableWidth, MapRightPanel primary) =>
    availableWidth >=
    fleetPanelRightInset(primary) +
        MgMetrics.fleetPanelWidth +
        kMapPanelEdgeInset +
        280; // Leave a usable strip of map beside both inspectors.

const kMapRightPanelTopInset = 12.0;
const kMapMessagePanelHeight = 340.0;

/// Bounds every inspector using the actual map area supplied by its parent.
class MapPanelSlot extends StatelessWidget {
  const MapPanelSlot({
    super.key,
    required this.child,
    required this.preferredWidth,
    this.preferredHeight,
    this.rightInset = kMapPanelEdgeInset,
    this.reserveInstruments = false,
  });

  final Widget child;
  final double preferredWidth;
  final double? preferredHeight;
  final double rightInset;
  final bool reserveInstruments;

  @override
  Widget build(BuildContext context) => Positioned.fill(
    child: LayoutBuilder(
      builder: (context, constraints) {
        final bottom = reserveInstruments
            ? (constraints.maxHeight < 450
                  ? 18 + MgMetrics.dashboardHeight + 12
                  : 18 + MgMetrics.attitudeCompassSize + 12)
            : 18.0;
        final width = math.min(
          preferredWidth,
          math.max(0.0, constraints.maxWidth - rightInset - kMapPanelEdgeInset),
        );
        final availableHeight = math.max(
          0.0,
          constraints.maxHeight - kMapRightPanelTopInset - bottom,
        );
        return Align(
          alignment: Alignment.topRight,
          child: Padding(
            padding: EdgeInsets.only(
              top: kMapRightPanelTopInset,
              right: rightInset,
            ),
            child: ConstrainedBox(
              constraints: BoxConstraints(maxHeight: availableHeight),
              child: SizedBox(
                width: width,
                height: preferredHeight == null
                    ? null
                    : math.min(preferredHeight!, availableHeight),
                child: child,
              ),
            ),
          ),
        );
      },
    ),
  );
}

class MapPlanningPanelSlot extends StatelessWidget {
  const MapPlanningPanelSlot({
    super.key,
    required this.child,
    this.preferredWidth = kMissionPanelWidth,
  });
  final Widget child;
  final double preferredWidth;

  @override
  Widget build(BuildContext context) => MapPanelSlot(
    preferredWidth: preferredWidth,
    preferredHeight: double.infinity,
    reserveInstruments: true,
    child: child,
  );
}

class AMapMapView extends StatefulWidget {
  const AMapMapView({
    super.key,
    required this.connectionController,
    this.mapStyle,
    this.customTileUrl,
    this.connectionPanelRequest,
    this.flightMessagePanelRequest,
    this.missionPlanningRequest,
    this.geofenceRequest,
    this.squadPanelRequest,
    this.fleetPanelRequest,
    this.onSelectFleetVehicle,
    this.onRequestFleetBatchAction,
    this.onOpenFleetFlightMode,
    this.onRequestCommandConfirmation,
  });

  final ConnectionController connectionController;

  /// 地图样式的唯一真源：设置页写、地图页读并持久化。
  final ValueNotifier<MapStyle>? mapStyle;

  /// 自定义瓦片源地址模板（KD-37）；设置页改这里，地图跟着换图层。
  final ValueNotifier<String>? customTileUrl;
  final ValueListenable<int>? connectionPanelRequest;
  final ValueListenable<int>? flightMessagePanelRequest;
  final ValueListenable<int>? missionPlanningRequest;
  final ValueListenable<int>? geofenceRequest;
  final ValueListenable<int>? squadPanelRequest;
  final ValueListenable<int>? fleetPanelRequest;

  /// 多机面板的飞机焦点切换和批量动作交给主页面统一处理。
  final Future<void> Function(int vehicleId)? onSelectFleetVehicle;
  final void Function(FleetBatchTarget target)? onRequestFleetBatchAction;
  final ValueChanged<int>? onOpenFleetFlightMode;

  /// 把飞行命令交给主页面顶部状态栏，等待用户长按确认后再执行。
  final ValueChanged<MgCommandConfirmation>? onRequestCommandConfirmation;

  @override
  State<AMapMapView> createState() => _AMapMapViewState();
}

class _AMapMapViewState extends State<AMapMapView> {
  final _controller = MapController();
  final _mapKey = GlobalKey();
  MapStyle _style = MapStyle.amap;
  String _customTileUrl = '';
  double _zoom = kDefaultMapZoom;
  LatLng _center = kDefaultMapCenter;

  /// 偏好读完之前不写盘，否则会把默认值覆盖掉用户上次的选择。
  bool _preferencesLoaded = false;

  /// 本会话是否已经因为首次定位移动过视角。
  bool _centeredOnVehicle = false;
  bool _followVehicle = false;
  Timer? _saveTimer;
  QuickMissionDraft? _quickDraft;
  MissionDocumentController? _quickOwner;
  int? _quickSession;
  int? _quickRevision;

  MapRightPanel _rightPanel = MapRightPanel.none;
  bool _fleetPanelVisible = true;
  OverlayEntry? _transientMessage;

  /// 地图点击菜单：锚点是点击处在地图控件内的坐标，点位是换算后的 WGS-84 坐标。
  Offset? _mapActionAnchor;
  LatLng? _mapActionPoint;

  /// 菜单开着时点了地图上别处：这一下只用来关闭，下一次地图点击不再重开菜单。
  bool _suppressNextMapTap = false;
  String? _draggingEntityId;
  LatLng? _dragPreviewWgs84;

  int? _markerSessionId;
  Timer? _fleetFitDebounce;
  Set<int> _lastFittedFleetVehicleIds = const {};
  late final Listenable _activeMapSources = Listenable.merge([
    widget.connectionController.positionChanges,
    widget.connectionController.flightStateChanges,
    widget.connectionController.connectionChanges,
    widget.connectionController.parameterChanges,
    widget.connectionController.profileChanges,
    widget.connectionController.stateChanges,
  ]);
  late Listenable _missionMapSources =
      widget.connectionController.missionPlanning;
  late final Listenable _fleetMapSources = widget.connectionController.fleet;
  late final Listenable _fleetMarkerSources = Listenable.merge([
    _fleetMapSources,
    widget.connectionController.fleetPositions,
  ]);
  late final Listenable _attitudeSources = Listenable.merge([
    widget.connectionController.positionChanges,
    widget.connectionController.attitudeChanges,
    widget.connectionController.flightStateChanges,
    widget.connectionController.connectionChanges,
    widget.connectionController.stateChanges,
  ]);
  late final Listenable _dashboardSources = Listenable.merge([
    widget.connectionController.positionChanges,
    widget.connectionController.flightDataChanges,
    widget.connectionController.stateChanges,
  ]);

  /// 飞行动作按钮的显隐/可用性由连接与遥测决定，必须跟着这两路通知重建。
  late final Listenable _vehicleStatusSources = Listenable.merge([
    widget.connectionController.connectionChanges,
    widget.connectionController.flightStateChanges,
    widget.connectionController.profileChanges,
    widget.connectionController.stateChanges,
  ]);

  @override
  void initState() {
    super.initState();
    widget.connectionController.addListener(_handleVehicleUpdate);
    widget.mapStyle?.addListener(_handleExternalStyleChange);
    widget.customTileUrl?.addListener(_handleExternalCustomUrlChange);
    widget.connectionPanelRequest?.addListener(_handleConnectionPanelRequest);
    widget.flightMessagePanelRequest?.addListener(
      _handleFlightMessagePanelRequest,
    );
    widget.missionPlanningRequest?.addListener(_handleMissionPlanningRequest);
    widget.geofenceRequest?.addListener(_handleGeofenceRequest);
    widget.squadPanelRequest?.addListener(_handleSquadPanelRequest);
    widget.fleetPanelRequest?.addListener(_handleFleetPanelRequest);
    unawaited(_restorePreferences());
  }

  void _showRightPanel(MapRightPanel panel) {
    setState(() {
      _clearQuickDraw();
      _rightPanel = nextMapRightPanel(_rightPanel, panel);
    });
  }

  void _handleConnectionPanelRequest() {
    if (mounted) _showRightPanel(MapRightPanel.connection);
  }

  void _handleFlightMessagePanelRequest() {
    if (mounted) _showRightPanel(MapRightPanel.messages);
  }

  void _handleMissionPlanningRequest() {
    if (mounted) _showRightPanel(MapRightPanel.mission);
  }

  void _handleGeofenceRequest() {
    if (mounted) _showRightPanel(MapRightPanel.geofence);
  }

  void _handleSquadPanelRequest() {
    if (mounted) _showRightPanel(MapRightPanel.squads);
  }

  void _handleFleetPanelRequest() {
    if (!mounted) return;
    final width =
        _mapKey.currentContext?.size?.width ?? MediaQuery.sizeOf(context).width;
    if (!canShowFleetAlongside(width, _rightPanel)) {
      // Narrow window: make room by closing the primary panel instead of
      // pushing the fleet inspector off-screen.
      setState(() {
        _clearQuickDraw();
        _rightPanel = MapRightPanel.none;
        _fleetPanelVisible = true;
      });
      return;
    }
    setState(() => _fleetPanelVisible = !_fleetPanelVisible);
  }

  /// 遥测定位更新时决定是否移动视角。
  ///
  /// 两种情况会动视角：
  /// - 视角跟随已开启：每次定位更新都居中飞机，不做距离判断；
  /// - 本会话首次拿到有效定位，且飞机离上次保存的地图中心超过 20 km：
  ///   地图中心是跨会话保存的，上次可能停在别的地方（例如默认的上海），
  ///   飞机在几千公里外，连上了却看不到飞机。距离近时不动，免得覆盖用户操作。
  ///
  /// 跟随关闭且已经处理过首次定位后直接返回，用户手动拖动的地图不会被抢回去。
  void _handleVehicleUpdate() {
    if (!mounted) return;
    final connection = widget.connectionController;
    if (_markerSessionId != connection.sessionId) {
      _markerSessionId = connection.sessionId;
      setState(() {
        _clearQuickDraw();
        _missionMapSources = connection.missionPlanning;
        _mapActionAnchor = null;
        _mapActionPoint = null;
        _draggingEntityId = null;
        _dragPreviewWgs84 = null;
      });
    }
    _scheduleFleetFit(connection);
    if (!connection.vehiclePositionValid) return;
    final fix = connection.vehiclePosition;
    if (fix == null) return;
    if (!shouldMoveCameraToVehicle(
      followVehicle: _followVehicle,
      centeredOnce: _centeredOnVehicle,
    )) {
      return;
    }

    final tilePosition = _toTileFrame(fix.latitude, fix.longitude);
    if (_followVehicle) {
      _centeredOnVehicle = true;
      _controller.move(tilePosition, _zoom);
      return;
    }
    _centeredOnVehicle = true;
    final distance = wgs84DistanceMeters(
      _center,
      LatLng(fix.latitude, fix.longitude),
    );
    if (distance < 20000) return;
    _controller.move(tilePosition, _zoom < 13 ? 15 : _zoom);
  }

  void _scheduleFleetFit(ConnectionController connection) {
    final positionedIds = connection.fleetSnapshot.vehicles
        .where((vehicle) => vehicle.positionValid)
        .map((vehicle) => vehicle.id)
        .toSet();
    if (positionedIds.length < 2 ||
        setEquals(positionedIds, _lastFittedFleetVehicleIds)) {
      return;
    }
    _fleetFitDebounce?.cancel();
    _fleetFitDebounce = Timer(const Duration(milliseconds: 350), () {
      if (!mounted) return;
      final points = <LatLng>[];
      for (final vehicle in connection.fleetSnapshot.vehicles) {
        if (!vehicle.positionValid || !positionedIds.contains(vehicle.id)) {
          continue;
        }
        points.add(_toTileFrame(vehicle.latitude, vehicle.longitude));
      }
      if (points.length < 2) return;
      _lastFittedFleetVehicleIds = positionedIds;
      setState(() => _followVehicle = false);
      _centeredOnVehicle = true;
      _controller.fitCamera(
        CameraFit.bounds(
          bounds: LatLngBounds.fromPoints(points),
          padding: const EdgeInsets.fromLTRB(80, 100, 80, 130),
          maxZoom: 15,
        ),
      );
    });
  }

  void _toggleFollowVehicle() {
    final enable = !_followVehicle;
    setState(() {
      _followVehicle = enable;
      // 重新开启跟随时忘记“首次定位已处理”，这样飞机没再移动也会立刻回中。
      if (enable) _centeredOnVehicle = false;
    });
    if (enable) _handleVehicleUpdate();
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
      _customTileUrl = preferences.customTileUrl;
      widget.mapStyle?.value = _style;
      widget.customTileUrl?.value = _customTileUrl;
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
          customTileUrl: _customTileUrl,
        ).save(),
      );
    });
  }

  /// 设置页改了地图样式：跟着切图层并持久化。
  void _handleExternalStyleChange() {
    final style = widget.mapStyle?.value;
    if (style == null || style == _style) return;
    _selectStyle(style);
  }

  /// 设置页改了自定义瓦片地址：跟着换源并持久化。
  void _handleExternalCustomUrlChange() {
    final url = widget.customTileUrl?.value ?? '';
    if (url == _customTileUrl) return;
    setState(() => _customTileUrl = url);
    if (!_preferencesLoaded) return;
    unawaited(
      MapPreferences(
        styleName: _style.name,
        zoom: _zoom,
        center: _center,
        customTileUrl: _customTileUrl,
      ).save(),
    );
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
        customTileUrl: _customTileUrl,
      ).save(),
    );
  }

  @override
  void dispose() {
    widget.connectionController.removeListener(_handleVehicleUpdate);
    widget.mapStyle?.removeListener(_handleExternalStyleChange);
    widget.customTileUrl?.removeListener(_handleExternalCustomUrlChange);
    widget.connectionPanelRequest?.removeListener(
      _handleConnectionPanelRequest,
    );
    widget.flightMessagePanelRequest?.removeListener(
      _handleFlightMessagePanelRequest,
    );
    widget.missionPlanningRequest?.removeListener(
      _handleMissionPlanningRequest,
    );
    widget.geofenceRequest?.removeListener(_handleGeofenceRequest);
    widget.squadPanelRequest?.removeListener(_handleSquadPanelRequest);
    widget.fleetPanelRequest?.removeListener(_handleFleetPanelRequest);
    _clearQuickDraw();
    _saveTimer?.cancel();
    _fleetFitDebounce?.cancel();
    _transientMessage?.remove();
    _transientMessage = null;
    super.dispose();
  }

  void _clearQuickDraw() {
    _quickDraft?.dispose();
    _quickDraft = null;
    _quickOwner = null;
    _quickSession = null;
    _quickRevision = null;
  }

  Future<void> _beginQuickDraw(QuickMissionShape shape) async {
    final owner = widget.connectionController.missionPlanning;
    final session = widget.connectionController.sessionId;
    await owner.initialize();
    if (!mounted ||
        widget.connectionController.sessionId != session ||
        !identical(widget.connectionController.missionPlanning, owner)) {
      return;
    }
    if (!owner.initialized || owner.busy || owner.transferActive) {
      _showTransientMessage('任务正在处理，请稍后绘制');
      return;
    }
    final document = owner.document!;
    setState(() {
      _clearQuickDraw();
      _mapActionAnchor = null;
      _mapActionPoint = null;
      _draggingEntityId = null;
      _dragPreviewWgs84 = null;
      _quickOwner = owner;
      _quickSession = session;
      _quickRevision = document.revision;
      _quickDraft = QuickMissionDraft(
        shape,
        altitudeMeters: document.defaults.altitudeMeters,
        frame: document.defaults.frame,
      );
      _rightPanel = MapRightPanel.mission;
    });
  }

  Future<void> _generateQuickDraw() async {
    final draft = _quickDraft;
    final owner = _quickOwner;
    if (draft == null ||
        owner == null ||
        !identical(widget.connectionController.missionPlanning, owner) ||
        widget.connectionController.sessionId != _quickSession) {
      throw StateError('飞机已切换，请重新开始绘制');
    }
    final points = draft.generate();
    final prefix = DateTime.now().microsecondsSinceEpoch;
    final entities = [
      for (var index = 0; index < points.length; index++)
        MissionEntity(
          id: 'quick-$prefix-$index',
          label: draft.shape.label,
          latitude: points[index].latitude,
          longitude: points[index].longitude,
          altitudeMeters: draft.altitudeMeters,
          frame: draft.frame,
        ),
    ];
    draft.frozen = true;
    try {
      await owner.insertGeneratedWaypoints(
        entities,
        expectedRevision: _quickRevision!,
        expectedSessionId: _quickSession,
      );
    } finally {
      draft.frozen = false;
    }
    if (!mounted || !identical(_quickDraft, draft)) return;
    setState(_clearQuickDraw);
    _showTransientMessage('已生成 ${entities.length} 个航点，可在任务列表中撤销');
  }

  Widget _quickDrawVisuals(QuickMissionDraft draft) => AnimatedBuilder(
    animation: draft,
    builder: (context, _) {
      final colors = context.mgColors;
      var points = <LatLng>[];
      try {
        points = draft.generate();
      } on ArgumentError {
        /* Await valid controls. */
      }
      final document = _quickOwner?.document;
      final insertion = _quickOwner?.generatedWaypointInsertionIndex ?? 0;
      final entities = document?.entities ?? const <MissionEntity>[];
      final home = document?.home ?? const MissionHome();
      final preceding = entities
          .take(insertion)
          .map((entity) => missionEntityMapPosition(entity, home))
          .whereType<LatLng>()
          .lastOrNull;
      final following = entities
          .skip(insertion)
          .map((entity) => missionEntityMapPosition(entity, home))
          .whereType<LatLng>()
          .firstOrNull;
      final stride = math.max(1, (points.length / 60).ceil());
      final camera = MapCamera.of(context);
      final labels = <int>{};
      final labelPositions = <Offset>[];
      for (
        var index = 0;
        index < points.length - (draft.closed ? 1 : 0);
        index += stride
      ) {
        final position = camera.latLngToScreenOffset(
          _toTileFrame(points[index].latitude, points[index].longitude),
        );
        if (labelPositions.every(
          (other) => (other - position).distance >= 28,
        )) {
          labels.add(index);
          labelPositions.add(position);
        }
      }
      return Stack(
        children: [
          PolylineLayer(
            polylines: [
              if (points.isNotEmpty)
                for (final connection in [
                  if (preceding != null) [preceding, points.first],
                  if (following != null) [points.last, following],
                ])
                  Polyline(
                    points: [
                      for (final point in connection)
                        _toTileFrame(point.latitude, point.longitude),
                    ],
                    color: colors.warning.withValues(alpha: 0.65),
                    strokeWidth: 2,
                    pattern: StrokePattern.dashed(segments: const [5, 5]),
                  ),
              if (points.length > 1)
                Polyline(
                  points: [
                    for (final point in points)
                      _toTileFrame(point.latitude, point.longitude),
                  ],
                  color: colors.warning,
                  strokeWidth: 2.5,
                ),
              if (draft.shape == QuickMissionShape.circle &&
                  draft.controls.length == 2)
                Polyline(
                  points: [
                    for (final point in draft.controls)
                      _toTileFrame(point.latitude, point.longitude),
                  ],
                  color: colors.textTertiary,
                  strokeWidth: 1,
                ),
            ],
          ),
          MarkerLayer(
            markers: [
              for (
                var index = 0;
                index < points.length - (draft.closed ? 1 : 0);
                index += stride
              )
                Marker(
                  point: _toTileFrame(
                    points[index].latitude,
                    points[index].longitude,
                  ),
                  width: labels.contains(index) ? 24 : 6,
                  height: labels.contains(index) ? 24 : 6,
                  child: IgnorePointer(
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: colors.surface,
                        shape: BoxShape.circle,
                        border: Border.all(color: colors.warning),
                      ),
                      child: labels.contains(index)
                          ? Center(
                              child: Text(
                                '${index + 1}',
                                style: MgTypography.sans(
                                  MgTypography.caption.copyWith(
                                    color: colors.textPrimary,
                                  ),
                                ),
                              ),
                            )
                          : null,
                    ),
                  ),
                ),
              for (var index = 0; index < draft.controls.length; index++)
                Marker(
                  point: _toTileFrame(
                    draft.controls[index].latitude,
                    draft.controls[index].longitude,
                  ),
                  width: 38,
                  height: 38,
                  child: GestureDetector(
                    behavior: HitTestBehavior.opaque,
                    onTap: () {},
                    onPanStart: (_) => FocusScope.of(context).unfocus(),
                    onPanUpdate: (details) {
                      final render = _mapKey.currentContext?.findRenderObject();
                      if (render is! RenderBox) return;
                      final point = _controller.camera.screenOffsetToLatLng(
                        render.globalToLocal(details.globalPosition),
                      );
                      draft.movePoint(index, _toWgs84(point));
                    },
                    child: Center(
                      child: Container(
                        width: 28,
                        height: 28,
                        decoration: BoxDecoration(
                          color: colors.accent,
                          shape: BoxShape.circle,
                          border: Border.all(color: colors.surface, width: 2),
                        ),
                        child: Icon(
                          index == 0 && draft.shape == QuickMissionShape.circle
                              ? Icons.add
                              : Icons.open_with,
                          color: colors.onAccent,
                          size: 17,
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ],
      );
    },
  );

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
  /// 单击地图：规划页开着就新增航点并打开编辑框；否则未解锁时弹起飞前操作，
  /// 已解锁时弹 Guided 操作。
  ///
  /// 与 MP 的右键菜单 / QGC 的 Guided Actions 一致，都是“先选位置再选动作”。
  Future<void> _handleMapTap(LatLng point, Offset anchor) async {
    // 菜单已经展开时，点地图上任何位置都只是"关掉它"，不在新位置重开——
    // 否则不选项目就永远关不掉。
    if (_suppressNextMapTap) {
      _suppressNextMapTap = false;
      return;
    }
    // 瓦片坐标（可能是 GCJ-02）换算回 WGS-84 再交给文档与飞控。
    final wgs84 = _toWgs84(point);
    final controller = widget.connectionController;
    final mission = controller.missionPlanning;
    if (_quickDraft case final draft?) {
      FocusScope.of(context).unfocus();
      draft.addPoint(wgs84);
      return;
    }

    if (_rightPanel == MapRightPanel.mission) {
      await mission.initialize();
      final home = controller.homePosition;
      if (home != null && controller.homePositionValid) {
        await mission.adoptVehicleHomeIfMissing(
          latitude: home.latitude,
          longitude: home.longitude,
          altitudeMeters: home.altitudeMeters ?? 0,
        );
      }
      if (!mounted) return;
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

    // QGC `FlyViewMap.qml`：点击地图直接展开动作列表，而不是先弹一个模态窗口。
    if (_buildMapActions(wgs84).isEmpty) {
      _closeMapActionMenu();
      _showTransientMessage('当前飞行状态没有可用的地图操作。');
      return;
    }
    setState(() {
      _mapActionAnchor = anchor;
      _mapActionPoint = wgs84;
    });
  }

  void _closeMapActionMenu() {
    if (_mapActionAnchor == null && _mapActionPoint == null) return;
    setState(() {
      _mapActionAnchor = null;
      _mapActionPoint = null;
    });
  }

  /// 把一条会改变飞机状态的指令交给顶部悬浮长按确认层。
  ///
  /// 未装配确认通道时**不下发**，只提示：缺少确认组件不能降级成"无确认直发"，
  /// 否则一旦宿主忘记传回调，地图上的飞行指令就会变成单击即执行。
  /// 返回是否已进入确认流程。
  bool _requestConfirmedCommand(String label, Future<void> Function() run) {
    final onRequest = widget.onRequestCommandConfirmation;
    if (onRequest == null) {
      _showTransientMessage('$label 需要长按确认，当前没有确认通道，命令未下发。');
      return false;
    }
    onRequest(MgCommandConfirmation(label: label, execute: run));
    return true;
  }

  /// 触点是否落在地图区域内——只有"点在地图上"才需要吃掉随后那一次 onTap。
  bool _isPointInsideMap(Offset globalPosition) {
    final box = context.findRenderObject();
    if (box is! RenderBox || !box.hasSize) return false;
    return (Offset.zero & box.size).contains(box.globalToLocal(globalPosition));
  }

  /// 把菜单收在地图范围内：贴着点击点，但不越出边缘。
  double _menuLeft(BoxConstraints constraints) {
    final anchor = _mapActionAnchor?.dx ?? 0;
    final maxLeft = constraints.maxWidth - MgMapActionMenu.width - 8;
    return anchor.clamp(8.0, maxLeft < 8 ? 8.0 : maxLeft);
  }

  double _menuTop(BoxConstraints constraints, int actionCount) {
    final anchor = _mapActionAnchor?.dy ?? 0;
    final maxTop =
        constraints.maxHeight - MgMapActionMenu.heightFor(actionCount) - 8;
    return anchor.clamp(8.0, maxTop < 8 ? 8.0 : maxTop);
  }

  /// 按 QGC `GuidedActionsController.qml` 的 `show*` 条件挑出菜单项，顺序也照抄。
  List<MgMapAction> _buildMapActions(LatLng target) {
    final controller = widget.connectionController;
    final home = controller.homePosition;
    final visibility = mapActionVisibility(
      guidedEnabled: controller.supportsGuidedMode,
      flying: controller.vehicleFlying,
      homeReady: controller.homePositionValid && home?.altitudeMeters != null,
      // QGC 的 showOrbit 要求当前不在任务中；用飞行模式近似判断。
      missionActive: controller.vehicleFlightMode.toUpperCase().contains(
        'AUTO',
      ),
      hasGpsSensor: controller.hasGpsSensor,
      supportsOrbit: controller.supportsOrbit,
      supportsRoi: controller.supportsRoi,
      supportsChangeHeading: controller.supportsChangeHeading,
    );

    void requestConfirmation(String label, Future<void> Function() run) {
      _closeMapActionMenu();
      _requestConfirmedCommand(label, run);
    }

    return <MgMapAction>[
      if (visibility.gotoLocation)
        MgMapAction(
          label: '移动到此处',
          onSelected: () =>
              requestConfirmation('移动到此处', () => _mapGotoCommand(target)),
        ),
      if (visibility.orbit)
        MgMapAction(
          label: '环绕飞行',
          onSelected: () =>
              requestConfirmation('环绕飞行', () => _mapOrbit(target)),
        ),
      if (visibility.roi)
        MgMapAction(
          label: '在此外设置兴趣点',
          onSelected: () =>
              requestConfirmation('设置兴趣点', () => _mapSetRoi(target)),
        ),
      if (visibility.setHome)
        MgMapAction(
          label: '在此设置 Home',
          onSelected: () =>
              requestConfirmation('设置 Home', () => _mapSetHome(target)),
        ),
      if (visibility.setEstimatorOrigin)
        MgMapAction(
          label: '设置估计原点',
          onSelected: () => requestConfirmation(
            '设置估计原点',
            () => _mapSetEstimatorOrigin(target),
          ),
        ),
      if (visibility.changeHeading)
        MgMapAction(
          label: '设置航向',
          onSelected: () =>
              requestConfirmation('设置航向', () => _mapSetHeading(target)),
        ),
    ];
  }

  Future<void> _runMapCommand(Future<void> Function() run) async {
    try {
      await run();
    } on Object catch (error) {
      if (!mounted) return;
      _showTransientMessage(error.toString());
    }
  }

  /// QGC 的 Go to location 只用点击点的经纬度，高度取飞机当前 **AMSL**，
  /// 也就是“保持现在的高度飞过去”。拿不到绝对高度时必须拒发：DO_REPOSITION
  /// 用的是 MAV_FRAME_GLOBAL，退化到相对高度或 0 等于命令飞机飞向地面。
  Future<void> _mapGotoCommand(LatLng target) async {
    final controller = widget.connectionController;
    final absoluteAltitude = controller.absoluteAltitudeMeters;
    if (!controller.vehiclePositionValid ||
        absoluteAltitude == null ||
        absoluteAltitude == 0) {
      throw StateError('移动到此处失败：还没有有效的海拔高度');
    }
    await controller.guidedGoto(
      target.latitude,
      target.longitude,
      absoluteAltitude,
    );
  }

  /// QGC 的 Orbit 高度是 `homePosition.altitude + 滑块高度`（AMSL）。
  Future<void> _mapOrbit(LatLng target) => _runMapCommand(() async {
    final controller = widget.connectionController;
    final absoluteAltitude = controller.absoluteAltitudeMeters;
    if (absoluteAltitude == null || absoluteAltitude == 0) {
      throw StateError('环绕飞行失败：还没有有效的海拔高度');
    }
    await controller.guidedOrbit(
      latitude: target.latitude,
      longitude: target.longitude,
      altitudeMeters: absoluteAltitude,
    );
  });

  /// ROI 的高度是**相对 Home**，地图点击没有高度信息时用 0（= Home 高度），
  /// 与 QGC `APMFirmwarePlugin::guidedModeROI` 的默认一致。
  Future<void> _mapSetRoi(LatLng target) => _runMapCommand(
    () => widget.connectionController.guidedSetRoi(
      target.latitude,
      target.longitude,
    ),
  );

  Future<void> _mapSetHome(LatLng target) => _runMapCommand(() async {
    final controller = widget.connectionController;
    await controller.setVehicleHome(
      target.latitude,
      target.longitude,
      controller.absoluteAltitudeMeters ?? 0,
    );
  });

  Future<void> _mapSetEstimatorOrigin(LatLng target) =>
      _runMapCommand(() async {
        final controller = widget.connectionController;
        await controller.setEstimatorOrigin(
          target.latitude,
          target.longitude,
          controller.absoluteAltitudeMeters ?? 0,
        );
      });

  Future<void> _mapSetHeading(LatLng target) => _runMapCommand(
    () => widget.connectionController.guidedSetHeading(
      target.latitude,
      target.longitude,
    ),
  );

  Future<void> _showGuidedAction(String actionLabel) async {
    final controller = widget.connectionController;
    if (!controller.isConnected) {
      _showTransientMessage('连接飞控后才能执行引导操作。');
      return;
    }

    // 起飞是独立流程：点击后直接填写高度并在同一个弹窗确认，
    // 不进入“引导模式选项”窗口，也不再叠加顶部的长按确认层。
    if (actionLabel.startsWith('起飞')) {
      await showTakeoffDialog(context, controller: controller);
      return;
    }

    // 返航是 QGC Guided Actions 的核心动作，但这里按 M-GCS 的统一交互走
    // 顶部状态栏长按确认，不再进入二次弹窗流程。
    if (actionLabel.startsWith('返航')) {
      _requestConfirmedCommand('返航', controller.guidedReturnToLaunch);
      return;
    }

    // 降落同样直接交给顶部悬浮长按确认，避免再进入引导模式二次弹窗。
    if (actionLabel.startsWith('降落')) {
      _requestConfirmedCommand('降落', controller.guidedLand);
      return;
    }

    final fix = controller.vehiclePosition;
    final position = fix == null
        ? _center
        : LatLng(fix.latitude, fix.longitude);
    if (!mounted) return;
    await showGuidedActionsDialog(
      context,
      controller: controller,
      position: position,
      initialActionLabel: actionLabel,
    );
  }

  LatLng _toTileFrame(double latitude, double longitude) {
    if (!_sourceFor(_style, customTileUrl: _customTileUrl).gcj02) {
      return LatLng(latitude, longitude);
    }
    final (lat, lon) = GcjCoordinates.wgs84ToGcj02(latitude, longitude);
    return LatLng(lat, lon);
  }

  /// 反向：把瓦片坐标系里的点还原成 WGS-84。
  ///
  /// 相机中心始终按 WGS-84 保存，这样切换地图源、重启后恢复视图都不会平移。
  LatLng _toWgs84(LatLng point) {
    if (!_sourceFor(_style, customTileUrl: _customTileUrl).gcj02) return point;
    final (lat, lon) = GcjCoordinates.gcj02ToWgs84(
      point.latitude,
      point.longitude,
    );
    return LatLng(lat, lon);
  }

  ///
  /// 数据来自后端缓存的 `HOME_POSITION` / `GLOBAL_POSITION_INT`，两个都是
  /// `valid` 才画——定位还没锁定时不画一个假的点在地图中心。
  /// go to location 的盘旋圆（仅固定翼/已转固定翼的 VTOL）。
  ///
  /// 半径取 `|WP_LOITER_RAD|`，方向取该参数符号（正=顺时针，负=逆时针），
  /// 对应 ArduPilot `ArduPlane/Parameters.cpp` 里 `WP_LOITER_RAD` 的定义。
  /// 多旋翼的 go to location 是平移过去悬停，没有盘旋圆，所以这里返回空。
  List<CircleMarker<Object>> _buildLoiterCircles(BuildContext context) {
    final connection = widget.connectionController;
    final target = connection.guidedTargetPosition;
    final radius = connection.loiterRadiusMeters;
    if (!connection.hasActiveGuidedTarget ||
        target == null ||
        radius == null ||
        !connection.inForwardFlight) {
      return const [];
    }
    final colors = context.mgColors;
    return [
      CircleMarker<Object>(
        point: _toTileFrame(target.latitude, target.longitude),
        radius: radius,
        useRadiusInMeter: true,
        color: colors.warning.withValues(alpha: 0.10),
        borderColor: colors.warning.withValues(alpha: 0.85),
        borderStrokeWidth: 2,
      ),
    ];
  }

  VehicleMarkerKind _fleetVehicleMarkerKind(String vehicleType) {
    return vehicleMarkerKindFor(switch (vehicleType.toLowerCase()) {
      'copter' || 'quadrotor' || 'multirotor' => 'multirotor',
      'fixedwing' || 'fixed wing' || 'vtol' => 'fixed_wing',
      _ => 'unknown',
    });
  }

  List<Marker> _buildActiveMarkers(BuildContext context, MgColors colors) {
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
    // 地图点击菜单选中的那个点：跟着菜单一起出现/消失。
    final menuPoint = _mapActionPoint;
    if (menuPoint != null) {
      markers.add(
        Marker(
          point: _toTileFrame(menuPoint.latitude, menuPoint.longitude),
          width: 30,
          height: 30,
          child: _MapMarker(
            icon: CupertinoIcons.scope,
            fill: colors.surface,
            border: colors.accent,
            iconColor: colors.accent,
          ),
        ),
      );
    }
    final guidedTarget = connection.guidedTargetPosition;
    if (connection.hasActiveGuidedTarget && guidedTarget != null) {
      markers.add(
        Marker(
          point: _toTileFrame(guidedTarget.latitude, guidedTarget.longitude),
          width: 34,
          height: 34,
          child: _MapMarker(
            icon: CupertinoIcons.placemark,
            fill: colors.surface,
            border: colors.warning,
            iconColor: colors.warning,
          ),
        ),
      );
    }
    // 盘旋方向指示：箭头嵌在圆周线上，上下各一个，沿切线指向绕行方向。
    // 顺时针时正北点切线朝东（右）、正南点切线朝西（左）；逆时针反之。
    final loiterRadius = connection.loiterRadiusMeters;
    if (connection.hasActiveGuidedTarget &&
        guidedTarget != null &&
        loiterRadius != null &&
        connection.inForwardFlight) {
      // 纬度 1 度约 111.32 km，用来把半径换算成南北向偏移——只是把箭头放到圆周上，
      // 圆本身仍按米半径绘制。
      const metersPerDegreeLatitude = 111320.0;
      final offset = loiterRadius / metersPerDegreeLatitude;
      final clockwise = connection.loiterClockwise;
      final arrows = <(double, double)>[
        (guidedTarget.latitude + offset, clockwise ? 0.0 : math.pi),
        (guidedTarget.latitude - offset, clockwise ? math.pi : 0.0),
      ];
      for (final arrow in arrows) {
        markers.add(
          Marker(
            point: _toTileFrame(arrow.$1, guidedTarget.longitude),
            width: 16,
            height: 12,
            child: _LoiterArrowHead(angle: arrow.$2, color: colors.warning),
          ),
        );
      }
    }
    final fix = connection.vehiclePosition;
    if (connection.vehiclePositionValid && fix != null) {
      markers.add(
        Marker(
          point: _toTileFrame(fix.latitude, fix.longitude),
          width: 46,
          height: 46,
          // 按机型画机体剪影（多旋翼/固定翼/VTOL），不再用统一的纸飞机图标。
          child: VehicleMarkerIcon(
            vehicleId: connection.fleetSnapshot
                .vehicle(connection.sessionId ?? -1)
                ?.systemId,
            kind: vehicleMarkerKindFor(
              connection.firmwareInfo?['vehicle_class']?.toString() ??
                  'unknown',
            ),
            fill: colors.accent,
            outline: colors.surface,
            headingDegrees: fix.headingDegrees,
          ),
        ),
      );
    }
    return markers;
  }

  List<Marker> _buildFleetMarkers(BuildContext context, MgColors colors) {
    final connection = widget.connectionController;
    final markers = <Marker>[];
    // Position and heading use the same polling cadence as the active aircraft.
    for (final vehicle in connection.fleetSnapshot.vehicles) {
      if (vehicle.id == connection.sessionId) continue;
      final details = <String>[];
      if (vehicle.flightMode.isNotEmpty) details.add(vehicle.flightMode);
      if (vehicle.armed) details.add('已解锁');
      final battery = vehicle.batteryRemainingPercent;
      if (battery != null && battery >= 0) details.add('$battery%');
      if (vehicle.missionValid && vehicle.missionTotal > 0) {
        details.add(
          '任务 ${vehicle.missionCurrentIndex}/${vehicle.missionTotal}',
        );
      }
      final label = [
        fleetVehicleLabel(vehicle, connection.fleetSnapshot.vehicles),
        if (details.isNotEmpty) details.join(' · '),
      ].join(' · ');
      if (vehicle.homePositionValid) {
        markers.add(
          Marker(
            point: _toTileFrame(vehicle.homeLatitude, vehicle.homeLongitude),
            width: 24,
            height: 24,
            child: Tooltip(
              message: '$label · Home',
              child: _MapMarker(
                icon: CupertinoIcons.house_fill,
                fill: colors.surface,
                border: colors.success,
                iconColor: colors.success,
              ),
            ),
          ),
        );
      }
      final positions = connection.fleetPositions.value;
      final fix = positions.containsKey(vehicle.id)
          ? positions[vehicle.id]
          : vehicle.positionValid
          ? VehicleFix(
              latitude: vehicle.latitude,
              longitude: vehicle.longitude,
              headingDegrees: vehicle.headingDegrees,
            )
          : null;
      if (fix == null) continue;
      markers.add(
        Marker(
          point: _toTileFrame(fix.latitude, fix.longitude),
          width: 46,
          height: 46,
          child: Tooltip(
            message: label,
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: () => connection.selectVehicle(vehicle.id),
              child: VehicleMarkerIcon(
                vehicleId: vehicle.systemId,
                kind: _fleetVehicleMarkerKind(vehicle.vehicleType),
                fill: colors.textSecondary,
                outline: colors.surface,
                headingDegrees: fix.headingDegrees,
              ),
            ),
          ),
        ),
      );
    }
    return markers;
  }

  List<Marker> _buildMissionMarkers(BuildContext context, MgColors colors) {
    final connection = widget.connectionController;
    final markers = <Marker>[];
    // 任务文档里的航点：画成编号标记，点击打开编辑框。
    final mission = connection.missionPlanning;
    final missionHome = mission.document?.home ?? const MissionHome();
    final geoFence = mission.document?.geoFence;
    final breachReturn = geoFence?.breachReturn;
    if (breachReturn?.latitude != null && breachReturn?.longitude != null) {
      markers.add(
        Marker(
          point: _toTileFrame(breachReturn!.latitude!, breachReturn.longitude!),
          width: 28,
          height: 28,
          child: Tooltip(
            message: '围栏越界返航点',
            child: _MapMarker(
              icon: CupertinoIcons.scope,
              fill: colors.surface,
              border: colors.danger,
              iconColor: colors.danger,
            ),
          ),
        ),
      );
    }
    for (
      var index = 0;
      index < (mission.document?.rallyPoints.length ?? 0);
      index++
    ) {
      final rally = mission.document!.rallyPoints[index];
      final latitude = rally.latitude;
      final longitude = rally.longitude;
      if (latitude == null || longitude == null) continue;
      markers.add(
        Marker(
          point: _toTileFrame(latitude, longitude),
          width: 26,
          height: 26,
          child: Tooltip(
            message: 'Rally 点 ${index + 1}',
            child: _MapMarker(
              icon: CupertinoIcons.flag,
              fill: colors.surface,
              border: colors.warning,
              iconColor: colors.warning,
            ),
          ),
        ),
      );
    }
    final entities = mission.document?.entities ?? const <MissionEntity>[];
    final dragEnabled = !mission.busy && !mission.transferActive;
    for (var index = 0; index < entities.length; index++) {
      final entity = entities[index];
      // 返航只绘制通往 Home 的航段，复用原有 Home 图标。
      if (entity.isReturnToLaunch) continue;
      final dragging = entity.id == _draggingEntityId;
      final preview = dragging ? _dragPreviewWgs84 : null;
      final position = missionEntityMapPosition(
        entity,
        missionHome,
        preview: preview,
      );
      if (position == null) continue;
      final entityDragEnabled = dragEnabled && !entity.isTakeoff;
      final vehicleHome = connection.homePosition;
      final overlapsHome =
          entity.isTakeoff &&
          connection.homePositionValid &&
          vehicleHome != null &&
          wgs84DistanceMeters(
                position,
                LatLng(vehicleHome.latitude, vehicleHome.longitude),
              ) <
              1;
      markers.add(
        Marker(
          point: _toTileFrame(position.latitude, position.longitude),
          // When T and Home share a coordinate, offset only the badge in
          // screen space so both remain visible at the same geographic point.
          alignment: overlapsHome ? Alignment.bottomLeft : Alignment.center,
          width: 44,
          height: 44,
          child: MouseRegion(
            cursor: !entityDragEnabled
                ? SystemMouseCursors.basic
                : dragging
                ? SystemMouseCursors.grabbing
                : SystemMouseCursors.move,
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: () => mission.selectEntity(index),
              onDoubleTap: () => unawaited(_editEntity(mission, entity)),
              onPanStart: entityDragEnabled
                  ? (_) => _beginEntityDrag(index, entity)
                  : null,
              onPanUpdate: entityDragEnabled
                  ? (details) => _updateEntityDrag(details.globalPosition)
                  : null,
              onPanEnd: entityDragEnabled
                  ? (_) => unawaited(_finishEntityDrag(mission, entity))
                  : null,
              onPanCancel: entityDragEnabled ? _cancelEntityDrag : null,
              child: Center(
                // Keep the visible circle small; the surrounding area also accepts drags.
                child: SizedBox.square(
                  dimension: 26,
                  child: _WaypointMarker(
                    label: entity.isTakeoff ? 'T' : '${index + 1}',
                    color: entity.isTakeoff ? colors.warning : colors.accent,
                    selected: mission.selectedEntityIndex == index,
                    dragging: dragging,
                  ),
                ),
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
    final document = mission.document;
    final entities = document?.entities ?? const <MissionEntity>[];
    final home = document?.home ?? const MissionHome();
    final vertices = <_MissionRouteVertex>[];
    var takeoffHandled = false;
    for (final entity in entities) {
      if (!entity.enabled) continue;
      final position = missionEntityMapPosition(
        entity,
        home,
        preview: entity.id == _draggingEntityId ? _dragPreviewWgs84 : null,
      );
      if (entity.isTakeoff) {
        if (!takeoffHandled && position != null) {
          takeoffHandled = true;
          vertices.add(
            _MissionRouteVertex(
              wgs84: position,
              tileFrame: _toTileFrame(position.latitude, position.longitude),
            ),
          );
        }
        // The planning line uses the same displayed reference as T. This does
        // not change how the flight controller executes the takeoff command.
        continue;
      }
      if (position == null) continue;
      vertices.add(
        _MissionRouteVertex(
          wgs84: position,
          tileFrame: _toTileFrame(position.latitude, position.longitude),
          returning: entity.isReturnToLaunch,
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
      for (var i = 1; i < vertices.length; i++)
        Polyline(
          points: [vertices[i - 1].tileFrame, vertices[i].tileFrame],
          color: (vertices[i].returning ? colors.warning : colors.accent)
              .withValues(alpha: 0.82),
          strokeWidth: 2.2,
          pattern: StrokePattern.dashed(segments: const [8, 6]),
        ),
    ];
  }

  List<Polygon> _buildMissionFencePolygons(BuildContext context) {
    final document = widget.connectionController.missionPlanning.document;
    final fence = document?.geoFence;
    if (fence == null || fence.polygons.isEmpty) return const [];
    final colors = context.mgColors;
    final polygons = <Polygon>[];
    for (final polygon in fence.polygons) {
      final points = <LatLng>[];
      for (final vertex in polygon.vertices) {
        final latitude = vertex.latitude;
        final longitude = vertex.longitude;
        if (latitude == null || longitude == null) continue;
        points.add(_toTileFrame(latitude, longitude));
      }
      if (points.length < 3) continue;
      final color = polygon.inclusion ? colors.accent : colors.danger;
      polygons.add(
        Polygon(
          points: points,
          color: color.withValues(alpha: 0.08),
          borderColor: color.withValues(alpha: 0.68),
          borderStrokeWidth: 1.8,
        ),
      );
    }
    return polygons;
  }

  List<CircleMarker<Object>> _buildMissionFenceCircles(BuildContext context) {
    final document = widget.connectionController.missionPlanning.document;
    final fence = document?.geoFence;
    if (fence == null || fence.circles.isEmpty) return const [];
    final colors = context.mgColors;
    final circles = <CircleMarker<Object>>[];
    for (final circle in fence.circles) {
      final latitude = circle.latitude;
      final longitude = circle.longitude;
      if (latitude == null || longitude == null || circle.radiusMeters <= 0) {
        continue;
      }
      final color = circle.inclusion ? colors.accent : colors.danger;
      circles.add(
        CircleMarker<Object>(
          point: _toTileFrame(latitude, longitude),
          radius: circle.radiusMeters,
          useRadiusInMeter: true,
          color: color.withValues(alpha: 0.08),
          borderColor: color.withValues(alpha: 0.68),
          borderStrokeWidth: 1.8,
        ),
      );
    }
    return circles;
  }

  /// 非 active 飞机的任务路径使用 Fleet 低频快照绘制，颜色弱化，
  /// 不参与 active 飞机的地图编辑和拖拽。
  List<Polyline> _buildFleetMissionRouteLines(BuildContext context) {
    final connection = widget.connectionController;
    final colors = context.mgColors;
    final lines = <Polyline>[];
    for (final vehicle in connection.fleetSnapshot.vehicles) {
      if (vehicle.id == connection.sessionId ||
          !vehicle.missionValid ||
          vehicle.missionPoints.length < 2) {
        continue;
      }
      lines.add(
        Polyline(
          points: [
            for (final point in vehicle.missionPoints)
              _toTileFrame(point.latitude, point.longitude),
          ],
          color: colors.textSecondary.withValues(alpha: 0.42),
          strokeWidth: 1.5,
          pattern: StrokePattern.dashed(segments: const [6, 6]),
        ),
      );
    }
    return lines;
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
  Widget build(BuildContext context) => LayoutBuilder(builder: _buildMap);

  Widget _buildMap(BuildContext context, BoxConstraints constraints) {
    final source = _sourceFor(_style, customTileUrl: _customTileUrl);
    final screenWidth = constraints.maxWidth;
    final compactHeight = constraints.maxHeight < 450;
    final quickDrawing =
        _rightPanel == MapRightPanel.mission && _quickDraft != null;
    final quickPanelWidth =
        constraints.maxHeight -
                kMapRightPanelTopInset -
                (compactHeight ? 90 : 226) <
            300
        ? 680.0
        : 460.0;
    final primaryWidth = math.min(
      quickDrawing ? quickPanelWidth : mapRightPanelWidth(_rightPanel),
      math.max(0.0, screenWidth - 36),
    );
    final instrumentSize = compactHeight
        ? (screenWidth < 700 ? 104.0 : 128.0)
        : MgMetrics.attitudeCompassSize;
    final fleetRight = quickDrawing
        ? kMapPanelEdgeInset + primaryWidth + kMapPanelGap
        : fleetPanelRightInset(_rightPanel);
    final showFleetPanel =
        _fleetPanelVisible &&
        widget.connectionController.fleetSnapshot.vehicles.length >= 2 &&
        screenWidth >=
            fleetRight + MgMetrics.fleetPanelWidth + kMapPanelEdgeInset + 280;
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
            onTap: (tapPosition, point) => unawaited(
              _handleMapTap(point, tapPosition.relative ?? Offset.zero),
            ),
            onPositionChanged: (camera, hasGesture) {
              // 拖动/缩放地图时收起点击菜单，和 QGC 的行为一致。
              if (hasGesture) {
                _suppressNextMapTap = false;
                _closeMapActionMenu();
              }
              _scheduleSave(camera.center, camera.zoom);
            },
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
            // Keep slow-changing mission and fleet geometry independent from
            // the 20 Hz active-vehicle position stream.
            AnimatedBuilder(
              animation: _fleetMapSources,
              builder: (context, _) => RepaintBoundary(
                child: PolylineLayer(
                  polylines: _buildFleetMissionRouteLines(context),
                ),
              ),
            ),
            AnimatedBuilder(
              animation: _missionMapSources,
              builder: (context, _) => RepaintBoundary(
                child: PolylineLayer(
                  polylines: _buildMissionRouteLines(context),
                ),
              ),
            ),
            AnimatedBuilder(
              animation: _activeMapSources,
              builder: (context, _) => RepaintBoundary(
                child: PolylineLayer(polylines: _buildHomeLine(context)),
              ),
            ),
            AnimatedBuilder(
              animation: _missionMapSources,
              builder: (context, _) => RepaintBoundary(
                child: PolygonLayer(
                  polygons: _buildMissionFencePolygons(context),
                ),
              ),
            ),
            AnimatedBuilder(
              animation: _activeMapSources,
              builder: (context, _) => RepaintBoundary(
                child: CircleLayer(circles: _buildLoiterCircles(context)),
              ),
            ),
            AnimatedBuilder(
              animation: _missionMapSources,
              builder: (context, _) => RepaintBoundary(
                child: CircleLayer(circles: _buildMissionFenceCircles(context)),
              ),
            ),
            AnimatedBuilder(
              animation: _fleetMarkerSources,
              builder: (context, _) => RepaintBoundary(
                child: MarkerLayer(
                  markers: _buildFleetMarkers(context, context.mgColors),
                ),
              ),
            ),
            AnimatedBuilder(
              animation: _missionMapSources,
              builder: (context, _) => RepaintBoundary(
                child: MarkerLayer(
                  markers: [
                    ..._buildMissionDistanceLabels(context),
                    ..._buildMissionMarkers(context, context.mgColors),
                  ],
                ),
              ),
            ),
            AnimatedBuilder(
              animation: _activeMapSources,
              builder: (context, _) => RepaintBoundary(
                child: MarkerLayer(
                  markers: [
                    ..._buildHomeDistanceLabel(context),
                    ..._buildActiveMarkers(context, context.mgColors),
                  ],
                ),
              ),
            ),
            if (_quickDraft case final draft?) _quickDrawVisuals(draft),
          ],
        ),
        // 右下角组合仪表：只保留 v0.0.5 MacGlass 的指南针 + 姿态球本体。
        Positioned(
          right: compactHeight && primaryWidth > 0 ? primaryWidth + 30 : 18,
          bottom: compactHeight ? 18 + MgMetrics.dashboardHeight + 12 : 18,
          child: SizedBox.square(
            dimension: instrumentSize,
            child: FittedBox(
              child: AnimatedBuilder(
                animation: _attitudeSources,
                builder: (context, _) => RepaintBoundary(
                  child: MgMacGlassAttitudeCompass(
                    headingDegrees:
                        widget.connectionController.headingDegrees ?? 0,
                    rollDegrees: widget.connectionController.rollDegrees ?? 0,
                    pitchDegrees: widget.connectionController.pitchDegrees ?? 0,
                  ),
                ),
              ),
            ),
          ),
        ),
        Positioned(
          left: 82,
          right: compactHeight ? 18 : 18 + MgMetrics.attitudeCompassSize + 12,
          bottom: 18,
          child: Center(
            child: AnimatedBuilder(
              animation: _dashboardSources,
              builder: (context, _) => SingleChildScrollView(
                scrollDirection: Axis.horizontal,
                child: MgTelemetryDashboard(
                  data: MgTelemetryDashboardData.fromController(
                    widget.connectionController,
                  ),
                ),
              ),
            ),
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
            followVehicle: _followVehicle,
            onToggleFollowVehicle: _toggleFollowVehicle,
          ),
        ),
        Positioned(
          left: 18,
          top: 18,
          child: AnimatedBuilder(
            animation: _vehicleStatusSources,
            builder: (context, _) => _FlightActionButtons(
              controller: widget.connectionController,
              onAction: (label) => unawaited(_showGuidedAction(label)),
              onChangeAltitude: () => unawaited(
                showChangeAltitudeDialog(
                  context,
                  controller: widget.connectionController,
                  onRequestCommandConfirmation:
                      widget.onRequestCommandConfirmation,
                ),
              ),
            ),
          ),
        ),
        if (showFleetPanel)
          MapPanelSlot(
            preferredWidth: MgMetrics.fleetPanelWidth,
            rightInset: fleetRight,
            reserveInstruments: true,
            child: MgFleetBatchPanel(
              fleet: widget.connectionController.fleet,
              squads: widget.connectionController.squadsOrNull,
              onSelectVehicle:
                  widget.onSelectFleetVehicle ??
                  (vehicleId) async =>
                      widget.connectionController.selectVehicle(vehicleId),
              onRequestAction: widget.onRequestFleetBatchAction ?? (_) {},
              onOpenFlightMode: widget.onOpenFleetFlightMode,
              onClose: () => setState(() => _fleetPanelVisible = false),
            ),
          ),
        if (_rightPanel == MapRightPanel.connection)
          MapPanelSlot(
            preferredWidth: kConnectionPanelMaxWidth,
            reserveInstruments: true,
            child: ConnectionFloatingPanel(
              controller: widget.connectionController,
              onClose: () => setState(() => _rightPanel = MapRightPanel.none),
            ),
          ),
        if (_rightPanel == MapRightPanel.mission && _quickDraft != null)
          MapPanelSlot(
            preferredWidth: quickPanelWidth,
            reserveInstruments: true,
            child: QuickMissionPanel(
              key: ObjectKey(_quickDraft),
              draft: _quickDraft!,
              onGenerate: _generateQuickDraw,
              onCancel: () => setState(_clearQuickDraw),
            ),
          ),
        if (_rightPanel == MapRightPanel.mission && _quickDraft == null)
          MapPlanningPanelSlot(
            child: MissionPlanningFloatingPanel(
              key: ValueKey(widget.connectionController.sessionId),
              controller: widget.connectionController,
              onRequestCommandConfirmation: widget.onRequestCommandConfirmation,
              onQuickDraw: _beginQuickDraw,
              onClose: () => setState(() => _rightPanel = MapRightPanel.none),
            ),
          ),
        if (_rightPanel == MapRightPanel.geofence)
          MapPlanningPanelSlot(
            preferredWidth: kGeofencePanelWidth,
            child: GeofenceFloatingPanel(
              key: ValueKey(widget.connectionController.sessionId),
              controller: widget.connectionController,
              onClose: () => setState(() => _rightPanel = MapRightPanel.none),
            ),
          ),
        if (_rightPanel == MapRightPanel.squads)
          MapPlanningPanelSlot(
            preferredWidth: kSquadPanelWidth,
            child: SquadFloatingPanel(
              squads: widget.connectionController.squadsOrNull,
              fleet: widget.connectionController.fleet,
              onRequestAction: widget.onRequestFleetBatchAction ?? (_) {},
              onClose: () => setState(() => _rightPanel = MapRightPanel.none),
            ),
          ),
        if (_rightPanel == MapRightPanel.messages)
          // 右上角与连接面板共用同一个槽位，避免互相覆盖。
          MapPanelSlot(
            preferredWidth: kFlightMessagePanelWidth,
            preferredHeight: kMapMessagePanelHeight,
            reserveInstruments: true,
            child: FlightMessageFloatingPanel(
              controller: widget.connectionController,
              onClose: () => setState(() => _rightPanel = MapRightPanel.none),
            ),
          ),
        if (_mapActionPoint != null && _mapActionAnchor != null)
          Positioned.fill(
            // 只有菜单本体拦截点击，空白处仍然落到地图上。
            child: LayoutBuilder(
              builder: (context, constraints) {
                final target = _mapActionPoint!;
                final actions = _buildMapActions(target);
                if (actions.isEmpty) return const SizedBox.shrink();
                return Stack(
                  children: [
                    Positioned(
                      left: _menuLeft(constraints),
                      top: _menuTop(constraints, actions.length),
                      // TapRegion 覆盖全app：点地图、点面板、点顶部栏都会关闭，
                      // 点菜单内部不会。
                      child: TapRegion(
                        onTapOutside: (event) {
                          if (_mapActionPoint == null) return;
                          _suppressNextMapTap = _isPointInsideMap(
                            event.position,
                          );
                          _closeMapActionMenu();
                        },
                        child: MgMapActionMenu(
                          latitude: target.latitude,
                          longitude: target.longitude,
                          actions: actions,
                        ),
                      ),
                    ),
                  ],
                );
              },
            ),
          ),
      ],
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
  const _MissionRouteVertex({
    required this.wgs84,
    required this.tileFrame,
    this.returning = false,
  });

  final LatLng wgs84;
  final LatLng tileFrame;
  final bool returning;
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
    required this.followVehicle,
    required this.onToggleFollowVehicle,
  });

  final VoidCallback onZoomIn;
  final VoidCallback onZoomOut;
  final VoidCallback onReset;
  final bool followVehicle;
  final VoidCallback onToggleFollowVehicle;

  @override
  Widget build(BuildContext context) => MgFloatingPanel(
    borderRadius: BorderRadius.circular(9),
    child: Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        _MapIconButton(icon: Icons.add, tooltip: '放大', onPressed: onZoomIn),
        _MapIconButton(icon: Icons.remove, tooltip: '缩小', onPressed: onZoomOut),
        _MapIconButton(
          icon: Icons.my_location,
          tooltip: '定位到飞机',
          onPressed: onReset,
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 9),
          child: Container(height: 1, color: MgColors.of(context).divider),
        ),
        _MapIconButton(
          icon: CupertinoIcons.location_north_fill,
          tooltip: followVehicle ? '停止视角跟随' : '视角跟随',
          onPressed: onToggleFollowVehicle,
          active: followVehicle,
        ),
      ],
    ),
  );
}

/// 遥测位置更新时是否需要把视角移到飞机。
///
/// - 视角跟随开启：始终跟随；
/// - 尚未做过首次定位：做一次首次居中（是否真的移动还要看距离阈值）；
/// - 其余情况：不动，保留用户手动拖动的地图。
@visibleForTesting
bool shouldMoveCameraToVehicle({
  required bool followVehicle,
  required bool centeredOnce,
}) => followVehicle || !centeredOnce;

/// QGC `GuidedActionsController.qml` 的 Takeoff / Land / RTL 显示规则。
///
/// 未飞行只显示 Takeoff；飞行中且后端声明支持引导降落才显示 Land；飞行中显示 RTL。
/// Takeoff 与 Land 互斥，RTL 独立。
@visibleForTesting
({bool takeoff, bool land, bool rtl, bool changeAltitude})
flightActionVisibility({required bool flying, required bool landCapability}) =>
    (
      takeoff: !flying,
      land: flying && landCapability,
      rtl: flying,
      // 改变高度只在空中才有意义，和降落/返航同时出现，排在它们下面。
      changeAltitude: flying,
    );

/// 独立的飞行动作区：尺寸明显大于地图工具按钮，避免把危险动作藏在小图标里。
class _FlightActionButtons extends StatelessWidget {
  const _FlightActionButtons({
    required this.controller,
    required this.onAction,
    required this.onChangeAltitude,
  });

  final ConnectionController controller;
  final ValueChanged<String> onAction;

  /// “改变高度”是独立入口，不走引导操作菜单。
  final VoidCallback onChangeAltitude;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    final visibility = flightActionVisibility(
      flying: controller.vehicleStateFresh && controller.vehicleFlying,
      landCapability: controller.canLandFromGuided,
    );
    final guidedReady =
        controller.vehicleStateFresh &&
        controller.vehicleArmed &&
        controller.supportsGuidedMode;
    final buttons = <Widget>[
      if (visibility.takeoff)
        _FlightActionButton(
          icon: CupertinoIcons.arrow_up_circle_fill,
          label: '起飞',
          color: colors.success,
          enabled:
              controller.vehicleStateFresh && controller.supportsGuidedTakeoff,
          onPressed: () => onAction('起飞'),
        ),
      if (visibility.land)
        _FlightActionButton(
          icon: CupertinoIcons.arrow_down_circle_fill,
          label: '降落',
          color: colors.warning,
          enabled: guidedReady,
          onPressed: () => onAction('降落'),
        ),
      if (visibility.rtl)
        _FlightActionButton(
          icon: CupertinoIcons.house_fill,
          label: '返航',
          color: colors.accent,
          enabled: guidedReady,
          onPressed: () => onAction('返航 (RTL)'),
        ),
      // 改变高度：和降落/返航并列的独立入口，不放进引导操作菜单。
      if (visibility.changeAltitude)
        _FlightActionButton(
          icon: Icons.height,
          label: '高度',
          color: colors.accent,
          enabled: guidedReady,
          onPressed: onChangeAltitude,
        ),
    ];
    return MgFloatingPanel(
      borderRadius: BorderRadius.circular(9),
      child: Padding(
        padding: const EdgeInsets.all(6),
        // 竖排：起飞/降落/返航从上往下排，和 QGC 工具栏的图标列一致。
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (var index = 0; index < buttons.length; index++) ...[
              if (index > 0) const SizedBox(height: 6),
              buttons[index],
            ],
          ],
        ),
      ),
    );
  }
}

class _FlightActionButton extends StatelessWidget {
  const _FlightActionButton({
    required this.icon,
    required this.label,
    required this.color,
    required this.enabled,
    required this.onPressed,
  });

  final IconData icon;
  final String label;
  final Color color;
  final bool enabled;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return MacosTooltip(
      message: label,
      child: Material(
        color: colors.transparent,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        child: InkWell(
          onTap: enabled ? onPressed : null,
          borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          hoverColor: colors.mapControlHover,
          child: SizedBox(
            width: 62,
            height: 56,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  icon,
                  size: 27,
                  color: enabled ? color : colors.textTertiary,
                ),
                const SizedBox(height: 2),
                Text(
                  label,
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(
                      color: enabled ? colors.textPrimary : colors.textTertiary,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _MapIconButton extends StatelessWidget {
  const _MapIconButton({
    required this.icon,
    required this.tooltip,
    required this.onPressed,
    this.active = false,
  });

  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;
  final bool active;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return MacosTooltip(
      message: tooltip,
      child: MacosIconButton(
        icon: Icon(
          icon,
          size: 22,
          color: active ? colors.accent : colors.mapControlIcon,
        ),
        semanticLabel: tooltip,
        boxConstraints: const BoxConstraints.tightFor(width: 46, height: 46),
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
    required this.label,
    required this.color,
    this.selected = false,
    this.dragging = false,
  });

  final String label;
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
        label,
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(color: context.mgColors.onAccent),
      ),
    ),
  );
}

/// 盘旋圆上的方向箭头：实心箭头，按切线方向旋转后贴在圆周线上。
class _LoiterArrowHead extends StatelessWidget {
  const _LoiterArrowHead({required this.angle, required this.color});

  /// 切线方向（弧度），0 = 指向东（右）。
  final double angle;
  final Color color;

  @override
  Widget build(BuildContext context) => Transform.rotate(
    angle: angle,
    child: CustomPaint(
      size: const Size(16, 12),
      painter: _ArrowHeadPainter(color: color),
    ),
  );
}

class _ArrowHeadPainter extends CustomPainter {
  const _ArrowHeadPainter({required this.color});

  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    // 指向 +x 的实心箭头（尾部内凹，视觉上像嵌在线端）。
    final path = ui.Path()
      ..moveTo(size.width, size.height / 2)
      ..lineTo(0, 0)
      ..lineTo(size.width * 0.32, size.height / 2)
      ..lineTo(0, size.height)
      ..close();
    canvas.drawPath(path, Paint()..color = color);
  }

  @override
  bool shouldRepaint(_ArrowHeadPainter old) => old.color != color;
}
