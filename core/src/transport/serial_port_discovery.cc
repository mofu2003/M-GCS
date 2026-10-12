/**
 * serial_port_discovery.cc - Serial port enumeration and flight controller detection
 *
 * Enumerates system serial ports and matches USB VID/PID against known
 * flight controller boards defined in USBBoardInfo.json.
 *
 * References:
 *   QGC: src/comm/SerialLink.cc + src/USBBoardInfo.json
 *   MP:  ExtLibs/Mavlink/MAVLinkInterface.cs (port enumeration)
 *
 * Platform support:
 *   Windows: SetupDi + Registry
 *   Linux/macOS: /sys/class/tty + /dev
 */

#include "core/transport/serial_port_discovery.h"
#include <fstream>
#include "core/log/logging.h"
#include "core/platform/platform_filesystem.h"
#include "core/util/json.h"
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#ifndef GUID_DEVCLASS_PORTS
static const GUID GUID_DEVCLASS_PORTS = {
    0x4D36E978, 0xE325, 0x11CE,
    {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}
};
#endif
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")

#ifdef _WIN32
// Convert Windows ANSI (GBK/CP936 on Chinese Windows) string to UTF-8.
// SetupDiGetDeviceRegistryProperty returns system-locale encoded strings,
// which are not valid UTF-8 on non-English Windows. This function converts
// them via the Windows Unicode (UTF-16) intermediate representation.
static std::string ansi_to_utf8(const char* ansi_str) {
    if (!ansi_str || ansi_str[0] == '\0') return "";

    // Step 1: ANSI (system codepage) -> UTF-16
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, nullptr, 0);
    if (wlen <= 1) return std::string(ansi_str); // fallback: return as-is
    std::wstring wstr(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, &wstr[0], wlen);
    wstr.resize(static_cast<size_t>(wlen - 1));

    // Step 2: UTF-16 -> UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 1) return std::string(ansi_str);
    std::string utf8(static_cast<size_t>(ulen), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], ulen, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(ulen - 1));

    return utf8;
}
#endif

#else
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/USBSpec.h>
#endif
#endif

namespace mg::core::transport {

namespace {

bool is_flight_controller_board_class(const std::string& board_class) {
    return board_class == "Pixhawk" ||
           board_class == "OpenPilot" ||
           board_class == "SiK Radio";
}

} // namespace

#ifdef __APPLE__

static std::string cf_string(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return "";

    char buffer[1024] = {};
    return CFStringGetCString(static_cast<CFStringRef>(value), buffer,
                              sizeof(buffer), kCFStringEncodingUTF8)
        ? std::string(buffer)
        : "";
}

static uint16_t cf_uint16(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return 0;

    int number = 0;
    return CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType,
                            &number) && number >= 0 && number <= UINT16_MAX
        ? static_cast<uint16_t>(number)
        : 0;
}

static CFTypeRef copy_ancestor_property(io_registry_entry_t service,
                                         CFStringRef key) {
    io_registry_entry_t current = service;
    bool owns_current = false;

    while (current != IO_OBJECT_NULL) {
        CFTypeRef value = IORegistryEntryCreateCFProperty(
            current, key, kCFAllocatorDefault, 0);
        if (value) {
            if (owns_current) IOObjectRelease(current);
            return value;
        }

        io_registry_entry_t parent = IO_OBJECT_NULL;
        if (IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent) !=
            KERN_SUCCESS) {
            break;
        }
        if (owns_current) IOObjectRelease(current);
        current = parent;
        owns_current = true;
    }

    if (owns_current && current != IO_OBJECT_NULL) IOObjectRelease(current);
    return nullptr;
}

static std::string ancestor_string_property(io_registry_entry_t service,
                                            CFStringRef key) {
    CFTypeRef value = copy_ancestor_property(service, key);
    const std::string result = cf_string(value);
    if (value) CFRelease(value);
    return result;
}

