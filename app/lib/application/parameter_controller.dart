import 'package:flutter/foundation.dart';

import 'connection_models.dart';
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
  bool _fileBusy = false;
  bool _descriptorCatalogLoading = false;
  String? _errorMessage;

  List<FlightControllerParameter> get parameters => _parameters;
  List<StagedParameterChange> get stagedChanges => _staged.values.toList();
  bool get refreshing => _refreshing;
  bool get writing => _writing;
  bool get fileBusy => _fileBusy;
  String? get errorMessage => _errorMessage;
  bool get canEdit =>
      isSessionReady() && !_writing && !_refreshing && !_fileBusy;
  bool get canManageParameterFiles =>
      backend is ParameterFileSessionBackend &&
      _parameters.isNotEmpty &&
      canEdit;

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
    await _loadParameters(() => backend.loadParameters());
  }

  Future<void> refreshFromVehicle() async {
    if (backend case final ParameterRefreshSessionBackend refreshBackend) {
      await _loadParameters(refreshBackend.refreshParameters);
    } else {
      await _loadParameters(backend.loadParameters);
    }
  }

  Future<void> _loadParameters(
    Future<List<FlightControllerParameter>> Function() loader,
  ) async {
    if (!isSessionReady() || _refreshing || _writing || _fileBusy) return;
    _refreshing = true;
    _errorMessage = null;
    notifyListeners();
    try {
      _parameters = await loader();
      _parameters = [..._parameters]..sort((a, b) => a.name.compareTo(b.name));
      _loadDescriptorCatalog();
    } on Object catch (error) {
      _errorMessage = '读取参数失败：$error';
    } finally {
      _refreshing = false;
      notifyListeners();
    }
  }

  Future<ParameterFileImportSummary> importParameterFile(String content) async {
    if (!canManageParameterFiles || backend is! ParameterFileSessionBackend) {
      throw StateError('当前参数会话无法读取文件');
    }
    final fileBackend = backend as ParameterFileSessionBackend;
    _fileBusy = true;
    _errorMessage = null;
    notifyListeners();
    try {
      final entries = await fileBackend.parseParameterFile(content);
      if (backend
          case final ParameterChangeSetSessionBackend changeSetBackend) {
        final preview = await changeSetBackend.prepareParameterChangeSet(
          entries,
        );
        final current = {for (final item in _parameters) item.name: item};
        for (final item in preview.items) {
          switch (item.disposition) {
            case 'ready':
              final parameter = current[item.name];
              if (parameter != null) {
                _staged[item.name] = StagedParameterChange(
                  parameter: parameter,
                  stagedValue: item.value,
                );
              }
            case 'unchanged':
              _staged.remove(item.name);
          }
        }
        return ParameterFileImportSummary(
          parsed: preview.items.length,
          staged: preview.ready,
          unchanged: preview.unchanged,
          unknown: preview.unknown + preview.invalid,
        );
      }
      final imported = <String, ParameterFileEntry>{};
      for (final entry in entries) {
        if (entry.name.isNotEmpty && entry.value.isFinite) {
          imported[entry.name] = entry;
        }
      }
      if (imported.isEmpty) {
        throw const FormatException('文件中没有可读取的参数');
      }

      final current = {for (final item in _parameters) item.name: item};
      var staged = 0;
      var unchanged = 0;
      var unknown = 0;
      for (final entry in imported.values) {
        final parameter = current[entry.name];
        if (parameter == null) {
          unknown++;
        } else if (parameter.value == entry.value) {
          _staged.remove(entry.name);
          unchanged++;
        } else {
          _staged[entry.name] = StagedParameterChange(
            parameter: parameter,
            stagedValue: entry.value,
          );
          staged++;
        }
      }
      return ParameterFileImportSummary(
        parsed: imported.length,
        staged: staged,
        unchanged: unchanged,
        unknown: unknown,
      );
    } finally {
      _fileBusy = false;
      notifyListeners();
    }
  }

  Future<String> exportParameterFile() async {
    if (!canManageParameterFiles || backend is! ParameterFileSessionBackend) {
      throw StateError('当前参数会话无法保存文件');
    }
    final fileBackend = backend as ParameterFileSessionBackend;
    _fileBusy = true;
    _errorMessage = null;
    notifyListeners();
    try {
      return await fileBackend.createParameterFile(_parameters);
    } finally {
      _fileBusy = false;
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

  Future<ConnectionOperationResult> writeValueImmediately(
    String name,
    double value,
  ) async {
    if (!canEdit || !value.isFinite) {
      return const ConnectionOperationResult(
        success: false,
        message: '当前无法写入参数',
      );
    }
    final parameter = _parameters
        .where((item) => item.name == name)
        .firstOrNull;
    if (parameter == null) {
      return const ConnectionOperationResult(
        success: false,
        message: '飞控未提供该参数',
      );
    }
    if (parameter.value == value) {
      _staged.remove(name);
      return const ConnectionOperationResult(success: true, message: '参数未变化');
    }

    _writing = true;
    _errorMessage = null;
    notifyListeners();
    try {
      final result = await backend.writeParameter(parameter, value);
      if (result.success) {
        _parameters = _parameters
            .map(
              (item) => item.name == name ? item.copyWith(value: value) : item,
            )
            .toList(growable: false);
        _staged.remove(name);
      } else {
        _errorMessage = '$name 写入失败：${result.message}';
      }
      return result;
    } on Object catch (error) {
      _errorMessage = '$name 写入失败：$error';
      return ConnectionOperationResult(
        success: false,
        message: error.toString(),
      );
    } finally {
      _writing = false;
      notifyListeners();
    }
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
    if (backend case final ParameterChangeSetSessionBackend changeSetBackend) {
      try {
        final result = await changeSetBackend.commitParameterChangeSet(
          _staged.values.toList(growable: false),
        );
        _applyWriteSummary(result);
        return result;
      } on Object catch (error) {
        _errorMessage = '参数写入失败：$error';
        return ParameterWriteSummary(
          succeeded: const [],
          failed: {
            for (final change in _staged.values)
              change.parameter.name: error.toString(),
          },
        );
      } finally {
        _writing = false;
        notifyListeners();
      }
    }
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

  void _applyWriteSummary(ParameterWriteSummary result) {
    final valuesByName = {
      for (final change in _staged.values)
        change.parameter.name: change.stagedValue,
    };
    for (final name in result.succeeded) {
      final value = valuesByName[name];
      if (value != null) {
        _parameters = _parameters
            .map(
              (item) => item.name == name ? item.copyWith(value: value) : item,
            )
            .toList(growable: false);
      }
      _staged.remove(name);
    }
    for (final entry in result.failed.entries) {
      final change = _staged[entry.key];
      if (change != null) {
        _staged[entry.key] = change.copyWith(errorMessage: entry.value);
      }
    }
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
