#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_vehicle_session.h"
#include "mock_vehicle_test_env.h"

#include <cstdio>


static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static void test_fallback_session_is_available() {
    fprintf(stderr, "\n[Group] fallback session\n");
    MockVehicleTestEnv env;

    auto session = mg::bridge::ffi::resolve_vehicle_session(env.core);
    CHECK(session.vehicle != nullptr, "fallback vehicle is available");
    CHECK(session.mavlink != nullptr, "fallback mavlink is available");
    CHECK(session.connected == false, "fallback session starts disconnected");
}

static void test_live_vehicle_session_wins_when_selected() {
    fprintf(stderr, "\n[Group] live session routing\n");
    MockVehicleTestEnv env;

    auto selected_vehicle = std::make_shared<mg::core::vehicle::Vehicle>(7);
    env.core->vehicles.add_for_test(selected_vehicle);
    env.core->vehicles.set_active_vehicle(7);

    auto* selected = env.core->vehicles.get_vehicle(7);
    CHECK(selected != nullptr, "selected vehicle exists");

    auto session = mg::bridge::ffi::resolve_vehicle_session(env.core);
    CHECK(session.vehicle == selected, "selected live vehicle wins over fallback");
}

static void test_vehicle_registry_does_not_implicitly_change_focus() {
    fprintf(stderr, "\n[Group] vehicle registry focus isolation\n");
    MockVehicleTestEnv env;

    auto first = std::make_shared<mg::core::vehicle::Vehicle>(7);
    auto second = std::make_shared<mg::core::vehicle::Vehicle>(8);
    env.core->vehicles.add_for_test(first);
    env.core->vehicles.add_for_test(second);

    CHECK(mg_vehicle_get_active(env.core) == -1,
          "registering vehicle sessions does not select an active vehicle");

    env.core->vehicles.set_active_vehicle(7);
    CHECK(mg_vehicle_get_active(env.core) == 7,
          "active vehicle changes only after explicit selection");
    env.core->vehicles.add_for_test(std::make_shared<mg::core::vehicle::Vehicle>(9));
    CHECK(mg_vehicle_get_active(env.core) == 7,
          "registering another session does not steal existing focus");

    env.core->vehicles.close_vehicle(7);
    CHECK(mg_vehicle_get_active(env.core) == -1,
          "closing the focused vehicle clears focus instead of selecting another");
}

int main() {
    fprintf(stderr, "=== Bridge Vehicle Session Tests ===\n");
    test_fallback_session_is_available();
    test_live_vehicle_session_wins_when_selected();
    test_vehicle_registry_does_not_implicitly_change_focus();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
