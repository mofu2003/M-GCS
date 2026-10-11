import '../mission/mission_document_controller.dart';
import '../mission/mission_document_models.dart';
import 'survey_backend.dart';
import 'survey_models.dart';

/// 测绘规划控制器：把配置交给 native 生成航线，并把结果并入任务文档。
///
/// 生成是本地纯计算，不要求连接飞控；并入任务走 [MissionDocumentController]
/// 的 `replace` 操作，是单个可撤销步骤。上传仍由任务面板负责，本控制器不假设
/// 生成即上载成功。
final class SurveyController {
  SurveyController({required this.backend});

  final SurveyBackend backend;

  Future<SurveyResult> generateGrid(SurveyGridConfig config) =>
      backend.generateGrid(config);

  Future<SurveyResult> generateCorridor(SurveyCorridorConfig config) =>
      backend.generateCorridor(config);

  /// 把生成结果转成任务文档实体。`type` 取自命令目录；目录缺失时回退
  /// `waypoint`，不让未知命令变成无法编辑的条目。
  ///
  /// [frame] 统一覆盖高度基准帧（例如 `MAV_FRAME_GLOBAL_TERRAIN_ALT`），
  /// 用于测绘页选「地形跟随 (AGL)」时把整条航线写成同一基准。
  List<MissionEntity> entitiesFor(
    SurveyResult result,
    List<MissionCommandDescriptor> catalog, {
    int? frame,
  }) => [
    for (final waypoint in result.waypoints)
      waypoint.toMissionEntity(
        _entityTypeFor(catalog, waypoint.command),
        frame: frame,
      ),
  ];

  /// 生成结果并入当前任务文档，返回追加的实体个数。
  ///
  /// 文档尚未初始化时按需创建，保证「先测绘、后开任务面板」也能成功并入。
  Future<int> applyToMission({
    required MissionDocumentController mission,
    required SurveyResult result,
    int? frame,
  }) async {
    if (!result.valid) {
      throw StateError(result.message.isEmpty ? '测绘结果无效' : result.message);
    }
    await mission.initialize();
    var document = mission.document;
    if (document == null) {
      await mission.create();
      document = mission.document;
    }
    if (document == null) {
      throw StateError('任务文档不可用');
    }
    final entities = entitiesFor(result, mission.commandCatalog, frame: frame);
    if (entities.isEmpty) return 0;
    final before = document.entities.length;
    await mission.replaceDocument(
      document.copyWith(entities: [...document.entities, ...entities]),
    );
    // `MissionDocumentController._run` 会把后端错误收进 `error` 而不向外抛出，
    // 这里显式复查，避免把一次失败的 replace 当成「已并入」报给用户。
    final failure = mission.error;
    if (failure != null && failure.isNotEmpty) {
      throw StateError(failure);
    }
    final after = mission.document?.entities.length ?? 0;
    if (after < before + entities.length) {
      throw StateError('任务文档未接纳测绘结果（$before → $after，期望追加 ${entities.length} 项）');
    }
    return after - before;
  }

  static String _entityTypeFor(
    List<MissionCommandDescriptor> catalog,
    int command,
  ) {
    for (final descriptor in catalog) {
      if (descriptor.command == command) return descriptor.entityType;
    }
    return 'waypoint';
  }
}
