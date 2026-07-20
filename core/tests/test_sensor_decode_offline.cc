/**
 * test_sensor_decode_offline.cc - Comprehensive sensor ID decoder tests
 *
 * Tests all AP and PX4 sensor categories with known device IDs.
 * No hardware needed — purely offline database lookups.
 */
#include "core/sensor/sensor_id_decoder.h"
#include "core/platform/platform_filesystem.h"
#include <cstdint>
#include <cstdio>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

int main() {
    fprintf(stderr, "=== Sensor ID Decoder Comprehensive Tests ===\n\n");

    std::string db_path = mg::core::platform::find_data_file("SensorDeviceInfo.json");
    mg::core::sensor::SensorIdDecoder dec(db_path);
    CHECK(dec.is_loaded(), "database loaded successfully");

    // PX4 IMU sensors
    fprintf(stderr, "[Group] PX4 IMU sensors\n");
    {
        auto r = dec.decode_px4_type_value(144, "imu");
        CHECK(r.valid && r.name == "ICM42688-P", "PX4 ICM42688-P");
    }
    {
        auto r = dec.decode_px4_type_value(176, "imu");
        CHECK(r.valid && r.name == "BMI270", "PX4 BMI270");
    }
    {
        auto r = dec.decode_px4_type_value(136, "imu");
        CHECK(r.valid && r.name == "ICM20602", "PX4 ICM20602");
    }
    {
        auto r = dec.decode_px4_type_value(96, "imu");
        CHECK(r.valid && r.name == "MPU6000", "PX4 MPU6000");
    }
    {
        auto r = dec.decode_px4_type_value(97, "imu");
        CHECK(r.valid && r.name == "MPU9250", "PX4 MPU9250");
    }
    {
        auto r = dec.decode_px4_type_value(105, "imu");
        CHECK(r.valid && r.name == "BMI160", "PX4 BMI160");
    }
    {
        auto r = dec.decode_px4_type_value(121, "imu");
        CHECK(r.valid && r.name == "BMI088", "PX4 BMI088");
    }
    {
        auto r = dec.decode_px4_type_value(173, "imu");
        CHECK(r.valid && r.name == "ICM42670P", "PX4 ICM42670P");
    }

    // PX4 Mag sensors
    fprintf(stderr, "[Group] PX4 Mag sensors\n");
    {
        auto r = dec.decode_px4_type_value(196, "mag");
        CHECK(r.valid && r.name == "LIS3MDL", "PX4 LIS3MDL");
    }
    {
        auto r = dec.decode_px4_type_value(200, "mag");
        CHECK(r.valid && r.name == "AK09916", "PX4 AK09916");
    }
    {
        auto r = dec.decode_px4_type_value(114, "mag");
        CHECK(r.valid && r.name == "QMC5883L", "PX4 QMC5883L");
    }
    {
        auto r = dec.decode_px4_type_value(97, "mag");
        CHECK(r.valid && r.name == "HMC5883", "PX4 HMC5883");
    }

    // PX4 Baro sensors
    fprintf(stderr, "[Group] PX4 Baro sensors\n");
    {
        auto r = dec.decode_px4_type_value(50, "baro");
        CHECK(r.valid && r.name == "MS5611", "PX4 MS5611");
    }
    {
        auto r = dec.decode_px4_type_value(48, "baro");
        CHECK(r.valid && r.name == "BMP280", "PX4 BMP280");
    }
    {
        auto r = dec.decode_px4_type_value(177, "baro");
        CHECK(r.valid && r.name == "BMP388", "PX4 BMP388");
    }
    {
        auto r = dec.decode_px4_type_value(52, "baro");
        CHECK(r.valid && r.name == "DPS310", "PX4 DPS310");
    }

    // ArduPilot INS sensors
    fprintf(stderr, "[Group] ArduPilot INS sensors\n");
    {
        auto r = dec.decode_device_id(56 << 16, "ins");
        CHECK(r.valid && r.name == "BMI270", "AP BMI270 (devid=56)");
    }
    {
        auto r = dec.decode_device_id(52 << 16, "ins");
        CHECK(r.valid && r.name == "ICM42688", "AP ICM42688 (devid=52)");
    }
    {
        auto r = dec.decode_device_id(43 << 16, "ins");
        CHECK(r.valid && r.name == "BMI088", "AP BMI088 (devid=43)");
    }
    {
        auto r = dec.decode_device_id(9 << 16, "ins");
        CHECK(r.valid && r.name == "BMI160", "AP BMI160 (devid=9)");
    }

    // ArduPilot Compass sensors
    fprintf(stderr, "[Group] ArduPilot Compass sensors\n");
    {
        auto r = dec.decode_device_id(7 << 16, "compass");
        CHECK(r.valid && r.name == "HMC5883L", "AP HMC5883L (devid=7)");
    }
    {
        auto r = dec.decode_device_id(9 << 16, "compass");
        CHECK(r.valid && r.name == "AK09916", "AP AK09916 (devid=9)");
    }
    {
        auto r = dec.decode_device_id(10 << 16, "compass");
        CHECK(r.valid && r.name == "IST8310", "AP IST8310 (devid=10)");
    }
    {
        auto r = dec.decode_device_id(11 << 16, "compass");
        CHECK(r.valid && r.name == "ICM20948_Mag", "AP ICM20948_Mag (devid=11)");
    }
    {
        auto r = dec.decode_device_id(8 << 16, "compass");
        CHECK(r.valid && r.name == "LIS3MDL", "AP LIS3MDL (devid=8)");
    }

    // ArduPilot Baro sensors
    fprintf(stderr, "[Group] ArduPilot Baro sensors\n");
    {
        auto r = dec.decode_device_id(3 << 16, "baro");
        CHECK(r.valid && r.name == "BMP280", "AP BMP280 (devid=3)");
    }
    {
        auto r = dec.decode_device_id(11 << 16, "baro");
        CHECK(r.valid && r.name == "MS5611", "AP MS5611 (devid=11)");
    }
    {
        auto r = dec.decode_device_id(20 << 16, "baro");
        CHECK(r.valid && r.name == "BMP390", "AP BMP390 (devid=20)");
    }

    // Auto-detect firmware from ID pattern
    fprintf(stderr, "[Group] Auto-detect firmware\n");
    {
        auto r = dec.decode_device_id(0x90000118, "imu");
        CHECK(r.detected_firmware == mg::core::sensor::SensorIdFirmware::PX4, "large ID detected as PX4");
    }
    {
        auto r = dec.decode_device_id(56 << 16, "ins");
        CHECK(r.detected_firmware == mg::core::sensor::SensorIdFirmware::ArduPilot, "small ID detected as AP");
    }

    // Unknown sensor
    fprintf(stderr, "[Group] Unknown sensor\n");
    {
        auto r = dec.decode_device_id(static_cast<uint32_t>(99999u << 16), "ins");
        CHECK(!r.valid || r.name.empty(), "unknown devid returns invalid/empty");
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}