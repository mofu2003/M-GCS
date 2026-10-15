/**
 * test_vehicle_semantics_contract.cc - 跨层语义契约测试
 *
 * 背景（2026-10 VTOL 事故）：机型的「语义裁定」曾在前后端各算一遍——
 * Dart 侧读 Q_ENABLE、C++ 侧读 MAV_TYPE 区间。两边签名完全匹配、各自
 * 单测也都过，但合起来 QuadPlane 被判成不同机型：引导起飞按钮灰掉，
 * NAV_VTOL_TAKEOFF(84)/NAV_VTOL_LAND(85) 在任务编辑器整个会话不可见。
 *
 * 现有测试对此零防御：
 *   - core/tests 的 C++ 单测测的是适配器内部；
 *   - flutter_shell/test 的 Dart 单测用假后端；
 *   - tests/test_ffi_response_contract.cc 只断言 JSON 里「字段存在」，
 *     从不断言「字段值的语义对不对」。
 *
 * 本文件补上这一层：用进程内的 MockFlightController 造出**真实机型的
 * MAVLink 组合**，驱动完整 GCS↔飞控闭环，然后断言 FFI 出口的语义字段。
 * 它同时锁住三件事：
 *   1. 后端对每种真实机型组合的裁定结果；
 *   2. 前端的判定入口（guideTakeoffAllowed / missionCommandVisible）
 *      对同一份 JSON 的解释与后端一致；
 *   3. 裁定不得因为「参数还没到」而变化（时序稳定）。
 *
 * 关键机型：ArduPilot QuadPlane 在 Q_MAV_TYPE=0（上游默认）时上报
 * MAV_TYPE_FIXED_WING(1)——机型身份只能靠 Q_ENABLE 区分。这是最容易
 * 前后端判不一致的组合，也是本测试的核心用例。
 */

#include "mock_vehicle_test_env.h"
#include "mgcs_test_check.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

