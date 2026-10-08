import 'fleet_models.dart';

/// 连接与 UI 焦点分离的 native seam。
///
/// 参考 QGC MultiVehicleManager 和 MP ConnectionControl/MAVList。
/// open 不改变焦点；close 精确关闭 session；读取列表不能临时切换 active。
/// 实际 FFI 与测试用内存适配器使用同一接口，不依赖 Flutter 页面。
abstract interface class FleetBackend {
  int open(FleetConnectionSpec connection);
  void close(int linkId);
  void closeRoute(int routeId);
  void select(int vehicleId);
  FleetSnapshot snapshot();
}
