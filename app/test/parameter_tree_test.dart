import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/features/backstage/parameter_tree.dart';

void main() {
  test('uses top-level categories and only keeps shared deep prefixes', () {
    final tree = ParameterTreeNode.build([
      'GPS_TYPE',
      'GPS_TYPE2',
      'Q_ENABLE',
      'BATT_MONITOR',
      'BATT2_MONITOR',
      'ATC_RAT_RLL_I',
      'ATC_RAT_RLL_P',
    ]);

    final gps = tree.children.firstWhere((node) => node.label == 'GPS');
    final atc = tree.children.firstWhere((node) => node.label == 'ATC');

    expect(gps.prefix, 'GPS_');
    expect(gps.children.map((node) => node.parameterName), {
      'GPS_TYPE',
      'GPS_TYPE2',
    });
    expect(tree.children.any((node) => node.label == 'Q'), isTrue);
    expect(tree.children.any((node) => node.label == 'BATT'), isTrue);
    expect(tree.children.any((node) => node.label == 'BATT2'), isTrue);
    expect(atc.children.single.label, 'ATC_RAT');
    expect(atc.children.single.children.single.label, 'ATC_RAT_RLL');
    expect(
      atc.children.single.children.single.children.map(
        (node) => node.parameterName,
      ),
      {
      'ATC_RAT_RLL_I',
      'ATC_RAT_RLL_P',
      },
    );
  });

  test('does not add a deep branch without a shared prefix', () {
    final tree = ParameterTreeNode.build(['BATT_MONITOR']);
    final batt = tree.children.single;

    expect(batt.label, 'BATT');
    expect(batt.children.single.parameterName, 'BATT_MONITOR');
  });

  test('keeps a parameter without an underscore as a root leaf', () {
    final tree = ParameterTreeNode.build(['FORMAT_VERSION', 'SIMPLE']);

    final simple = tree.children.firstWhere((node) => node.label == 'SIMPLE');
    expect(simple.parameterName, 'SIMPLE');
    expect(simple.children, isEmpty);
  });
}