static uint16_t ancestor_uint16_property(io_registry_entry_t service,
                                         CFStringRef key) {
    CFTypeRef value = copy_ancestor_property(service, key);
    const uint16_t result = cf_uint16(value);
    if (value) CFRelease(value);
    return result;
}

static void populate_macos_usb_details(SerialPortInfo& port) {
    CFMutableDictionaryRef matching = IOServiceMatching(kIOSerialBSDServiceValue);
    if (!matching) return;

    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) !=
        KERN_SUCCESS) {
        return;
    }

    while (const io_registry_entry_t service = IOIteratorNext(iterator)) {
        CFTypeRef callout = IORegistryEntryCreateCFProperty(
            service, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
        const bool matches = cf_string(callout) == port.port_name;
        if (callout) CFRelease(callout);
        if (!matches) {
            IOObjectRelease(service);
            continue;
        }

        port.vendor_id = ancestor_uint16_property(service, CFSTR(kUSBVendorID));
        port.product_id = ancestor_uint16_property(service, CFSTR(kUSBProductID));
        port.description = ancestor_string_property(service, CFSTR(kUSBProductString));
        port.manufacturer = ancestor_string_property(service, CFSTR(kUSBVendorString));
        port.serial_number = ancestor_string_property(
            service, CFSTR(kUSBSerialNumberString));
        IOObjectRelease(service);
        break;
    }

    IOObjectRelease(iterator);
}

#endif

// ================================================================
// Public API
// ================================================================

SerialPortDiscovery::SerialPortDiscovery() = default;
SerialPortDiscovery::~SerialPortDiscovery() = default;

std::vector<SerialPortInfo> SerialPortDiscovery::enumerate_ports() {
    if (needs_refresh_) {
        cached_ports_ = enumerate_ports_impl();
        needs_refresh_ = false;
    }
    return cached_ports_;
}

std::vector<SerialPortInfo> SerialPortDiscovery::filter_ports(PortFilter filter) {
    auto ports = enumerate_ports();
    std::vector<SerialPortInfo> filtered;

    for (const auto& port : ports) {
        bool include = false;
        switch (filter) {
            case PortFilter::All:                 include = true; break;
            case PortFilter::LikelyFlightController: include = port.is_likely_fc; break;
            case PortFilter::AvailableOnly:       include = port.is_available; break;
        }
        if (include) filtered.push_back(port);
    }
    return filtered;
}

std::optional<SerialPortInfo> SerialPortDiscovery::best_candidate() {
    auto fc_ports = filter_ports(PortFilter::LikelyFlightController);
    if (!fc_ports.empty()) return fc_ports[0];

    auto available = filter_ports(PortFilter::AvailableOnly);
    if (!available.empty()) return available[0];

    return std::nullopt;
}

bool SerialPortDiscovery::is_port_available(const std::string& port_name) {
    auto ports = enumerate_ports();
    for (const auto& port : ports) {
        if (port.port_name == port_name) return port.is_available;
    }
    return false;
}

void SerialPortDiscovery::refresh() {
    needs_refresh_ = true;
    cached_ports_.clear();
}

// ================================================================
// JSON board info loading
// ================================================================

// Minimal JSON parser for USBBoardInfo.json
// USBBoardInfo.json 用 cJSON 解析（third_party/cjson，项目已有依赖）。
//
// 历史上这里是一段手写的括号扫描器，有两个真实缺陷：
//   1. 用 `find(']')` 找数组结尾，遇到正则字符串里的 ']' 就提前收尾：
//      `"^fmuv[2345]$"` 会让 boardDescriptionFallback 只读到 35/51 条，
//      静默丢弃 16 条板卡描述（ARK TROPIC/mRoControlZeroF7 等）。
//   2. 用"找下一个引号字符串"的方式读 androidOnly，而它在 JSON 里是 bool，
//      于是 android_only 恒为 false——那条标注 "too dangerous for non-android"
//      的宽泛 fallback 本应在桌面端被排除。
// 改用 cJSON 后两者都消失：转义与嵌套由解析器处理，类型由 cJSON_Is* 判断，
// 首字节的 UTF-8 BOM 由 cJSON_Parse 自行跳过。

