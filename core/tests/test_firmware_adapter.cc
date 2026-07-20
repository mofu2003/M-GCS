#include "core/firmware/firmware_adapter.h"

#include <cstdio>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { g_passed++; fprintf(stderr, "  [PASS] %s\n", name); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", name); } \
} while (0)

using mg::core::firmware::FirmwareAdapter;

static void test_ardupilot_vehicle_families() {
    fprintf(stderr, "\n[Group] ArduPilot normalized vehicle families\n");

    FirmwareAdapter plane;
    plane.detect(3, 1);
    CHECK("AP plane firmware", plane.firmware_name() == "ArduPilot");
    CHECK("AP plane normalized family", plane.normalized_vehicle_name() == "FixedWing");

    FirmwareAdapter tricopter;
    tricopter.detect(3, 15);
    CHECK("AP tricopter normalized family", tricopter.normalized_vehicle_name() == "Copter");

    FirmwareAdapter boat;
    boat.detect(3, 11);
    CHECK("AP boat normalized family", boat.normalized_vehicle_name() == "Rover");

    FirmwareAdapter sub;
    sub.detect(3, 12);
    CHECK("AP submarine normalized family", sub.normalized_vehicle_name() == "Submarine");

    FirmwareAdapter vtol;
    vtol.detect(3, 19);
    CHECK("AP VTOL normalized family", vtol.normalized_vehicle_name() == "VTOL");

    FirmwareAdapter tracker;
    tracker.detect(3, 5);
    CHECK("AP tracker normalized family", tracker.normalized_vehicle_name() == "AntennaTracker");
}

static void test_px4_vehicle_families() {
    fprintf(stderr, "\n[Group] PX4 normalized vehicle families\n");

    FirmwareAdapter quad;
    quad.detect(12, 2);
    CHECK("PX4 quad firmware", quad.firmware_name() == "PX4");
    CHECK("PX4 quad normalized family", quad.normalized_vehicle_name() == "Copter");

    FirmwareAdapter plane;
    plane.detect(12, 1);
    CHECK("PX4 plane normalized family", plane.normalized_vehicle_name() == "FixedWing");

    FirmwareAdapter boat;
    boat.detect(12, 11);
    CHECK("PX4 boat normalized family", boat.normalized_vehicle_name() == "Rover");

    FirmwareAdapter sub;
    sub.detect(12, 12);
    CHECK("PX4 submarine normalized family", sub.normalized_vehicle_name() == "Submarine");

    FirmwareAdapter vtol;
    vtol.detect(12, 20);
    CHECK("PX4 VTOL normalized family", vtol.normalized_vehicle_name() == "VTOL");
}

int main() {
    fprintf(stderr, "=== Firmware Adapter Classification Tests ===\n");

    test_ardupilot_vehicle_families();
    test_px4_vehicle_families();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
