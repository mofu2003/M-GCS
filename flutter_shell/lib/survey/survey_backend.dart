import 'survey_models.dart';

/// 测绘生成的 backend 接口。生成是纯计算，不需要连接飞控。
abstract interface class SurveyBackend {
  Future<SurveyResult> generateGrid(SurveyGridConfig config);

  Future<SurveyResult> generateCorridor(SurveyCorridorConfig config);
}

/// native 库不可用时的降级实现：明确报错，不静默返回空结果。
final class SurveyUnavailableBackend implements SurveyBackend {
  const SurveyUnavailableBackend();

  @override
  Future<SurveyResult> generateGrid(SurveyGridConfig config) async =>
      const SurveyResult(valid: false, message: '原生库未加载，无法生成测绘航线');

  @override
  Future<SurveyResult> generateCorridor(SurveyCorridorConfig config) async =>
      const SurveyResult(valid: false, message: '原生库未加载，无法生成测绘航线');
}