static int json_int_or_zero(cJSON* object, const char* key) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? static_cast<int>(item->valuedouble) : 0;
}

static std::string json_str_or_empty(cJSON* object, const char* key) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring != nullptr
        ? std::string(item->valuestring) : std::string();
}

static bool json_bool_or_false(cJSON* object, const char* key) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, key));
}

static bool load_board_info_from_json(const std::string& path,
                                       std::vector<BoardInfo>& boards,
                                       std::vector<BoardRegExpFallback>& desc_fallbacks,
                                       std::vector<BoardRegExpFallback>& mfg_fallbacks) {
    std::ifstream file(path);
    if (!file.is_open()) {
        MGC_LOG_ERROR("[Discovery] Cannot open %s", path.c_str());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    cJSON* root = mg::core::json::parse(content.c_str());
    if (root == nullptr) {
        MGC_LOG_ERROR("[Discovery] %s is not valid JSON", path.c_str());
        return false;
    }

    // boardInfo：VID/PID 同时为 0 的条目不参与匹配（保持历史行为）。
    cJSON* board_list = cJSON_GetObjectItemCaseSensitive(root, "boardInfo");
    cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, board_list) {
        if (!cJSON_IsObject(entry)) continue;
        BoardInfo info;
        info.vendor_id   = static_cast<uint16_t>(json_int_or_zero(entry, "vendorID"));
        info.product_id  = static_cast<uint16_t>(json_int_or_zero(entry, "productID"));
        info.board_class = json_str_or_empty(entry, "boardClass");
        info.name        = json_str_or_empty(entry, "name");
        if (info.vendor_id != 0 || info.product_id != 0) {
            boards.push_back(std::move(info));
        }
    }

    // 两张按名字匹配的兜底表：结构相同，只有 boardDescriptionFallback 带
    // androidOnly 标记。正则编译失败的条目跳过（保持历史行为）。
    const auto load_fallbacks = [&](const char* array_key, bool read_android_only,
                                    std::vector<BoardRegExpFallback>& out) {
        cJSON* list = cJSON_GetObjectItemCaseSensitive(root, array_key);
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, list) {
            if (!cJSON_IsObject(item)) continue;
            const std::string pattern = json_str_or_empty(item, "regExp");
            if (pattern.empty()) continue;
            BoardRegExpFallback fb;
            fb.board_class  = json_str_or_empty(item, "boardClass");
            fb.android_only = read_android_only && json_bool_or_false(item, "androidOnly");
            try {
                fb.reg_exp = std::regex(pattern, std::regex::icase);
                out.push_back(std::move(fb));
            } catch (const std::regex_error&) {
            }
        }
    };
    load_fallbacks("boardDescriptionFallback", true, desc_fallbacks);
    load_fallbacks("boardManufacturerFallback", false, mfg_fallbacks);

    cJSON_Delete(root);
    return !boards.empty();
}

bool SerialPortDiscovery::load_json_data() {
    if (json_loaded_) return json_data_valid_;
    json_loaded_ = true;

    // Try multiple paths for USBBoardInfo.json
    // 1. Next to the executable
    // 2. In core/src/transport/ (development tree)
    // Search paths relative to the current working directory.
    // The JSON file lives at core/src/transport/USBBoardInfo.json in the source tree.
    // At runtime the working directory may be build/, project root, or elsewhere.
    std::vector<std::string> search_paths = {
        mg::core::platform::find_data_file("USBBoardInfo.json"),
        mg::core::platform::get_module_directory() +
            "../Resources/USBBoardInfo.json",             // macOS app bundle
        "USBBoardInfo.json",                              // next to executable (build/)
        "build/USBBoardInfo.json",                        // from project root
        "core/src/transport/USBBoardInfo.json",            // from project root
        "../core/src/transport/USBBoardInfo.json",         // from build/
        "../../core/src/transport/USBBoardInfo.json",      // from build/<subdir>/
        "../../../core/src/transport/USBBoardInfo.json",   // from build/<subdir>/<subdir>/
    };

    for (const auto& path : search_paths) {
        if (!std::filesystem::exists(path)) continue;
        if (load_board_info_from_json(path, board_info_list_,
                                       board_description_fallback_list_,
                                       board_manufacturer_fallback_list_)) {
            json_data_valid_ = true;
            MGC_LOG_INFO("[Discovery] Loaded %zu boards from %s", board_info_list_.size(), path.c_str());
            return true;
        }
    }

    MGC_LOG_ERROR("[Discovery] Failed to load USBBoardInfo.json");
    return false;
}