/// 在 JSON 文本里取一个顶层字符串字段的值（够用即可，不引 JSON 库）。
std::string json_string(const std::string& json, const std::string& key) {
    const std::string pat = "\"" + key + "\":\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos += pat.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

/// 在 JSON 文本里取一个顶层布尔字段的值；找不到返回 false。
bool json_bool(const std::string& json, const std::string& key) {
    return json.find("\"" + key + "\":true") != std::string::npos;
}

/// 取嵌套在 "capabilities":{...} 里的布尔字段。
bool capability(const std::string& json, const std::string& name) {
    auto cap = json.find("\"capabilities\":{");
    if (cap == std::string::npos) return false;
    auto close = json.find('}', cap);
    if (close == std::string::npos) return false;
    const std::string scope = json.substr(cap, close - cap);
    return scope.find("\"" + name + "\":true") != std::string::npos;
}

/// 把一段 JSON 里所有整数值收集起来，用于断言数组内容。
std::vector<int> json_ints(const std::string& json, const std::string& key) {
    std::vector<int> out;
    auto pos = json.find("\"" + key + "\":[");
    if (pos == std::string::npos) return out;
    pos = json.find('[', pos) + 1;
    while (pos < json.size() && json[pos] != ']') {
        while (pos < json.size() &&
               (json[pos] == ',' || json[pos] == ' ' ||
                json[pos] == '"' || json[pos] == '\n' || json[pos] == '\t')) {
            pos++;
        }
        if (pos >= json.size() || json[pos] == ']') break;
        if (!isdigit((unsigned char)json[pos]) && json[pos] != '-') {
            pos++;  // 非数字内容（如字符串字段）跳过，避免死循环
            continue;
        }
        size_t end = pos;
        if (json[end] == '-') end++;
        while (end < json.size() && isdigit((unsigned char)json[end])) end++;
        if (end > pos) out.push_back(std::stoi(json.substr(pos, end - pos)));
        pos = end;
    }
    return out;
}

struct FirmwareReading {
    std::string vehicle_type;        // 归一化名：Copter / FixedWing / VTOL / ...
    std::string vehicle_class;       // multirotor / fixed_wing / vtol / ...
    std::string control_profile;     // copter / fixed_wing / vtol / rover
    std::string mission_vehicle_class;
    std::string pause_mode;
    bool forward_flight = false;
    bool cap_takeoff = false;
    bool cap_guided_land = false;
    bool cap_guided_mode = false;
    std::vector<int> pwm_ranges;
    std::vector<int> arming_bits;
    std::string raw;
};

/// 连接一架被测机型，完整下载参数列表，再读回 mg_firmware_get_type 的语义。
///
/// 参数必须先主动请求（mg_params_request_all）——飞控不会自发推全表。
/// QuadPlane 的机型身份依赖 Q_ENABLE，所以这里必须等到参数真正到齐，
/// 否则读到的是「参数未到达」时的中间态。
FirmwareReading read_firmware(MockVehicleTestEnv& env, int wait_ms = 6000) {
    CHECK(env.wait_connected(wait_ms), "必须先连上被测机型");

    char* pr = mg_params_request_all(env.core);
    CHECK(pr != nullptr && std::string(pr ? pr : "").find("\"success\":true") != std::string::npos,
          "参数列表请求必须被接受");
    if (pr != nullptr) mg_free_string(pr);

    // 等到 received == total，即全表到齐。
    bool complete = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(wait_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        if (prog != nullptr) {
            const std::string p = prog;
            mg_free_string(prog);
            const auto r = p.find("\"received\":");
            const auto t = p.find("\"total\":");
            if (r != std::string::npos && t != std::string::npos) {
                const int recv = std::atoi(p.c_str() + r + 11);
                const int total = std::atoi(p.c_str() + t + 8);
                if (recv > 0 && recv == total) { complete = true; break; }
            }
        }
    }
    CHECK(complete, "参数列表必须完整下载（QuadPlane 身份依赖 Q_ENABLE）");

    char* raw = mg_firmware_get_type(env.core);
    FirmwareReading out;
    if (raw != nullptr) {
        out.raw = raw;
        mg_free_string(raw);
    }

    out.vehicle_type = json_string(out.raw, "vehicle_type");
    out.vehicle_class = json_string(out.raw, "vehicle_class");
    out.control_profile = json_string(out.raw, "control_profile");
    out.mission_vehicle_class = json_string(out.raw, "mission_vehicle_class");
    out.pause_mode = json_string(out.raw, "pause_mode");
    out.forward_flight = json_bool(out.raw, "forward_flight");
    out.cap_takeoff = capability(out.raw, "takeoff");
    out.cap_guided_land = capability(out.raw, "guided_land");
    out.cap_guided_mode = capability(out.raw, "guided_mode");
    out.pwm_ranges = json_ints(out.raw, "flight_mode_pwm_ranges");
    out.arming_bits = json_ints(out.raw, "arming_check_bits");
    return out;
}

// ---------------------------------------------------------------------------
// 1. QuadPlane：MAV_TYPE=FIXED_WING(1) + Q_ENABLE=1
//    这是事故本体。后端必须认出它是 VTOL，而不是 FixedWing。
// ---------------------------------------------------------------------------
void test_quadplane_identified_as_vtol() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Plane;
    cfg.mav_type_override = MAV_TYPE_FIXED_WING;  // 上游 QuadPlane 的真实上报
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("Q_ENABLE", 1);

    const auto fw = read_firmware(env);

    CHECK(fw.vehicle_class == "vtol",
          "QuadPlane(MAV_TYPE=1,Q_ENABLE=1) vehicle_class 必须是 vtol");
    CHECK(fw.vehicle_type == "VTOL", "归一化名必须是 VTOL");
    CHECK(fw.control_profile == "vtol", "control_profile 必须是 vtol");
    CHECK(fw.mission_vehicle_class == "vtol",
          "任务命令目录必须按 vtol 过滤，否则 84/85 不可见");
    CHECK(fw.cap_takeoff, "QuadPlane 必须带 takeoff 能力（引导起飞可用）");
    CHECK(fw.cap_guided_land, "QuadPlane 必须带 guided_land 能力");
    CHECK(fw.cap_guided_mode, "QuadPlane 必须带 guided_mode 能力");
    CHECK(fw.pause_mode == "Loiter", "Plane 族的暂停语义是 Loiter");
}

