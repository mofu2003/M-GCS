import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/features/backstage/parameter_tree.dart';

void main() {
  test('uses only top-level categories with complete parameter leaves', () {
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
    expect(atc.children.map((node) => node.parameterName), {
      'ATC_RAT_RLL_I',
      'ATC_RAT_RLL_P',
    });
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

  test('groups FLTMODE1 through FLTMODE6 under FLTMODE', () {
    final tree = ParameterTreeNode.build([
      'FLTMODE1',
      'FLTMODE2',
      'FLTMODE6',
      'FLTMODE_CH',
      'SIMPLE',
    ]);

    final flightModes = tree.children.singleWhere(
      (node) => node.label == 'FLTMODE',
    );
    expect(flightModes.prefix, 'FLTMODE');
    expect(flightModes.children.map((node) => node.parameterName), [
      'FLTMODE1',
      'FLTMODE2',
      'FLTMODE6',
      'FLTMODE_CH',
    ]);
    expect(
      tree.children.singleWhere((node) => node.label == 'SIMPLE').isLeaf,
      isTrue,
    );
  });
}
