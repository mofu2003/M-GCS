import 'mission_document_models.dart';

abstract interface class MissionDocumentBackend {
  Future<MissionDocument> createMissionDocument([JsonMap config = const {}]);

  Future<MissionDocument> getMissionDocument();

  Future<MissionDocumentOperationResult> applyMissionDocumentCommand(
    JsonMap command,
  );

  Future<MissionDocumentOperationResult> undoMissionDocument();

  Future<MissionDocumentOperationResult> redoMissionDocument();

  Future<MissionValidationReport> validateMissionDocument();

  Future<MissionCompileResult> compileMissionDocument();

  Future<MissionEstimate> estimateMissionDocument();

  Future<List<MissionCommandDescriptor>> getMissionCommandCatalog();

  Future<JsonMap> uploadMissionDocument(int expectedRevision);

  Future<void> startMissionDownload();

  Future<MissionTransferProgress> getMissionTransferProgress();

  /// 任务执行进度（当前项 / 已到达 / 是否跑完）。
  Future<MissionExecutionProgress> getMissionExecutionProgress();

  /// 清空飞控上的任务（保留本地文档）。MP/QGC 覆盖前都会先确认，这里由调用方确认。
  Future<void> clearVehicleMission();

  Future<void> cancelMissionTransfer();

  Future<MissionDocument> importMissionDocumentFromVehicle();

  Future<MissionVerificationResult> verifyMissionDocument({
    double toleranceMeters = 1.0,
  });

  /// 围栏是独立的 MAVLink mission type，不随任务上传。下面这组接口与任务
  /// 上传/读取/核对对称，差别只是走 fence mission type。
  Future<JsonMap> compileFencePlan();

  Future<JsonMap> uploadFence(int expectedRevision);

  Future<void> startFenceDownload();

  Future<MissionDocument> importFenceFromVehicle();

  Future<MissionVerificationResult> verifyFence({double toleranceMeters = 1.0});

  Future<MissionTransferProgress> getFenceTransferProgress();

  Future<void> clearFence();

  /// Rally 同样是独立 mission type（MAV_MISSION_TYPE_RALLY）。
  Future<JsonMap> compileRallyPlan();

  Future<JsonMap> uploadRally(int expectedRevision);

  Future<void> startRallyDownload();

  Future<MissionDocument> importRallyFromVehicle();

  Future<MissionVerificationResult> verifyRally({double toleranceMeters = 1.0});

  Future<MissionTransferProgress> getRallyTransferProgress();

  Future<void> clearRally();

  Future<MissionDocument> importMissionDocument(String json);

  Future<String> exportMissionDocument();

  /// 按交换格式导出任务：`waypoints`（QGC WPL 110 文本）或 `plan`（QGC JSON）。
  Future<MissionInterchangeExport> exportMissionInterchange(String format);

  Future<void> saveMissionDocument(String filePath);

  Future<MissionDocument> loadMissionDocument(String filePath);
}
