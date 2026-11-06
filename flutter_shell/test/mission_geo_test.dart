import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/mission/mission_geo.dart';

void main() {
  test('RTL is drawn at Home and never at its unused coordinate', () {
    const rtl = MissionEntity(command: 20, latitude: 0, longitude: 0);
    expect(missionEntityMapPosition(rtl, const MissionHome()), isNull);
    final point = missionEntityMapPosition(
      rtl,
      const MissionHome(set: true, latitude: 32.5, longitude: 114.7),
    );
    expect(point, const LatLng(32.5, 114.7));
  });

  test('mission distance uses Mission Planner Haversine radius', () {
    final distance = wgs84DistanceMeters(
      const LatLng(0, 0),
      const LatLng(1, 0),
    );

    expect(distance, closeTo(111194.9, 0.5));
  });

  test('mission distance formatting switches to kilometres', () {
    expect(formatMissionDistance(89.4), '89 m');
    expect(formatMissionDistance(1240), '1.24 km');
  });

  test('takeoff stays at the clicked reference even when Home is set', () {
    final position = missionEntityMapPosition(
      const MissionEntity(
        command: kMavCmdNavTakeoff,
        latitude: 31,
        longitude: 121,
      ),
      const MissionHome(set: true, latitude: 32.5, longitude: 114.7),
    );

    expect(position?.latitude, 31);
    expect(position?.longitude, 121);
  });

  test('takeoff keeps a reference position when Home is not set', () {
    final position = missionEntityMapPosition(
      const MissionEntity(
        command: kMavCmdNavTakeoff,
        latitude: 31,
        longitude: 121,
      ),
      const MissionHome(),
    );

    expect(position?.latitude, 31);
    expect(position?.longitude, 121);
  });
  test('VTOL takeoff keeps its own coordinate instead of snapping to Home', () {
    final point = missionEntityMapPosition(
      const MissionEntity(
        command: kMavCmdNavVtolTakeoff,
        latitude: 31,
        longitude: 121,
      ),
      const MissionHome(set: true, latitude: 32.5, longitude: 114.7),
    );
    expect(point, const LatLng(31, 121));
  });

  test(
    'takeoff without a reference falls back to Home instead of Null Island',
    () {
      for (final command in [kMavCmdNavTakeoff, kMavCmdNavVtolTakeoff]) {
        for (final entity in [
          MissionEntity(command: command),
          MissionEntity(command: command, latitude: 0, longitude: 0),
        ]) {
          expect(
            missionEntityMapPosition(
              entity,
              const MissionHome(set: true, latitude: 32.5, longitude: 114.7),
            ),
            const LatLng(32.5, 114.7),
          );
          expect(missionEntityMapPosition(entity, const MissionHome()), isNull);
        }
      }
    },
  );
}
