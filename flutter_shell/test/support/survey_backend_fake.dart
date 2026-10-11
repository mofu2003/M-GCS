import 'package:mgcs_frontend/survey/survey_backend.dart';
import 'package:mgcs_frontend/survey/survey_models.dart';

/// `mg_survey_generate*` 的替身后端：不接触 `dart:ffi`，可注入成功结果、
/// `valid:false` 结果或异常，供控制器与页面测试驱动「生成 → 预览 → 并入」。
class FakeSurveyBackend implements SurveyBackend {
  SurveyResult gridResult = surveyOk(waypoints: 2);
  SurveyResult corridorResult = surveyOk(waypoints: 3, lines: 1);

  /// 非空时生成直接抛出，模拟 FFI 层异常。
  Object? error;

  SurveyGridConfig? lastGrid;
  SurveyCorridorConfig? lastCorridor;

  @override
  Future<SurveyResult> generateGrid(SurveyGridConfig config) async {
    lastGrid = config;
    final failure = error;
    if (failure != null) throw failure;
    return gridResult;
  }

  @override
  Future<SurveyResult> generateCorridor(SurveyCorridorConfig config) async {
    lastCorridor = config;
    final failure = error;
    if (failure != null) throw failure;
    return corridorResult;
  }
}

/// 一条成功结果。默认 2 个 `NAV_WAYPOINT`（command 16），坐标与高度固定，
/// 便于断言并入任务后的实体字段。
SurveyResult surveyOk({int waypoints = 2, int lines = 2}) => SurveyResult(
  valid: true,
  waypointCount: waypoints,
  flightLineCount: lines,
  totalDistanceM: 1200,
  estimatedTimeS: 300,
  areaM2: 40000,
  photoCount: waypoints,
  waypoints: [
    for (var i = 0; i < waypoints; i++)
      SurveyWaypoint(
        seq: i,
        command: 16,
        frame: 3,
        autocontinue: 1,
        params: const [0, 0, 0, 0],
        latitude: 31.0 + i * 0.001,
        longitude: 121.0 + i * 0.001,
        altitudeMeters: 80,
      ),
  ],
);
