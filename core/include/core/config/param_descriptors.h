/**
 * param_descriptors.h - Parameter descriptor database
 * 参数元数据 — 加载 JSON 数据库，提供参数显示名、枚举选项、验证规则
 *
 * Loads ParameterDescriptors.json at startup.
 * Provides parameter metadata (display name, enum options, validation)
 * for any parameter name.
 *
 * Design: same pattern as SensorIdDecoder and SerialPortDiscovery.
 * Zero network dependency. All data from local JSON file.
 *
 * Usage:
 *   ParamDescriptors db("path/to/ParameterDescriptors.json");
 *   auto desc = db.get("SERIAL1_PROTOCOL");
 *   // desc.display_name = "Protocol"
 *   // desc.options = [{0,"Disabled"}, {2,"MAVLink2"}, ...]
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

struct cJSON;

namespace mg::core::config {

/// A single enum option
struct ParamOption {
    int value;
    std::string label;
    std::string english_label;
    std::string description;
};

/// Parameter descriptor
struct ParamDescriptor {
    std::string param_name;         // Full param name (e.g., "SERIAL1_PROTOCOL")
    std::string display_name;       // Human-readable (e.g., "Serial 1 Protocol")
    std::string description;        // What it controls
    std::string group;              // Logical group (e.g., "serial")
    std::string type;               // "enum", "int", "float", "bool"
    std::string category;           // Standard/Advanced when provided by firmware metadata
    bool requires_reboot = false;
    bool read_only = false;
    float min_value = 0;
    float max_value = 0;
    float default_value = 0;
    float increment = 0;
    std::string unit;
    std::vector<ParamOption> options;
    bool found = false;             // true if descriptor exists for this param
};

/// Group descriptor
struct GroupDescriptor {
    std::string name;
    std::string display_name;
    std::string description;
    std::vector<std::string> param_names;  // Ordered list of params in this group
};

class ParamDescriptors {
public:
    /// Load from JSON file. If path is empty, tries to find it automatically.
    explicit ParamDescriptors(const std::string& json_path = "",
                              const std::string& translation_path = "");
    ~ParamDescriptors();

    /// Get descriptor for a parameter by full name.
    /// Returns a descriptor with found=false if not in database.
    ParamDescriptor get(const std::string& param_name) const;
    ParamDescriptor get(const std::string& profile, const std::string& param_name) const;

    /// Check if a parameter has a descriptor.
    bool has(const std::string& param_name) const;
    bool has(const std::string& profile, const std::string& param_name) const;

    /// Get all parameter names in a group.
    std::vector<std::string> get_group_params(const std::string& group_name) const;
    std::vector<std::string> get_group_params(const std::string& profile, const std::string& group_name) const;

    /// Get group descriptor.
    GroupDescriptor get_group(const std::string& group_name) const;
    GroupDescriptor get_group(const std::string& profile, const std::string& group_name) const;

    /// Get all group names.
    std::vector<std::string> get_group_names() const;
    std::vector<std::string> get_group_names(const std::string& profile) const;

    /// Get all groups.
    std::vector<GroupDescriptor> get_all_groups() const;
    std::vector<GroupDescriptor> get_all_groups(const std::string& profile) const;

    /// Validate a value against a parameter's descriptor.
    /// Returns empty string if valid, error message if invalid.
    std::string validate(const std::string& param_name, float value) const;
    std::string validate(const std::string& profile, const std::string& param_name, float value) const;

    /// Look up enum label for a value.
    /// Returns empty string if not found.
    std::string enum_label(const std::string& param_name, int value) const;
    std::string enum_label(const std::string& profile, const std::string& param_name, int value) const;

    bool is_loaded() const { return loaded_; }

private:
    bool loaded_ = false;
    std::map<std::string, ParamDescriptor> descriptors_;
    std::map<std::string, GroupDescriptor> groups_;
    std::map<std::string, std::map<std::string, ParamDescriptor>> profile_descriptors_;
    std::map<std::string, std::map<std::string, GroupDescriptor>> profile_groups_;
    std::map<std::string, std::string> profile_extends_;
    std::set<std::string> fallback_translation_profiles_;
    std::map<std::string, std::string> fallback_translation_terms_;
    std::map<std::string, std::string> fallback_option_labels_;
    std::map<std::string, std::string> fallback_option_terms_;
    std::vector<std::pair<std::string, std::string>> fallback_option_terms_sorted_;

    bool load_json(const std::string& path);
    bool load_translation_json(const std::string& path);
    bool parse_groups(cJSON* groups,
                      std::map<std::string, ParamDescriptor>& descriptors,
                      std::map<std::string, GroupDescriptor>& groups_out) const;
    void apply_group_translation(cJSON* group_obj,
                                 std::map<std::string, GroupDescriptor>& groups);
    void apply_param_translation(cJSON* param_obj,
                                 std::map<std::string, ParamDescriptor>& descriptors);
    std::string find_json_file() const;
    std::string find_translation_file() const;
    std::string normalize_profile(const std::string& profile) const;
    ParamDescriptor get_from_profile(const std::string& profile,
                                     const std::string& param_name,
                                     int depth) const;
    ParamDescriptor apply_fallback_translation(const std::string& profile,
                                               ParamDescriptor descriptor) const;
    std::string fallback_group_display_name(const std::string& profile,
                                            const std::string& group,
                                            int depth) const;
    std::string fallback_display_name(const std::string& profile,
                                      const ParamDescriptor& descriptor) const;
    std::string fallback_option_label(const std::string& label,
                                      const std::string& english_label) const;
    std::map<std::string, GroupDescriptor> merged_profile_groups(const std::string& profile,
                                                                 int depth) const;
};

} // namespace mg::core::config