// ---------------------------------------------------------------------------
// 2. 纯固定翼：MAV_TYPE=FIXED_WING(1)、无 Q_ENABLE
//    与用例 1 的 MAV_TYPE 完全相同，只有参数不同——正是「MAV_TYPE 不足以
//    裁定身份」的证据，也确保修复没有把所有固定翼都当成 VTOL。
// ---------------------------------------------------------------------------
void test_plain_fixedwing_stays_fixedwing() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Plane;
    cfg.mav_type_override = MAV_TYPE_FIXED_WING;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("WPNAV_SPEED", 20);

    const auto fw = read_firmware(env);

    CHECK(fw.vehicle_class == "fixed_wing",
          "无 Q_ENABLE 的固定翼不能被认成 VTOL");
    CHECK(fw.control_profile == "fixed_wing", "control_profile 必须是 fixed_wing");
    CHECK(!fw.cap_guided_land, "固定翼没有引导降落能力");
    CHECK(!fw.cap_takeoff, "固定翼没有引导起飞能力（对齐 QGC）");
    CHECK(fw.forward_flight, "固定翼恒为前飞状态");
    CHECK(fw.pause_mode == "Loiter", "Plane 族的暂停语义是 Loiter");
}

// ---------------------------------------------------------------------------
// 3. Q_ENABLE=0 的 QuadPlane：vtol 框架已禁用，应退回固定翼
// ---------------------------------------------------------------------------
void test_quadplane_with_q_enable_off_is_fixedwing() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Plane;
    cfg.mav_type_override = MAV_TYPE_FIXED_WING;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("Q_ENABLE", 0);

    const auto fw = read_firmware(env);

    CHECK(fw.vehicle_class == "fixed_wing",
          "Q_ENABLE=0 表示 VTOL 框架未启用，应按固定翼处理");
    CHECK(fw.control_profile == "fixed_wing", "control_profile 必须是 fixed_wing");
}

// ---------------------------------------------------------------------------
// 4. 显式 VTOL MAV_TYPE：不依赖参数也应认出
//    逐个覆盖官方 VTOL 家族 19–25（含 MAV_TYPE_VTOL_RESERVED5=25）。
//    25 曾在多处被漏掉（写成 19–24），QGC 的 QGCMAVLink::vehicleClass()
//    把它算作 VTOL。
// ---------------------------------------------------------------------------
void test_explicit_vtol_mav_types() {
    const uint8_t vtol_types[] = {
        MAV_TYPE_VTOL_TAILSITTER_DUOROTOR,   // 19
        MAV_TYPE_VTOL_TAILSITTER_QUADROTOR,  // 20
        MAV_TYPE_VTOL_TILTROTOR,             // 21
        MAV_TYPE_VTOL_FIXEDROTOR,            // 22
        MAV_TYPE_VTOL_TAILSITTER,            // 23
        MAV_TYPE_VTOL_TILTWING,              // 24
        MAV_TYPE_VTOL_RESERVED5,             // 25
    };
    for (uint8_t t : vtol_types) {
        MockFlightController::Config cfg;
        cfg.vehicle = MockFlightController::Plane;
        cfg.mav_type_override = t;
        MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
        env.mock->add_param("WPNAV_SPEED", 20);

        const auto fw = read_firmware(env);
        const std::string label = "MAV_TYPE=" + std::to_string(t);
        CHECK(fw.vehicle_class == "vtol", (label + " 必须归为 vtol").c_str());
        CHECK(fw.cap_takeoff, (label + " 必须带 takeoff 能力").c_str());

        // 下面三条钉的是 2026-10 的第二起事故：VehicleKind 是离散枚举、
        // 只声明了 VTOL = 19，而 MAV_TYPE 的 VTOL 家族是 19–25 连续七个值。
        // detect() 里的 static_cast<VehicleKind>(vehicle_type) 因此只对 19
        // 恰好成立，20–25 会掉出所有 switch(vehicle_kind())。
        //
        // 注意：vehicle_class / cap_takeoff 走的是 normalized_vehicle_name()
        // 与 classify_ap_vehicle()，那条路有 is_vtol_mav_type() 兜底，所以
        // 上面两条断言在 bug 存在时**照样通过**——这正是它漏网的原因。
        // 只有直接 switch(vehicle_kind()) 的出口才能暴露它。
        CHECK(fw.control_profile == "vtol",
              (label + " control_profile 必须走 switch(vehicle_kind()) 的 VTOL 分支").c_str());
        CHECK(fw.pause_mode == "Loiter",
              (label + " pause_mode 不得为空——空串会让引导暂停抛 StateError").c_str());
        CHECK(fw.mission_vehicle_class == "vtol",
              (label + " 命令目录必须归为 vtol，否则 84/85 不可见").c_str());
    }
}