// ================================================================
// Board detection
// ================================================================

bool SerialPortDiscovery::get_board_info(const SerialPortInfo& port,
                                          std::string& board_class,
                                          std::string& board_name) {
    load_json_data();

    // Match by VID/PID
    for (const auto& info : board_info_list_) {
        if (info.vendor_id == port.vendor_id && info.product_id == port.product_id) {
            board_class = info.board_class;
            board_name  = info.name;
            return true;
        }
    }

    // Fallback: match by description regex
    for (const auto& fb : board_description_fallback_list_) {
#ifndef __ANDROID__
        if (fb.android_only) continue;  // allow android-only on android
#endif
        if (std::regex_search(port.description, fb.reg_exp)) {
            board_class = fb.board_class;
            board_name  = port.description;
            return true;
        }
    }

    // Fallback: match by manufacturer regex
    for (const auto& fb : board_manufacturer_fallback_list_) {
        if (std::regex_search(port.manufacturer, fb.reg_exp)) {
            board_class = fb.board_class;
            board_name  = port.manufacturer;
            return true;
        }
    }

    return false;
}

bool SerialPortDiscovery::is_bootloader(const SerialPortInfo& port) {
    // Bootloader detection: description contains "BL" prefix or "Bootloader"
    std::string desc = port.description;
    if (desc.find("BL ") != std::string::npos ||
        desc.find("Bootloader") != std::string::npos) {
        return true;
    }
    return false;
}

bool SerialPortDiscovery::is_system_port(const SerialPortInfo& port) {
    // Filter out Bluetooth, internal, and other non-physical ports
    std::string desc = port.description;
    if (desc.find("Bluetooth") != std::string::npos) return true;
    if (desc.find("Internal") != std::string::npos) return true;
    if (desc.find("Virtual") != std::string::npos) return true;
    return false;
}

// ================================================================
// Platform-specific port enumeration
// ================================================================

#ifdef _WIN32

