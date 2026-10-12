#ifndef MOCK_VEHICLE_TEST_ENV_H
#define MOCK_VEHICLE_TEST_ENV_H

#include "mock_flight_controller.h"
#include "ffi_api.h"
#include "ffi_internal.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

struct MockVehicleTestEnv {
    mg_core_t* core = nullptr;
    mg::core::transport::InMemoryLink* link = nullptr;
    MockFlightController* mock = nullptr;
    mg::core::mavlink::MavlinkAdapter* mav = nullptr;
    std::atomic<bool> pumping{false};
    std::thread pump_thread;

    explicit MockVehicleTestEnv(
        const MockFlightController::Config& cfg = MockFlightController::Config(),
        bool load_default_params = false,
        const std::function<void(mg::core::mavlink::MavlinkAdapter&, mg_core_t*)>& configure_mav = {}) {
        core = mg_create();

        auto link_up = std::make_unique<mg::core::transport::InMemoryLink>();
        link_up->open();
        auto mav_up = std::make_unique<mg::core::mavlink::MavlinkAdapter>();

        auto* link_raw = link_up.get();
        auto* mav_raw = mav_up.get();

        mav_raw->set_send_callback([link_raw](const uint8_t* data, size_t size) -> bool {
            return link_raw->send(data, size);
        });
        link_raw->set_receive_callback([mav_raw](const uint8_t* data, size_t size) {
            mav_raw->feed_raw(data, size);
        });

        if (configure_mav) {
            configure_mav(*mav_raw, core);
        }

        core->fallback_vehicle->conn().wire_for_test(std::move(link_up), std::move(mav_up));
        core->fallback_vehicle->init_subsystems();
        core->fallback_vehicle->wire_telemetry_callbacks();
        link = link_raw;
        mav = mav_raw;
        mock = new MockFlightController(*link, cfg);
        if (load_default_params) {
            mock->load_default_params();
        }
    }

    ~MockVehicleTestEnv() {
        stop_pump();
        delete mock;
        mg_destroy(core);
    }

    void tick(int ms, int drain_limit = 20) {
        mock->tick(ms);
        for (int drain = 0; drain < drain_limit; drain++) {
            auto out = link->take_outgoing();
            if (out.empty()) break;
            mock->on_gcs_data(out.data(), out.size());
        }
    }

    bool wait_connected(int timeout_ms = 5000, int tick_ms = 100) {
        auto start = std::chrono::steady_clock::now();
        while (!core->fallback_vehicle->conn().is_connected()) {
            tick(tick_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) return false;
        }
        return true;
    }

    void start_pump(int tick_ms = 1, int sleep_ms = 1) {
        if (pumping.load()) return;
        pumping = true;
        pump_thread = std::thread([this, tick_ms, sleep_ms]() {
            while (pumping.load()) {
                tick(tick_ms, 20);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
        });
    }

    void stop_pump() {
        pumping = false;
        if (pump_thread.joinable()) pump_thread.join();
    }

    char* call_with_pump(char* (*fn)(mg_core_t*), int tick_ms = 1, int sleep_ms = 1) {
        start_pump(tick_ms, sleep_ms);
        char* result = fn(core);
        stop_pump();
        return result;
    }
};

#endif