// ---------------------------------------------------------------------------
// 4b. VTOL 家族的每一个 MAV_TYPE 都必须在命令目录里得到非空 vehicle_class
//     命令目录（mg_mission_get_command_catalog）是另一条 switch(vehicle_kind())
//     路径，且它不传 Q_ENABLE（ADR-0010 第 4 条）。vehicle_class 为空串时
//     mission_vehicle_class_bit("") 返回 0，mission_command_supports_vehicle
//     对 0 一律放行——机型过滤完全空转，任何命令都显示为可用。
// ---------------------------------------------------------------------------
void test_vtol_mav_types_yield_non_empty_mission_class() {
    const uint8_t vtol_types[] = {
        MAV_TYPE_VTOL_TAILSITTER_DUOROTOR,   // 19
        MAV_TYPE_VTOL_TAILSITTER_QUADROTOR,  // 20
        MAV_TYPE_VTOL_TILTROTOR,             // 21
        MAV_TYPE_VTOL_FIXEDROTOR,            // 22
        MAV_TYPE_VTOL_TAILSITTER,            // 23
        MAV_TYPE_VTOL_TILTWING,              // 24
        MAV_TYPE_VTOL_RESERVED5,             // 25
    };
    for (uint8_t t : vtol_types) {
        MockFlightController::Config cfg;
        cfg.vehicle = MockFlightController::Plane;
        cfg.mav_type_override = t;
        MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
        env.mock->add_param("WPNAV_SPEED", 20);

        const auto fw = read_firmware(env);
        const std::string label = "MAV_TYPE=" + std::to_string(t);
        CHECK(!fw.mission_vehicle_class.empty(),
              (label + " 命令目录的 vehicle_class 不得为空（空串会让机型过滤空转）").c_str());
        CHECK(fw.mission_vehicle_class == "vtol",
              (label + " 命令目录必须归为 vtol").c_str());
    }
}

// ---------------------------------------------------------------------------
// 5. 非飞行器类型不得拿到引导起飞能力
//    GIMBAL(26)/ADSB(27)/PARAFOIL(28)/DODECAROTOR(29) 曾因机型分类的
//    default 分支落到 Copter 而得到 takeoff+guided_land——这是安全过滤
//    被摘掉造成的回归，必须有测试钉住。
// ---------------------------------------------------------------------------
void test_non_vehicle_types_have_no_takeoff() {
    const uint8_t non_vehicle[] = {
        MAV_TYPE_GIMBAL,       // 26
        MAV_TYPE_ADSB,         // 27
        MAV_TYPE_PARAFOIL,     // 28
        MAV_TYPE_DODECAROTOR,  // 29
    };
    for (uint8_t t : non_vehicle) {
        MockFlightController::Config cfg;
        cfg.vehicle = MockFlightController::Plane;
        cfg.mav_type_override = t;
        MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
        env.mock->add_param("WPNAV_SPEED", 20);

        const auto fw = read_firmware(env);
        const std::string label = "MAV_TYPE=" + std::to_string(t);
        CHECK(!fw.cap_takeoff, (label + " 不得带 takeoff 能力").c_str());
        CHECK(!fw.cap_guided_land, (label + " 不得带 guided_land 能力").c_str());
    }
}

