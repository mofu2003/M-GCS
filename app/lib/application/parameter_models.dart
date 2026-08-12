final class FlightControllerParameter {
  const FlightControllerParameter({
    required this.name,
    required this.value,
    required this.mavlinkType,
    required this.index,
    required this.count,
  });

  final String name;
  final double value;
  final int mavlinkType;
  final int index;
  final int count;

  FlightControllerParameter copyWith({double? value}) {
    return FlightControllerParameter(
      name: name,
      value: value ?? this.value,
      mavlinkType: mavlinkType,
      index: index,
      count: count,
    );
  }
}

final class ParameterDescriptor {
  const ParameterDescriptor({
    required this.name,
    required this.displayName,
    required this.description,
    required this.group,
    required this.groupDisplayName,
    required this.type,
    required this.found,
    this.options = const [],
    this.unit = '',
    this.category = '',
    this.minValue,
    this.maxValue,
    this.increment,
    this.requiresReboot = false,
    this.readOnly = false,
  });

  final String name;
  final String displayName;
  final String description;
  final String group;
  final String groupDisplayName;
  final String type;
  final bool found;
  final List<ParameterOption> options;
  final String unit;
  final String category;
  final double? minValue;
  final double? maxValue;
  final double? increment;
  final bool requiresReboot;
  final bool readOnly;

  ParameterDescriptor withGroupDisplayName(String value) {
    return ParameterDescriptor(
      name: name,
      displayName: displayName,
      description: description,
      group: group,
      groupDisplayName: value,
      type: type,
      found: found,
      options: options,
      unit: unit,
      category: category,
      minValue: minValue,
      maxValue: maxValue,
      increment: increment,
      requiresReboot: requiresReboot,
      readOnly: readOnly,
    );
  }
}

final class ParameterOption {
  const ParameterOption({
    required this.value,
    required this.label,
    this.description = '',
  });

  final int value;
  final String label;
  final String description;
}

final class StagedParameterChange {
  const StagedParameterChange({
    required this.parameter,
    required this.stagedValue,
    this.errorMessage,
  });

  final FlightControllerParameter parameter;
  final double stagedValue;
  final String? errorMessage;

  StagedParameterChange copyWith({
    String? errorMessage,
    bool clearError = false,
  }) {
    return StagedParameterChange(
      parameter: parameter,
      stagedValue: stagedValue,
      errorMessage: clearError ? null : errorMessage ?? this.errorMessage,
    );
  }
}

final class ParameterWriteSummary {
  const ParameterWriteSummary({required this.succeeded, required this.failed});

  final List<String> succeeded;
  final Map<String, String> failed;

  bool get hasFailures => failed.isNotEmpty;
}

final class ParameterChangeSetPreview {
  const ParameterChangeSetPreview({
    required this.ready,
    required this.unchanged,
    required this.unknown,
    required this.invalid,
    required this.items,
  });

  final int ready;
  final int unchanged;
  final int unknown;
  final int invalid;
  final List<ParameterChangeSetItem> items;
}

final class ParameterChangeSetItem {
  const ParameterChangeSetItem({
    required this.name,
    required this.value,
    required this.mavlinkType,
    required this.disposition,
    this.reason = '',
  });

  final String name;
  final double value;
  final int mavlinkType;
  final String disposition;
  final String reason;
}

final class ParameterFileEntry {
  const ParameterFileEntry({
    required this.name,
    required this.value,
    this.mavlinkType = 9,
  });

  final String name;
  final double value;
  final int mavlinkType;
}

final class ParameterFileImportSummary {
  const ParameterFileImportSummary({
    required this.parsed,
    required this.staged,
    required this.unchanged,
    required this.unknown,
  });

  final int parsed;
  final int staged;
  final int unchanged;
  final int unknown;
}

final class ServoOutputSnapshot {
  const ServoOutputSnapshot({required this.values, required this.valid});

  final List<int> values;
  final bool valid;
}

final class RemoteControlInputSnapshot {
  const RemoteControlInputSnapshot({
    required this.values,
    required this.channelCount,
    required this.valid,
  });

  final List<int> values;
  final int channelCount;
  final bool valid;
}

/// Physical UART/USART information reported by ArduPilot's @SYS/uarts.txt.
/// SERIALn is a logical port and cannot be mapped safely by arithmetic.
final class SerialPortHardwareDescriptor {
  const SerialPortHardwareDescriptor({
    required this.serialIndex,
    required this.name,
    this.flowControl,
  });

  final int serialIndex;
  final String name;
  final int? flowControl;
}
