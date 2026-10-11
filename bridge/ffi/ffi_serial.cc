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
#include "core/telemetry/json_serializers.h"
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

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();

    for (int i = 0; baud_params[i]; i++) {
        float baud = ffi_get_param(core, baud_params[i]);
        float proto = ffi_get_param(core, proto_params[i]);
        if (baud == 0 && proto == 0 && i > 2) continue;  // skip unconfigured

        mg::core::telemetry::JsonBuilder obj;
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

    // MAVLink FTP 传输在 core 的后台线程上跑；这里等它结束，最长不超过
    // kUartMappingWaitMs，且不持任何 adapter 锁。本入口目前没有 UI 调用方，
    // 等待上限就是它的最坏耗时；一旦有界面需要它，应改成
    // mg_fs_download_file + mg_fs_transfer_status 的异步形式。
    constexpr int kUartMappingWaitMs = 8000;
    auto* mavlink = get_active_mavlink(core);
    const auto temp = std::filesystem::temp_directory_path() /
        ("mgcs_uarts_" + std::to_string(reinterpret_cast<uintptr_t>(core)) + ".txt");
    std::error_code ignored;
    // 上一次调用可能留下半截文件；先删掉，避免把陈旧内容当成这次下载的结果。
    std::filesystem::remove(temp, ignored);

    std::string error;
    const bool started = mavlink->file_download_start(
        active_target_sys(core), active_target_comp(core), "@SYS/uarts.txt",
        temp.string());
    if (!started) {
        error = "MAVLink FTP 正在执行其他传输";
    } else if (!mavlink->file_transfer_wait(kUartMappingWaitMs)) {
        mavlink->file_transfer_cancel();
        error = "读取 @SYS/uarts.txt 超时";
    } else {
        const auto status = mavlink->file_transfer_status();
        if (!status.success) {
            error = status.error.empty() ? "MAVLink FTP 读取失败" : status.error;
        }
    }

    if (!error.empty()) {
        std::filesystem::remove(temp, ignored);
        // Do not turn a transport/FTP failure into an empty successful list.
        // Mission Planner reports this as an unavailable mapping; preserving
        // the reason here lets the client distinguish it from a real
        // board with zero UARTs.
        mg::core::telemetry::JsonBuilder empty_items;
        empty_items.begin_array();
        empty_items.end_array();
        mg::core::telemetry::JsonBuilder empty;
        empty.begin_object();
        empty.field("success", false);
        empty.field("available", false);
        write_error(empty, MG_ERROR_SEND_FAILED,
                    error.empty()
                        ? std::string("飞控未提供 @SYS/uarts.txt，或 MAVLink FTP 读取失败")
                        : error);
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

    mg::core::telemetry::JsonBuilder items;
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
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("serial_index", serial_index);
        item.field("name", hardware_name);
        if (flow_control >= 0) item.field("flow_control", flow_control);
        item.end_object();
        items.append_json(item.str());
        ++item_count;
    }
    items.end_array();
    mg::core::telemetry::JsonBuilder result;
    result.begin_object();
    result.field("success", item_count != 0);
    result.field("available", item_count != 0);
    result.field("count", item_count);
    if (item_count == 0) {
        write_error(result, MG_ERROR_NOT_FOUND,
                    "@SYS/uarts.txt 为空或格式中没有 SERIALn 硬件接口");
    }
    result.raw_field("items", items.str());
    result.end_object();
    input.close();
    std::filesystem::remove(temp, ignored);
    return to_cstr(result.str());
}
