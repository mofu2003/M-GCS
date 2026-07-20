#include "core/sensor/sensor_id_decoder.h"
#include "core/platform/platform_filesystem.h"
#include <cstdio>
#include <string>

int main() {
    fprintf(stderr, "=== PX4 Sensor Database Test (Offline) ===\n\n");
    
    std::string db_path = mg::core::platform::find_data_file("SensorDeviceInfo.json");
    fprintf(stderr, "DB path: %s\n", db_path.c_str());
    
    mg::core::sensor::SensorIdDecoder decoder(db_path);
    fprintf(stderr, "Loaded: %s\n\n", decoder.is_loaded() ? "YES" : "NO");
    
    // Test PX4 known devtypes
    struct TestCase { int devtype; const char* category; const char* expected; };
    TestCase px4_tests[] = {
        {144, "imu", "ICM42688-P"},   // PX4 devtype for ICM42688-P
        {176, "imu", "BMI270"},        // BMI270
        {136, "imu", "ICM20602"},      // ICM20602
        {96,  "imu", "MPU6000"},       // MPU6000
        {97,  "imu", "MPU9250"},       // MPU9250
        {105, "imu", "BMI160"},        // BMI160
        {121, "imu", "BMI088"},        // BMI088
        {173, "imu", "ICM42670P"},     // ICM42670P
        {196, "mag", "LIS3MDL"},       // LIS3MDL
        {200, "mag", "AK09916"},       // AK09916
        {114, "mag", "QMC5883L"},      // QMC5883L
        {97,  "mag", "HMC5883"},       // HMC5883
        {50,  "baro", "MS5611"},       // MS5611
        {48,  "baro", "BMP280"},       // BMP280
        {177, "baro", "BMP388"},       // BMP388
        {178, "baro", "BMP390"},       // BMP390
        {52,  "baro", "DPS310"},       // DPS310
        {13,  "diff_pres", "MS4525DO"}, // MS4525DO airspeed
    };
    
    int pass = 0, fail = 0;
    for (auto& tc : px4_tests) {
        auto r = decoder.decode_px4_type_value(tc.devtype, tc.category);
        bool ok = r.valid && r.name == tc.expected;
        fprintf(stderr, "  [%s] PX4 devtype=%3d cat=%-12s -> %-20s (expected %-16s) %s\n",
                ok ? "PASS" : "FAIL", tc.devtype, tc.category,
                r.valid ? r.name.c_str() : "NOT_FOUND",
                tc.expected,
                ok ? "" : r.valid ? "(MISMATCH)" : "(NOT IN DB)");
        if (ok) pass++; else fail++;
    }
    
    // Test ArduPilot known devtypes (backward compatibility)
    fprintf(stderr, "\n--- ArduPilot backward compatibility ---\n");
    TestCase ardu_tests[] = {
        {56,  "ins", "BMI270"},
        {52,  "ins", "ICM42688"},
        {43,  "ins", "BMI088"},
        {9,   "ins", "BMI160"},
        {7,   "compass", "HMC5883L"},
        {9,   "compass", "AK09916"},
        {10,  "compass", "IST8310"},
        {11,  "compass", "ICM20948_Mag"},
        {3,   "baro", "BMP280"},
        {11,  "baro", "MS5611"},
        {20,  "baro", "BMP390"},
    };
    
    for (auto& tc : ardu_tests) {
        auto r = decoder.decode_device_id(tc.devtype << 16, tc.category);
        bool ok = r.valid && r.name == tc.expected;
        fprintf(stderr, "  [%s] Ardu devid=%-10d cat=%-12s -> %-20s (expected %-16s) %s\n",
                ok ? "PASS" : "FAIL", tc.devtype << 16, tc.category,
                r.valid ? r.name.c_str() : "NOT_FOUND",
                tc.expected,
                ok ? "" : r.valid ? "(MISMATCH)" : "(NOT IN DB)");
        if (ok) pass++; else fail++;
    }
    
    // Test auto-detect
    fprintf(stderr, "\n--- Auto-detect (large ID -> PX4, small -> AP) ---\n");
    {
        auto r1 = decoder.decode_device_id(0x90000118, "imu");  // PX4: devtype=144=ICM42688-P
        fprintf(stderr, "  Auto 0x90000118 imu: %s (name=%s, fw=%s)\n",
                r1.valid ? "OK" : "FAIL", r1.name.c_str(),
                r1.detected_firmware == mg::core::sensor::SensorIdFirmware::PX4 ? "PX4" : "AP");
        
        auto r2 = decoder.decode_device_id(56 << 16, "ins");  // AP: devtype=56=BMI270
        fprintf(stderr, "  Auto %d ins: %s (name=%s, fw=%s)\n",
                56 << 16, r2.valid ? "OK" : "FAIL", r2.name.c_str(),
                r2.detected_firmware == mg::core::sensor::SensorIdFirmware::PX4 ? "PX4" : "AP");
    }
    
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail;
}
