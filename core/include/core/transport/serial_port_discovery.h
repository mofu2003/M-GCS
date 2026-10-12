#pragma once

// 串口发现与识别模块，枚举系统串口并识别飞控设备

#include "core/core_types.h"
#include <regex>

namespace mg::core::transport {

// 串口信息，包含端口名、描述、制造商、VID/PID 等设备信息
struct SerialPortInfo {
    std::string port_name;           // e.g., "COM5" or "/dev/ttyUSB0"
    std::string description;         // human-readable description
    std::string manufacturer;        // manufacturer string
    std::string serial_number;       // serial number if available
    uint16_t vendor_id = 0;          // USB VID
    uint16_t product_id = 0;         // USB PID
    bool is_likely_fc = false;       // heuristic: likely flight controller
    bool is_available = true;        // not currently in use
    std::string board_class;         // "Pixhawk", "SiK Radio", "OpenPilot", etc.
    std::string board_name;          // specific board name if known
};

// 端口过滤条件
enum class PortFilter {
    All,                // all serial ports
    LikelyFlightController, // only likely flight controllers
    AvailableOnly       // only available (not in use)
};

// 板卡信息（VID/PID 与板卡类型映射）
struct BoardInfo {
    uint16_t vendor_id;
    uint16_t product_id;
    std::string board_class;
    std::string name;
};

// 板卡描述的正则回退匹配规则
struct BoardRegExpFallback {
    std::regex reg_exp;
    std::string board_class;
    bool android_only = false;
};

// 串口发现类，枚举系统串口并通过 VID/PID 或描述正则识别飞控设备
class SerialPortDiscovery {
public:
    SerialPortDiscovery();
    ~SerialPortDiscovery();

    /// 枚举系统上所有串口
    std::vector<SerialPortInfo> enumerate_ports();

    /// 按条件过滤串口列表
    std::vector<SerialPortInfo> filter_ports(PortFilter filter);

    /// 获取最佳候选端口（最可能是飞控的端口）
    std::optional<SerialPortInfo> best_candidate();

    /// 检查指定端口是否可用
    bool is_port_available(const std::string& port_name);

    /// 刷新端口列表（重新枚举）
    void refresh();

    /// 判定一个端口对应的板卡类别与名称。
    /// 依次尝试：VID/PID 精确匹配 → 端口描述正则兜底 → 制造商正则兜底。
    /// 用于给「用户手工输入、或来自历史记录」的端口分类，这类端口不在
    /// enumerate_ports() 的结果里，拿不到已填好的 board_class/board_name。
    /// 返回 false 表示三者都没匹配上（board_class/board_name 不被修改）。
    bool get_board_info(const SerialPortInfo& port, std::string& board_class, std::string& board_name);

private:
    std::vector<SerialPortInfo> cached_ports_;  // 缓存的端口列表
    bool needs_refresh_ = true;
    bool json_loaded_ = false;
    bool json_data_valid_ = false;

    std::vector<BoardInfo> board_info_list_;                    // VID/PID 板卡映射表
    std::vector<BoardRegExpFallback> board_description_fallback_list_;  // 描述正则回退列表
    std::vector<BoardRegExpFallback> board_manufacturer_fallback_list_; // 制造商正则回退列表

    // 平台特定的端口枚举实现
    std::vector<SerialPortInfo> enumerate_ports_impl();

    // JSON 数据加载
    bool load_json_data();

    // 板卡检测逻辑
    bool is_bootloader(const SerialPortInfo& port);
    bool is_system_port(const SerialPortInfo& port);

};

} // namespace mg::core::transport
