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

  Future<JsonMap> verifyMissionDocument({double toleranceMeters = 0.1});

  Future<MissionDocument> importMissionDocument(String json);

  Future<String> exportMissionDocument();

  Future<void> saveMissionDocument(String filePath);

  Future<MissionDocument> loadMissionDocument(String filePath);
}
