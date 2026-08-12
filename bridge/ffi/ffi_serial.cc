#define MGCORE_EXPORTS
/**
 * ffi_serial.cc - Serial port configuration
 *
 * 串口配置：波特率和协议参数读写，自动适配 AP/PX4 参数命名。
 *
 * Reads/writes SERIAL*_BAUD and SERIAL*_PROTOCOL parameters.
 * Auto-detects AP vs PX4 parameter naming:
 *   AP:  SERIAL{n}_BAUD, SERIAL{n}_PROTOCOL
 *   PX4: SER_TEL{n}_BAUD, SER_TEL{n}_PROT
 *
 * Both use the same underlying PARAM_SET command.
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>

static const char* ap_serial_params[] = {
    "SERIAL0_BAUD", "SERIAL1_BAUD", "SERIAL2_BAUD", "SERIAL3_BAUD",
    "SERIAL4_BAUD", "SERIAL5_BAUD", "SERIAL6_BAUD", "SERIAL7_BAUD", NULL
};
static const char* ap_proto_params[] = {
    "SERIAL0_PROTOCOL", "SERIAL1_PROTOCOL", "SERIAL2_PROTOCOL", "SERIAL3_PROTOCOL",
    "SERIAL4_PROTOCOL", "SERIAL5_PROTOCOL", "SERIAL6_PROTOCOL", "SERIAL7_PROTOCOL", NULL
};

static const char* px4_serial_params[] = {
    "SER_TEL1_BAUD", "SER_TEL2_BAUD", "SER_TEL3_BAUD", "SER_TEL4_BAUD",
    "SER_GPS1_BAUD", "SER_GPS2_BAUD", NULL
};
static const char* px4_proto_params[] = {
    "SER_TEL1_PROT", "SER_TEL2_PROT", "SER_TEL3_PROT", "SER_TEL4_PROT",
    "SER_GPS1_PROT", "SER_GPS2_PROT", NULL
};

static const char* ap_proto_names[] = {
    "Disabled", "MAVLink2", "Serial/MAVLink", "GPS", "FrSky D",
    "FrSky S", "FrSky FPort", "VTX Telemetry", "FrSky FPort2", NULL
};

static std::string serial_port_name_from_param(const char* param_name) {
    std::string name = param_name ? param_name : "";
    const std::string baud_suffix = "_BAUD";
    if (name.size() >= baud_suffix.size() &&
        name.compare(name.size() - baud_suffix.size(), baud_suffix.size(), baud_suffix) == 0) {
        name.resize(name.size() - baud_suffix.size());
    }
    return name;
}

MGCORE_API char* mg_serial_get_configs(mg_core_t* core) {
    CHECK_CORE(core);
    bool px4 = ffi_is_px4(core);
    const char** baud_params = px4 ? px4_serial_params : ap_serial_params;
    const char** proto_params = px4 ? px4_proto_params : ap_proto_params;

    mg::bridge::JsonBuilder jb;
    jb.begin_array();

    for (int i = 0; baud_params[i]; i++) {
        float baud = ffi_get_param(core, baud_params[i]);
        float proto = ffi_get_param(core, proto_params[i]);
        if (baud == 0 && proto == 0 && i > 2) continue;  // skip unconfigured

        mg::bridge::JsonBuilder obj;
        obj.begin_object();
        obj.field("name", serial_port_name_from_param(baud_params[i]));
        obj.field("index", i);

        // Baud rate: stored as index (0=9600,1=19200,2=38400,3=57600,4=115200,5=230400,...)
        int baud_idx = (int)baud;
        int baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
        int baud_val = (baud_idx >= 0 && baud_idx < 8) ? baud_rates[baud_idx] : 115200;
        obj.field("baud_rate", baud_val);
        obj.field("baud_index", baud_idx);

        int proto_id = (int)proto;
        obj.field("protocol", proto_id);

        if (!px4 && proto_id >= 0 && proto_id < 10 && ap_proto_names[proto_id]) {
            obj.field("protocol_name", ap_proto_names[proto_id]);
        }

        obj.end_object();
        jb.append_json(obj.str());
    }

    jb.end_array();
    return to_cstr(jb.str());
}

// Read the board-specific SERIALn -> UART/USART mapping exposed by
// ArduPilot's @SYS/uarts.txt. SERIAL numbers are logical and must not be
// converted to UART letters by arithmetic.
MGCORE_API char* mg_serial_get_uart_names(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);

    const auto temp = std::filesystem::temp_directory_path() /
        ("mgcs_uarts_" + std::to_string(reinterpret_cast<uintptr_t>(core)) + ".txt");
    std::string error;
    const bool downloaded = get_active_mavlink(core)->download_file(
        active_target_sys(core), active_target_comp(core), "@SYS/uarts.txt",
        temp.string(), &error);
    if (!downloaded) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        // Do not turn a transport/FTP failure into an empty successful list.
        // Mission Planner reports this as an unavailable mapping; preserving
        // the reason here lets the Flutter page distinguish it from a real
        // board with zero UARTs.
        mg::bridge::JsonBuilder empty_items;
        empty_items.begin_array();
        empty_items.end_array();
        mg::bridge::JsonBuilder empty;
        empty.begin_object();
        empty.field("success", false);
        empty.field("available", false);
        const std::string reason = error.empty()
            ? "飞控未提供 @SYS/uarts.txt，或 MAVLink FTP 读取失败"
            : error;
        empty.field("error", reason);
        empty.raw_field("items", empty_items.str());
        empty.end_object();
        return to_cstr(empty.str());
    }

    std::ifstream input(temp);
    if (!input) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return fail(MG_ERROR_INTERNAL, "无法打开已下载的 @SYS/uarts.txt");
    }

    mg::bridge::JsonBuilder items;
    items.begin_array();
    unsigned item_count = 0;
    std::string line;
    while (std::getline(input, line)) {
        // ArduPilot emits a machine-readable UARTV1 header followed by lines
        // such as "SERIAL2 USART3 TX=...".  Be tolerant of indentation,
        // UTF-8 BOMs, and the older "SERIAL2: USART3" spelling.
        while (!line.empty() && std::isspace(
                   static_cast<unsigned char>(line.front()))) {
            line.erase(line.begin());
        }
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        std::istringstream fields(line);
        std::string serial_name;
        std::string hardware_name;
        fields >> serial_name >> hardware_name;
        if (!serial_name.empty() && serial_name.back() == ':') {
            serial_name.pop_back();
        }
        while (!hardware_name.empty() &&
               (hardware_name.back() == ':' || hardware_name.back() == ',')) {
            hardware_name.pop_back();
        }
        if (serial_name.rfind("SERIAL", 0) != 0 || hardware_name.empty()) continue;
        int serial_index = -1;
        try {
            const std::string index_text = serial_name.substr(6);
            size_t consumed = 0;
            serial_index = std::stoi(index_text, &consumed);
            if (consumed != index_text.size()) continue;
        } catch (...) {
            continue;
        }
        if (serial_index < 0) continue;
        int flow_control = -1;
        std::string token;
        while (fields >> token) {
            constexpr const char* prefix = "FlowCtrl=";
            if (token.rfind(prefix, 0) == 0) {
                try { flow_control = std::stoi(token.substr(9)); } catch (...) {}
            }
        }
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("serial_index", serial_index);
        item.field("name", hardware_name);
        if (flow_control >= 0) item.field("flow_control", flow_control);
        item.end_object();
        items.append_json(item.str());
        ++item_count;
    }
    items.end_array();
    mg::bridge::JsonBuilder result;
    result.begin_object();
    result.field("success", item_count != 0);
    result.field("available", item_count != 0);
    result.field("count", item_count);
    if (item_count == 0) {
        result.field("error", "@SYS/uarts.txt 为空或格式中没有 SERIALn 硬件接口");
    }
    result.raw_field("items", items.str());
    result.end_object();
    input.close();
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
    return to_cstr(result.str());
}

MGCORE_API char* mg_serial_get_config(mg_core_t* core, const char* port_name) {
    CHECK_CORE(core);
    if (!port_name || port_name[0] == '\0') return to_cstr("{}");

    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");

    // Try direct parameter lookup with the provided name
    std::string baud_param = std::string(port_name) + "_BAUD";
    std::string proto_param = std::string(port_name) + "_PROTOCOL";
    auto pb = mav->get_param_by_name(baud_param.c_str());
    auto pp = mav->get_param_by_name(proto_param.c_str());

    // Also try PX4 naming
    if (!pb.has_value()) {
        baud_param = std::string(port_name) + "_BAUD";
        proto_param = std::string(port_name) + "_PROT";
        pb = mav->get_param_by_name(baud_param.c_str());
        pp = mav->get_param_by_name(proto_param.c_str());
    }

    if (!pb.has_value()) return to_cstr("{}");

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("name", port_name);
    int baud_idx = (int)pb->value;
    int baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    jb.field("baud_rate", (baud_idx >= 0 && baud_idx < 8) ? baud_rates[baud_idx] : 115200);
    jb.field("baud_index", baud_idx);
    jb.field("protocol", pp.has_value() ? (int)pp->value : -1);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_serial_set_baud(mg_core_t* core, const char* port_name, int baud) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!port_name || port_name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null port");

    // Convert baud rate to index
    int baud_idx = 4;  // default 115200
    int rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (int i = 0; i < 8; i++) {
        if (rates[i] == baud) { baud_idx = i; break; }
    }

    std::string param = std::string(port_name) + "_BAUD";
    auto* mav = get_active_mavlink(core);
    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       param.c_str(), (float)baud_idx, 6)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "set baud failed");
}

MGCORE_API char* mg_serial_set_protocol(mg_core_t* core, const char* port_name, int protocol) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!port_name || port_name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null port");

    // Try AP naming first, then PX4
    std::string param = std::string(port_name) + "_PROTOCOL";
    auto* mav = get_active_mavlink(core);

    auto p = mav->get_param_by_name(param.c_str());
    if (!p.has_value()) {
        param = std::string(port_name) + "_PROT";
        p = mav->get_param_by_name(param.c_str());
    }
    if (!p.has_value()) return fail(MG_ERROR_NOT_FOUND, "serial param not found");

    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       param.c_str(), (float)protocol, p->type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "set protocol failed");
}
