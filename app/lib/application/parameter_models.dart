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
  });

  final String name;
  final String displayName;
  final String description;
  final String group;
  final String groupDisplayName;
  final String type;
  final bool found;
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
