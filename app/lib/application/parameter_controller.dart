import 'package:flutter/foundation.dart';

import 'connection_session_backend.dart';
import 'parameter_models.dart';

final class ParameterController extends ChangeNotifier {
  ParameterController({required this.backend, required this.isSessionReady});

  final ParameterSessionBackend backend;
  final ValueGetter<bool> isSessionReady;
  List<FlightControllerParameter> _parameters = const [];
  final Map<String, StagedParameterChange> _staged = {};
  final Map<String, ParameterDescriptor> _descriptors = {};
  bool _refreshing = false;
  bool _writing = false;
  bool _descriptorCatalogLoading = false;
  String? _errorMessage;

  List<FlightControllerParameter> get parameters => _parameters;
  List<StagedParameterChange> get stagedChanges => _staged.values.toList();
  bool get refreshing => _refreshing;
  bool get writing => _writing;
  String? get errorMessage => _errorMessage;
  bool get canEdit => isSessionReady() && !_writing;

  ParameterDescriptor? descriptorFor(String name) => _descriptors[name];

  String treeLabelForPrefix(String prefix) {
    final rawPrefix = prefix.endsWith('_')
        ? prefix.substring(0, prefix.length - 1)
        : prefix;
    if (rawPrefix.isEmpty) return '全部参数';
    final descriptor = _parameters
        .where((parameter) => parameter.name.startsWith(prefix))
        .map((parameter) => _descriptors[parameter.name])
        .whereType<ParameterDescriptor>()
        .firstOrNull;
    final translated = descriptor?.groupDisplayName;
    return translated == null || translated.isEmpty || translated == rawPrefix
        ? rawPrefix
        : '$rawPrefix - $translated';
  }

  Future<void> loadDescriptor(String name) async {
    if (!isSessionReady() || _descriptors.containsKey(name)) return;
    try {
      _descriptors[name] = await backend.loadParameterDescriptor(name);
      notifyListeners();
    } on Object {
      // The raw parameter remains usable when optional metadata is unavailable.
    }
  }

  Future<void> refresh() async {
    if (!isSessionReady() || _refreshing) return;
    _refreshing = true;
    _errorMessage = null;
    notifyListeners();
    try {
      _parameters = await backend.loadParameters();
      _parameters = [..._parameters]..sort((a, b) => a.name.compareTo(b.name));
      _loadDescriptorCatalog();
    } on Object catch (error) {
      _errorMessage = '读取参数失败：$error';
    } finally {
      _refreshing = false;
      notifyListeners();
    }
  }

  Future<void> _loadDescriptorCatalog() async {
    if (_descriptorCatalogLoading || _descriptors.isNotEmpty) return;
    _descriptorCatalogLoading = true;
    try {
      for (final descriptor in await backend.loadParameterDescriptors()) {
        _descriptors[descriptor.name] = descriptor;
      }
      notifyListeners();
    } on Object {
      // Raw parameter navigation remains available without optional metadata.
    } finally {
      _descriptorCatalogLoading = false;
    }
  }

  void stageValue(String name, double value) {
    if (!canEdit || !value.isFinite) return;
    final parameter = _parameters
        .where((item) => item.name == name)
        .firstOrNull;
    if (parameter == null) return;
    if (parameter.value == value) {
      _staged.remove(name);
    } else {
      _staged[name] = StagedParameterChange(
        parameter: parameter,
        stagedValue: value,
      );
    }
    notifyListeners();
  }

  void discardAll() {
    if (_staged.isEmpty || _writing) return;
    _staged.clear();
    notifyListeners();
  }

  Future<ParameterWriteSummary> writeStaged() async {
    if (!canEdit || _staged.isEmpty) {
      return const ParameterWriteSummary(succeeded: [], failed: {});
    }
    _writing = true;
    notifyListeners();
    final succeeded = <String>[];
    final failed = <String, String>{};
    for (final change in _orderedChanges()) {
      final result = await backend.writeParameter(
        change.parameter,
        change.stagedValue,
      );
      if (result.success) {
        succeeded.add(change.parameter.name);
        _parameters = _parameters
            .map(
              (item) => item.name == change.parameter.name
                  ? item.copyWith(value: change.stagedValue)
                  : item,
            )
            .toList(growable: false);
        _staged.remove(change.parameter.name);
      } else {
        failed[change.parameter.name] = result.message;
        _staged[change.parameter.name] = change.copyWith(
          errorMessage: result.message,
        );
      }
    }
    _writing = false;
    notifyListeners();
    return ParameterWriteSummary(succeeded: succeeded, failed: failed);
  }

  List<StagedParameterChange> _orderedChanges() {
    final changes = _staged.values.toList();
    changes.sort((left, right) {
      final leftEnable = left.parameter.name.endsWith('_ENABLE');
      final rightEnable = right.parameter.name.endsWith('_ENABLE');
      if (leftEnable == rightEnable) {
        return left.parameter.name.compareTo(right.parameter.name);
      }
      return leftEnable ? 1 : -1;
    });
    return changes;
  }
}
