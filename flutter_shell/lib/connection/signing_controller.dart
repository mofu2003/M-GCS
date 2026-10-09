import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'native_core.dart';
import 'signing_key_store.dart';
import 'vehicle_signing_bindings.dart';

/// MAVLink 签名的密钥库、自动探测与下发。
///
/// 原先这一整块（约 270 行）长在 `ConnectionController` 里。它真正需要的上下文只有
/// 三样：原生核心、当前会话号、机队里的飞机列表——都通过构造回调注入，
/// 于是这个类可以脱离控制器单独测试，控制器也不再背签名的细节。
class SigningController extends ChangeNotifier {
  SigningController({
    required NativeCore? Function() native,
    required bool Function() ready,
    required int? Function() sessionId,
    required void Function() refreshVehicles,
    required Iterable<int> Function() vehicleIds,
    SigningKeyStore? keyStore,
  }) : _signingKeyStore = keyStore ?? SigningKeyStore() {
    _native = native;
    _ready = ready;
    _sessionId = sessionId;
    _refreshVehicles = refreshVehicles;
    _vehicleIds = vehicleIds;
  }

  late final NativeCore? Function() _native;
  late final bool Function() _ready;
  late final int? Function() _sessionId;
  late final void Function() _refreshVehicles;
  late final Iterable<int> Function() _vehicleIds;
  final SigningKeyStore _signingKeyStore;

  List<SigningKeyPreference> _signingKeys = const [];
  String? _activeSigningKeyName;
  final _signingBindings = VehicleSigningBindings();
  int? _signingCandidatesVehicleId;
  bool _disposed = false;

