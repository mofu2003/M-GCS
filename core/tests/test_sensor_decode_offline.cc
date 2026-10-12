/**
 * test_sensor_decode_offline.cc - Comprehensive sensor ID decoder tests
 *
 * Tests all AP and PX4 sensor categories with known device IDs.
 * No hardware needed — purely offline database lookups.
 */
#include "core/sensor/sensor_id_decoder.h"
#include "core/platform/platform_filesystem.h"
#include "core/util/json.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


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

    // ---- Devtype table coverage ----
    // 解析器从手写字符串扫描改成 cJSON（core/util/json.h）后的守卫：逐条比对
    // 「JSON 里的条目数」与「解码器真能解出的条目数」。旧扫描器用 find('}') 取
    // 第一个 '}' 当条目结束、find_str 不认转义，遇到嵌套或转义会静默少读。
    fprintf(stderr, "[Group] Devtype table coverage\n");
    {
        std::ifstream input(db_path, std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        cJSON* root = mg::core::json::parse(buffer.str().c_str());
        CHECK(root != nullptr, "SensorDeviceInfo.json is valid JSON");

        struct TableSpec {
            const char* section;
            const char* category;
            bool px4;
            bool type_param;  // airspeed 走 decode_type_param（ParamEntry 风格）
        };
        const TableSpec tables[] = {
            {"ardupilot_imu_devtypes", "ins", false, false},
            {"ardupilot_compass_devtypes", "compass", false, false},
            {"ardupilot_baro_devtypes", "baro", false, false},
            {"ardupilot_airspeed_devtypes", "airspeed", false, true},
            {"px4_imu_devtypes", "imu", true, false},
            {"px4_mag_devtypes", "mag", true, false},
            {"px4_baro_devtypes", "baro", true, false},
            {"px4_differential_pressure_devtypes", "airspeed", true, false},
        };

        size_t total_entries = 0;
        size_t total_decoded = 0;
        for (const auto& table : tables) {
            const cJSON* section = cJSON_GetObjectItemCaseSensitive(root, table.section);
            const int expected = cJSON_IsObject(section) ? cJSON_GetArraySize(section) : 0;
            int decoded = 0;
            const cJSON* entry = nullptr;
            cJSON_ArrayForEach(entry, section) {
                if (entry->string == nullptr) continue;
                const long devtype = strtol(entry->string, nullptr, 0);
                bool ok = false;
                if (table.type_param) {
                    ok = dec.decode_type_param(static_cast<int>(devtype), table.category).valid;
                } else if (table.px4) {
                    ok = dec.decode_px4_type_value(static_cast<int>(devtype), table.category).valid;
                } else {
                    ok = dec.decode_device_id(static_cast<uint32_t>(devtype) << 16,
                                              table.category).valid;
                }
                if (ok) decoded++;
            }
            char message[160];
            snprintf(message, sizeof(message), "%s: %d/%d entries decoded",
                     table.section, decoded, expected);
            CHECK(expected > 0 && decoded == expected, message);
            total_entries += static_cast<size_t>(expected);
            total_decoded += static_cast<size_t>(decoded);
        }
        char total_message[160];
        snprintf(total_message, sizeof(total_message),
                 "8 devtype tables decoded completely: %zu/%zu entries",
                 total_decoded, total_entries);
        CHECK(total_entries > 0 && total_decoded == total_entries, total_message);
        cJSON_Delete(root);
    }

    // ---- Nested / escaped entries ----
    // 手写扫描器把「内层对象的第一个 '}'」当成条目结束、find_str 不认转义，
    // 所以嵌套对象后面的字段（sensor/family）会静默丢失，字符串里的 '}' 会把名字截断。
    // 换成 cJSON 后这两种输入都必须完整读出。
    fprintf(stderr, "[Group] Nested and escaped device table entries\n");
    {
        const auto fixture = std::filesystem::temp_directory_path() /
                             "mgcs_sensor_id_decoder_nested_fixture.json";
        {
            std::ofstream out(fixture, std::ios::binary | std::ios::trunc);
            out << R"({
  "ardupilot_imu_devtypes": {
    "1": {"name": "NESTED_IMU", "meta": {"vendor": "ACME"}, "sensor": "IMU", "family": "ACME"},
    "2": {"name": "BRACE}IMU", "sensor": "IMU", "family": "ACME"}
  }
}
)";
        }

        mg::core::sensor::SensorIdDecoder nested(fixture.string());
        CHECK(nested.is_loaded(), "nested fixture loads");

        const auto first = nested.decode_device_id(1u << 16, "ins");
        CHECK(first.valid && first.name == "NESTED_IMU", "nested object still yields the name");
        CHECK(first.sensor_type == "IMU", "field after a nested object is still read");
        CHECK(first.family == "ACME", "family after a nested object is still read");

        const auto second = nested.decode_device_id(2u << 16, "ins");
        CHECK(second.valid && second.name == "BRACE}IMU",
              "a '}' inside a string does not truncate the name");
        CHECK(second.sensor_type == "IMU",
              "field after a '}' inside a string is still read");

        std::error_code ignored;
        std::filesystem::remove(fixture, ignored);
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}