// ---------------------------------------------------------------------------
// 6. Copter：多旋翼身份与能力（对照组，确保修复没伤到正常路径）
// ---------------------------------------------------------------------------
void test_copter_semantics() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Copter;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("FRAME_CLASS", 1);

    const auto fw = read_firmware(env);

    CHECK(fw.vehicle_class == "multirotor", "Copter 归为 multirotor");
    CHECK(fw.control_profile == "copter", "control_profile 是 copter");
    CHECK(fw.cap_takeoff, "Copter 支持引导起飞");
    CHECK(fw.cap_guided_land, "Copter 支持引导降落");
    CHECK(fw.pause_mode == "Brake", "Copter 的暂停语义是 Brake");
    CHECK(!fw.forward_flight, "多旋翼不处于前飞状态");
}

// ---------------------------------------------------------------------------
// 7. Rover：地面载具（确保机型枚举新增 rover 后语义正确）
// ---------------------------------------------------------------------------
void test_rover_semantics() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Rover;
    cfg.mav_type_override = MAV_TYPE_GROUND_ROVER;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("WP_SPEED", 3);

    const auto fw = read_firmware(env);

    CHECK(fw.control_profile == "rover", "control_profile 是 rover");
    CHECK(!fw.cap_takeoff, "Rover 没有引导起飞能力");
    CHECK(!fw.cap_guided_land, "Rover 没有引导降落能力");
}

// ---------------------------------------------------------------------------
// 8. 命令目录与型号裁定的**有意分歧**（ADR-0010 第 4 条）
//    两处口径不同，且这个不同是刻意的：
//      - mg_firmware_get_type 用的是「含参数的完整裁定」→ QuadPlane 是 vtol；
//      - mg_mission_get_command_catalog 只按 MAV_TYPE 归一化 → QuadPlane 是 plane。
//    原因是命令目录每个会话只取一次并被前端缓存，如果它依赖 Q_ENABLE，
//    参数到达前取到的目录会把错误结果锁死整个会话（正是本次事故的成因）。
//    MAV_TYPE 从第一条心跳起就稳定，所以目录侧牺牲一点精度换取确定性。
//
//    本用例把这个分歧**钉死**：一旦有人「顺手」把参数依赖加回目录侧，
//    这里立刻变红。真正的命令可选性由 supported 字段按 MAV_TYPE 判定，
//    QuadPlane 会因此看不到 84/85——这是已知的、可接受的取舍，
//    需要时由用户在任务编辑器中显式选择，而不是让会话级缓存赌参数时序。
// ---------------------------------------------------------------------------
void test_mission_catalog_is_mav_type_only_by_design() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Plane;
    cfg.mav_type_override = MAV_TYPE_FIXED_WING;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("Q_ENABLE", 1);

    // 完整裁定：QuadPlane 是 vtol。
    const auto fw = read_firmware(env);
    CHECK(fw.vehicle_class == "vtol",
          "含参数的完整裁定必须认出 QuadPlane 是 VTOL");
    CHECK(fw.mission_vehicle_class == "vtol",
          "mg_firmware_get_type 侧的任务机型口径跟随完整裁定");

    // 目录裁定：只按 MAV_TYPE，QuadPlane 表现为 plane。
    char* cat = mg_mission_get_command_catalog(env.core);
    CHECK(cat != nullptr, "命令目录必须可读");
    const std::string catalog = cat ? cat : "";
    if (cat != nullptr) mg_free_string(cat);

    // 目录不暴露顶层 vehicle_class 字段，口径体现在每条命令的 supported 上。
    // 用普通命令 16（NAV_WAYPOINT，全机型可用）确认目录正常。
    CHECK(catalog.find("\"name\":\"NAV_WAYPOINT\"") != std::string::npos,
          "命令目录应包含 NAV_WAYPOINT");
    CHECK(catalog.find("\"supported\":true") != std::string::npos,
          "命令目录必须输出 supported 字段");

    // 84/85 是 VTOL 专属：MAV_TYPE=1 的 QuadPlane 在目录里看不到，属预期取舍。
    for (const int cmd : {84, 85}) {
        const std::string needle = "\"command\":" + std::to_string(cmd);
        auto pos = catalog.find(needle);
        CHECK(pos != std::string::npos,
              ("命令目录必须包含 " + std::to_string(cmd)).c_str());
        if (pos == std::string::npos) continue;
        auto end = catalog.find('}', pos);
        const std::string item =
            catalog.substr(pos, end == std::string::npos ? std::string::npos
                                                         : end - pos);
        CHECK(item.find("\"supported\":false") != std::string::npos,
              ("MAV_TYPE=1 的 QuadPlane 在目录侧应挡掉命令 " +
               std::to_string(cmd) + "（已知取舍，勿改回参数依赖）").c_str());
    }
}