  /// 当前密钥库里的条目（只读）。
  List<SigningKeyPreference> get keys => List.unmodifiable(_signingKeys);

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }

  NativeCore _requireNative() {
    final native = _native();
    if (native == null) throw StateError('原生后端未初始化，无法操作 MAVLink 签名');
    return native;
  }

  int? sessionId() => _sessionId();

  SigningKeyPreference? get preference {
    final name = _activeSigningKeyName;
    if (name == null) return null;
    for (final entry in _signingKeys) {
      if (entry.name == name) return entry;
    }
    return null;
  }

  Future<void> loadLibrary() async {
    final library = await _signingKeyStore.loadLibrary();
    _signingKeys = List.of(library.entries);
    _activeSigningKeyName = library.activeName;
    if (sessionId() case final int id) _signingBindings.invalidate(id);
    _signingCandidatesVehicleId = null;
    unawaited(applyIfNeeded());
    notifyListeners();
  }

  Future<void> _persistSigningLibrary() async {
    await _signingKeyStore.saveLibrary(
      SigningKeyLibrary(
        entries: List.of(_signingKeys),
        activeName: _activeSigningKeyName,
      ),
    );
  }

  Future<void> upsertSigningKey({
    required String name,
    required String keyHex,
    bool signOutgoing = true,
    int lastTimestampTicks = 0,
  }) async {
    final normalizedName = name.trim();
    final normalizedKey = keyHex.trim().toLowerCase();
    if (normalizedName.isEmpty) {
      throw ArgumentError('密钥名称不能为空');
    }
    if (!isValidSigningKeyHex(normalizedKey)) {
      throw ArgumentError('MAVLink signing key 必须是 64 位十六进制字符');
    }
    final next = List<SigningKeyPreference>.of(_signingKeys);
    final index = next.indexWhere(
      (entry) => entry.name.toLowerCase() == normalizedName.toLowerCase(),
    );
    final entry = SigningKeyPreference(
      name: normalizedName,
      keyHex: normalizedKey,
      signOutgoing: signOutgoing,
      lastTimestampTicks: lastTimestampTicks,
    );
    if (index >= 0) {
      next[index] = entry;
    } else {
      next.add(entry);
    }
    _signingKeys = next;
    _activeSigningKeyName = normalizedName;
    if (sessionId() case final int id) _signingBindings.bind(id, normalizedName);
    _signingCandidatesVehicleId = null;
    await _persistSigningLibrary();
    if (sessionId() case final int id) _signingBindings.invalidate(id);
    await applyIfNeeded();
    notifyListeners();
  }

  Future<void> selectSigningKey(String name) async {
    final entry = _signingKeys.firstWhere(
      (candidate) => candidate.name == name,
      orElse: () => throw StateError('密钥不存在'),
    );
    _activeSigningKeyName = entry.name;
    if (sessionId() case final int id) _signingBindings.bind(id, entry.name);
    await _persistSigningLibrary();
    if (sessionId() case final int id) _signingBindings.invalidate(id);
    await applyIfNeeded();
    notifyListeners();
  }

  Future<void> removeSigningKey(String name) async {
    for (final id in _signingBindings.removeKey(name)) {
      if (_ready()) {
        _requireNative().clearSigningKey(id);
      }
    }
    if (_ready()) {
      _refreshVehicles();
      for (final id in _vehicleIds()) {
        _requireNative().clearSigningCandidates(id);
      }
    }
    final next = List<SigningKeyPreference>.of(_signingKeys)
      ..removeWhere((entry) => entry.name == name);
    _signingKeys = next;
    if (_activeSigningKeyName == name) {
      _activeSigningKeyName = next.isEmpty ? null : next.first.name;
    }
    await _persistSigningLibrary();
    if (sessionId() case final int id) _signingBindings.invalidate(id);
    _signingCandidatesVehicleId = null;
    await applyIfNeeded();
    if (preference == null) {
      await _clearNativeSigning();
    }
    notifyListeners();
  }

  Future<void> saveSigningKeyPreference({
    required String keyHex,
    bool signOutgoing = true,
    int lastTimestampTicks = 0,
  }) {
    return upsertSigningKey(
      name: _activeSigningKeyName ?? '默认密钥',
      keyHex: keyHex,
      signOutgoing: signOutgoing,
      lastTimestampTicks: lastTimestampTicks,
    );
  }

  Future<void> _clearNativeSigning() async {
    if (sessionId() == null || !_ready()) return;
    final response = _requireNative().clearSigningKey(sessionId()!);
    if (response is! Map || response['success'] != true) {
      throw StateError(nativeErrorText(response, '清除 MAVLink 签名失败'));
    }
    if (sessionId() case final int id) _signingBindings.invalidate(id);
  }

  Future<void> clearSigningKeyPreference() async {
    _signingKeys = const [];
    _signingBindings.clear();
    if (_ready()) {
      _refreshVehicles();
      for (final id in _vehicleIds()) {
        _requireNative().clearSigningKey(id);
        _requireNative().clearSigningCandidates(id);
      }
    }
    _activeSigningKeyName = null;
    if (sessionId() case final int id) _signingBindings.invalidate(id);
    _signingCandidatesVehicleId = null;
    await _signingKeyStore.clear();
    await applyIfNeeded();
    await _clearNativeSigning();
    notifyListeners();
  }

  Future<Map<String, Object?>> readSigningStatus() async {
    final id = sessionId();
    if (id == null || !_ready()) {
      return const {'success': true, 'enabled': false, 'status': 'offline'};
    }
    final response = _requireNative().signingStatus(id);
    if (response is! Map<String, Object?>) {
      throw StateError('MAVLink 签名状态格式无效');
    }
    if (_adoptDetectedSigning(response, id)) {
      await _persistSigningLibrary();
      notifyListeners();
    }
    return response;
  }

  String _signingFingerprint(SigningKeyPreference key) =>
      '${key.name}:${key.keyHex}:${key.signOutgoing}';

  bool _adoptDetectedSigning(Map<String, Object?> status, int vehicleId) {
    if (status['auto_detected'] != true) return false;
    final name = status['key_name'];
    if (name is! String || name.isEmpty) return false;
    final index = _signingKeys.indexWhere((entry) => entry.name == name);
    if (index < 0) return false;
    final current = _signingKeys[index];
    final timestamp = (status['timestamp_ticks'] as num?)?.toInt() ?? 0;
    final nextTimestamp = timestamp > current.lastTimestampTicks
        ? timestamp
        : current.lastTimestampTicks;
    final changed = nextTimestamp != current.lastTimestampTicks;
    if (changed) {
      final next = List<SigningKeyPreference>.of(_signingKeys);
      next[index] = current.copyWith(lastTimestampTicks: nextTimestamp);
      _signingKeys = next;
    }
    _signingBindings.bind(vehicleId, name);
    _signingBindings.markApplied(vehicleId, _signingFingerprint(current));
    return changed;
  }

  Future<void> refreshAutoDetected(int vehicleId) async {
    if (_disposed || sessionId() != vehicleId || !_ready()) {
      return;
    }
    try {
      final response = _requireNative().signingStatus(vehicleId);
      if (response is! Map<String, Object?> || response['success'] != true) {
        return;
      }
      if (_adoptDetectedSigning(response, vehicleId)) {
        await _persistSigningLibrary();
        notifyListeners();
      }
    } on Object {
      // Status polling is best effort and must not disturb telemetry.
    }
  }

  Future<void> sendSetupSigning({
    required String keyHex,
    int initialTimestampTicks = 0,
  }) async {
    final normalized = keyHex.trim().toLowerCase();
    if (!isValidSigningKeyHex(normalized)) {
      throw ArgumentError('MAVLink signing key 必须是 64 位十六进制字符');
    }
    final id = sessionId();
    if (id == null || !_ready()) {
      throw StateError('未连接飞控，无法下发 SETUP_SIGNING');
    }
    final response = _requireNative().sendSetupSigning(id, normalized, initialTimestampTicks);
    if (response is! Map || response['success'] != true) {
      throw StateError(nativeErrorText(response, '下发 SETUP_SIGNING 失败'));
    }
    notifyListeners();
  }

  Future<void> applyIfNeeded() async {
    final id = sessionId();
    if (id == null || !_ready()) return;

    if (_signingCandidatesVehicleId != id) {
      final candidates = jsonEncode([
        for (final entry in _signingKeys)
          {
            'name': entry.name,
            'key_hex': entry.keyHex,
            'timestamp_ticks': entry.lastTimestampTicks,
          },
      ]);
      final response = _requireNative().setSigningCandidates(id, candidates);
      if (response is! Map || response['success'] != true) {
        throw StateError(nativeErrorText(response, '配置 MAVLink 签名候选密钥失败'));
      }
      _signingCandidatesVehicleId = id;
    }

    final nativeStatus = _requireNative().signingStatus(id);
    if (nativeStatus is Map<String, Object?> &&
        _signingBindings.nameFor(id) == null) {
      _adoptDetectedSigning(nativeStatus, id);
    }
    final boundName = _signingBindings.nameFor(id);
    final matches = _signingKeys.where((entry) => entry.name == boundName);
    if (matches.isEmpty) {
      return;
    } // New sessions use candidates, never a global default key.
    final preference = matches.first;
    final fingerprint = _signingFingerprint(preference);
    if (_signingBindings.isApplied(id, fingerprint)) return;
    final response = _requireNative().setSigningKey(
      id,
      preference.keyHex,
      preference.signOutgoing,
      preference.lastTimestampTicks,
    );
    if (response is! Map || response['success'] != true) {
      throw StateError(nativeErrorText(response, '应用 MAVLink 签名失败'));
    }
    _signingBindings.markApplied(id, fingerprint);
  }
}