std::vector<SerialPortInfo> SerialPortDiscovery::enumerate_ports_impl() {
    std::vector<SerialPortInfo> ports;

    HDEVINFO device_info_set = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS, nullptr, nullptr,
        DIGCF_PRESENT
    );

    if (device_info_set == INVALID_HANDLE_VALUE) {
        MGC_LOG_ERROR("[Discovery] SetupDiGetClassDevs failed");
        return ports;
    }

    SP_DEVINFO_DATA device_info_data = {};
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(device_info_set, i, &device_info_data); i++) {
        SerialPortInfo port;

        // Get port name from registry
        HKEY device_key = SetupDiOpenDevRegKey(
            device_info_set, &device_info_data,
            DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ
        );

        if (device_key != INVALID_HANDLE_VALUE) {
            char port_name[256] = {};
            DWORD port_name_size = sizeof(port_name);
            if (RegQueryValueEx(device_key, "PortName", nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(port_name), &port_name_size) == ERROR_SUCCESS) {
                port.port_name = port_name;
            }
            RegCloseKey(device_key);
        }

        // Get device description
        char description[256] = {};
        if (SetupDiGetDeviceRegistryProperty(device_info_set, &device_info_data,
                                              SPDRP_DEVICEDESC, nullptr,
                                              reinterpret_cast<PBYTE>(description), sizeof(description), nullptr)) {
            port.description = ansi_to_utf8(description);
        }

        // Get manufacturer
        char manufacturer[256] = {};
        if (SetupDiGetDeviceRegistryProperty(device_info_set, &device_info_data,
                                              SPDRP_MFG, nullptr,
                                              reinterpret_cast<PBYTE>(manufacturer), sizeof(manufacturer), nullptr)) {
            port.manufacturer = ansi_to_utf8(manufacturer);
        }

        // Get hardware ID to extract VID/PID
        char hardware_id[256] = {};
        if (SetupDiGetDeviceRegistryProperty(device_info_set, &device_info_data,
                                              SPDRP_HARDWAREID, nullptr,
                                              reinterpret_cast<PBYTE>(hardware_id), sizeof(hardware_id), nullptr)) {
            std::string hw_id = hardware_id;
            size_t vid_pos = hw_id.find("VID_");
            size_t pid_pos = hw_id.find("PID_");
            if (vid_pos != std::string::npos && pid_pos != std::string::npos) {
                try {
                    port.vendor_id  = static_cast<uint16_t>(std::stoul(hw_id.substr(vid_pos + 4, 4), nullptr, 16));
                    port.product_id = static_cast<uint16_t>(std::stoul(hw_id.substr(pid_pos + 4, 4), nullptr, 16));
                } catch (...) {}
            }
        }

        // Board detection
        std::string board_class, board_name;
        if (get_board_info(port, board_class, board_name)) {
            port.board_class = board_class;
            port.board_name  = board_name;
            port.is_likely_fc = is_flight_controller_board_class(board_class);
        }

        // Filter system ports
        if (!is_system_port(port)) {
            ports.push_back(port);
        }
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return ports;
}

#else  // Linux / macOS

std::vector<SerialPortInfo> SerialPortDiscovery::enumerate_ports_impl() {
    std::vector<SerialPortInfo> ports;

    DIR* dir = opendir("/dev");
    if (!dir) return ports;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

#ifdef __APPLE__
        if (name.find("tty.usb") == std::string::npos &&
            name.find("cu.usb")  == std::string::npos &&
            name.find("tty.SLAB") == std::string::npos &&
            name.find("cu.SLAB")  == std::string::npos) {
            continue;
        }
#else
        if (name.find("ttyUSB") == std::string::npos &&
            name.find("ttyACM") == std::string::npos &&
            name.find("ttyS")   == std::string::npos) {
            continue;
        }
#endif

        SerialPortInfo port;
        port.port_name = "/dev/" + name;

#ifdef __APPLE__
        port.description = name;
        populate_macos_usb_details(port);
#else
        std::string sys_path = "/sys/class/tty/" + name + "/device/";

        auto read_sys = [&](const std::string& file) -> std::string {
            std::ifstream f(sys_path + file);
            std::string val;
            if (f.is_open()) std::getline(f, val);
            return val;
        };

        std::string vid_str = read_sys("idVendor");
        if (!vid_str.empty()) {
            try { port.vendor_id = static_cast<uint16_t>(std::stoul(vid_str, nullptr, 16)); } catch (...) {}
        }

        std::string pid_str = read_sys("idProduct");
        if (!pid_str.empty()) {
            try { port.product_id = static_cast<uint16_t>(std::stoul(pid_str, nullptr, 16)); } catch (...) {}
        }

        port.manufacturer = read_sys("manufacturer");
        port.description  = read_sys("product");
#endif

        std::string board_class, board_name;
        if (get_board_info(port, board_class, board_name)) {
            port.board_class = board_class;
            port.board_name  = board_name;
            port.is_likely_fc = is_flight_controller_board_class(board_class);
        }

#ifdef __APPLE__
        // The USB product string is supplied by the connected board itself and
        // is more specific than generic VID/PID entries such as ArduPilot
        // ChibiOS. Keep the mapping for classification, but show this name.
        if (!port.description.empty() &&
            port.description != name) {
            port.board_name = port.description;
        }
#endif

        if (!is_system_port(port)) {
            ports.push_back(port);
        }
    }

    closedir(dir);
    return ports;
}

#endif

} // namespace mg::core::transport
