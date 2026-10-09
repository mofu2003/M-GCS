/// Session-local signing ownership. The shared key library is not a default
/// binding for every vehicle; only a user action or verified detection binds.
final class VehicleSigningBindings {
  final Map<int, String> _names = {};
  final Map<int, String> _applied = {};

  String? nameFor(int vehicleId) => _names[vehicleId];
  void bind(int vehicleId, String name) {
    if (_names[vehicleId] != name) _applied.remove(vehicleId);
    _names[vehicleId] = name;
  }

  bool isApplied(int vehicleId, String fingerprint) =>
      _applied[vehicleId] == fingerprint;
  void markApplied(int vehicleId, String fingerprint) =>
      _applied[vehicleId] = fingerprint;
  void invalidate(int vehicleId) => _applied.remove(vehicleId);
  List<int> removeKey(String name) {
    final affected = _names.entries
        .where((entry) => entry.value == name)
        .map((entry) => entry.key)
        .toList();
    for (final id in affected) {
      _names.remove(id);
      _applied.remove(id);
    }
    return affected;
  }

  void clear() {
    _names.clear();
    _applied.clear();
  }
}