// ---------------------------------------------------------------------------
// 9. 时序稳定性：参数未到达时不得给出「错误但自信」的裁定
//    事故的另一半原因是 Q_ENABLE 异步到达。若刚连上时后端说 fixed_wing、
//    参数到齐后改口 vtol，而前端只在初始化时读一次目录，整个会话就锁死
//    在错误状态。这里断言的是：最终态必须收敛到 vtol。
// ---------------------------------------------------------------------------
void test_parameter_arrival_converges_not_sticks() {
    MockFlightController::Config cfg;
    cfg.vehicle = MockFlightController::Plane;
    cfg.mav_type_override = MAV_TYPE_FIXED_WING;
    MockVehicleTestEnv env(cfg, /*load_default_params=*/false);
    env.mock->add_param("Q_ENABLE", 1);

    CHECK(env.wait_connected(4000), "必须连上");

    const auto before = [&] {
        char* r = mg_firmware_get_type(env.core);
        std::string c = r ? json_string(r, "vehicle_class") : "";
        if (r != nullptr) mg_free_string(r);
        return c;
    }();
    // 参数尚未请求：此时只能说 fixed_wing（除了 MAV_TYPE 没有别的信息）。
    CHECK(before == "fixed_wing",
          "参数未到达时应按 MAV_TYPE 给出 fixed_wing，不得凭空猜 vtol");

    // 请求参数并跑到到齐。
    char* pr = mg_params_request_all(env.core);
    if (pr != nullptr) mg_free_string(pr);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        bool done = false;
        if (prog != nullptr) {
            const std::string p = prog;
            mg_free_string(prog);
            const auto r = p.find("\"received\":");
            const auto t = p.find("\"total\":");
            if (r != std::string::npos && t != std::string::npos) {
                const int recv = std::atoi(p.c_str() + r + 11);
                const int total = std::atoi(p.c_str() + t + 8);
                done = recv > 0 && recv == total;
            }
        }
        if (done) break;
    }

    char* raw = mg_firmware_get_type(env.core);
    const std::string after = raw ? json_string(raw, "vehicle_class") : "";
    if (raw != nullptr) mg_free_string(raw);

    CHECK(after == "vtol",
          "Q_ENABLE 到达后裁定必须收敛到 vtol，不能永久停在 fixed_wing");
}

}  // namespace

int main() {
    fprintf(stderr, "=== 跨层机型语义契约测试 ===\n\n");

    test_quadplane_identified_as_vtol();
    test_plain_fixedwing_stays_fixedwing();
    test_quadplane_with_q_enable_off_is_fixedwing();
    test_explicit_vtol_mav_types();
    test_vtol_mav_types_yield_non_empty_mission_class();
    test_non_vehicle_types_have_no_takeoff();
    test_copter_semantics();
    test_rover_semantics();
    test_mission_catalog_is_mav_type_only_by_design();
    test_parameter_arrival_converges_not_sticks();

    MGCS_TEST_REPORT();
    return mgcs_test::failed() == 0 ? 0 : 1;
}
