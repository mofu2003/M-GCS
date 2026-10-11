/**
 * firmware_plugin_px4.cc - PX4 FirmwarePlugin implementation
 * PX4 插件实现 — 直接消费 modes/px4_modes.h 的唯一模式表
 *
 * PX4 encodes flight modes as: custom_mode = (main_mode << 16) | (sub_mode << 24)
 *
 * 本文件不再自带模式表：表在 core/include/core/firmware/modes/px4_modes.h，
 * 与 FirmwareAdapter 的状态栏解码共用，避免两张表再次漂移。
 *
 * QGC precedent: src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc
 */

#include "core/firmware/px4_plugin.h"
#include "core/firmware/modes/px4_modes.h"

namespace mg::core::firmware {

std::string PX4Plugin::decode_flight_mode(VehicleKind /*vehicle*/, uint32_t custom_mode) const {
    const std::string name = modes::decode_px4_mode(custom_mode);
    if (name.empty()) {
        return "Mode(" + std::to_string(custom_mode) + ")";
    }
    return name;
}

int PX4Plugin::encode_flight_mode(VehicleKind /*vehicle*/, const std::string& mode_name) const {
    return modes::encode_px4_mode_name(mode_name);
}

std::vector<FlightModeEntry> PX4Plugin::available_modes(VehicleKind /*vehicle*/) const {
    size_t count = 0;
    const modes::Px4ModeEntry* table = modes::px4_mode_table(count);

    std::vector<FlightModeEntry> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back({
            table[i].name,
            modes::encode_px4_custom_mode(table[i].main_mode, table[i].sub_mode),
            table[i].can_be_set,
        });
    }
    return result;
}

namespace {

/// QGC 判定用的是 Vehicle::multiRotor()/vtol()/fixedWing()，这里按 MAV_TYPE 归类。
enum class Px4VehicleClass { Multirotor, Vtol, FixedWing, Rover, Sub, Other };

Px4VehicleClass classify_px4_vehicle(VehicleKind v) {
    switch (v) {
        case VehicleKind::Quadrotor:
        case VehicleKind::Coaxial:
        case VehicleKind::Helicopter:
        case VehicleKind::Hexarotor:
        case VehicleKind::Octorotor:
        case VehicleKind::Tricopter:
            return Px4VehicleClass::Multirotor;
        case VehicleKind::FixedWing:
            return Px4VehicleClass::FixedWing;
        case VehicleKind::GroundRover:
        case VehicleKind::SurfaceBoat:
            return Px4VehicleClass::Rover;
        case VehicleKind::Submarine:
            return Px4VehicleClass::Sub;
        default:
            if (static_cast<uint8_t>(v) >= 19 && static_cast<uint8_t>(v) <= 24) {
                return Px4VehicleClass::Vtol;
            }
            return Px4VehicleClass::Other;
    }
}

}  // namespace

uint32_t PX4Plugin::capabilities(VehicleKind vehicle) const {
    // 照 QGC `PX4FirmwarePlugin::isCapable`（PX4FirmwarePlugin.cc:174-190）按机型细化：
    //   base: SetFlightMode | PauseVehicle | GuidedMode
    //   multiRotor: + ROI | ChangeHeading
    //   multiRotor | vtol: + TakeoffVehicle | GuidedTakeoff | OrbitMode
    //   vtol: + VTOLMulticopterTakeoff（M-GCS 没有独立能力位）
    //   fixedWing: + TakeoffVehicle | OrbitMode
    //
    // M-GCS 的差异：
    //   - 没有 SetFlightMode 能力位（模式切换不走能力位门控）；
    //   - CapTakeoff 在本项目里等价于 QGC 的 GuidedTakeoff（前端“起飞”按钮），
    //     所以固定翼按 QGC 只拿 OrbitMode，不发 CapTakeoff；
    //   - 保留 CapSetHome：M-GCS 的 DO_SET_HOME 走通用路径，PX4 会 ACK 并接受，
    //     与 QGC 因自身 home 流程不同而隐藏该入口的做法不同（见 px4-parity.md）。
    uint32_t caps = CapGuidedMode | CapPause | CapCalibration | CapSetHome;
    switch (classify_px4_vehicle(vehicle)) {
        case Px4VehicleClass::Multirotor:
            caps |= CapRoi | CapChangeHeading | CapTakeoff | CapOrbit;
            break;
        case Px4VehicleClass::Vtol:
            caps |= CapTakeoff | CapOrbit;
            break;
        case Px4VehicleClass::FixedWing:
            caps |= CapOrbit;
            break;
        case Px4VehicleClass::Rover:
        case Px4VehicleClass::Sub:
        case Px4VehicleClass::Other:
            break;
    }
    return caps;
}

float PX4Plugin::minimum_takeoff_altitude(VehicleKind /*vehicle*/) const {
    // QGC PX4 不覆写 FirmwarePlugin::minimumTakeoffAltitudeMeters()，用基类默认
    // 10 英尺 = 3.048 m（FirmwarePlugin.h:211）。
    return 3.048f;
}

} // namespace mg::core::firmware
