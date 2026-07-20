/**
 * param_descriptors.cc - Parameter descriptor database implementation
 * 参数元数据实现 — 解析 JSON，展开实例化参数组
 *
 * Parses ParameterDescriptors.json using cJSON.
 * Expands instance-based groups (SERIAL1-8, SERVO1-16, RC1-16)
 * into individual parameter descriptors.
 */
#include "core/config/param_descriptors.h"
#include "core/platform/platform_filesystem.h"
#include "core/log/logging.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace mg::core::config {

// ================================================================
// Constructor
// ================================================================

ParamDescriptors::ParamDescriptors(const std::string& json_path,
                                   const std::string& translation_path) {
    std::string path = json_path;
    if (path.empty()) {
        path = find_json_file();
    }
    if (!path.empty()) {
        loaded_ = load_json(path);
    }
    if (loaded_) {
        std::string zh_path = translation_path;
        if (zh_path.empty()) zh_path = find_translation_file();
        if (!zh_path.empty()) load_translation_json(zh_path);
    }
    if (loaded_) {
        MGC_LOG_INFO("[ParamDesc] Loaded %zu parameter descriptors, %zu groups",
                     descriptors_.size(), groups_.size());
    } else {
        MGC_LOG_WARN("[ParamDesc] Failed to load parameter descriptors");
    }
}

ParamDescriptors::~ParamDescriptors() = default;

// ================================================================
// Public API
// ================================================================

ParamDescriptor ParamDescriptors::get(const std::string& param_name) const {
    auto it = descriptors_.find(param_name);
    if (it != descriptors_.end()) return it->second;

    ParamDescriptor desc;
    desc.param_name = param_name;
    desc.display_name = param_name;
    desc.found = false;
    return desc;
}

ParamDescriptor ParamDescriptors::get(const std::string& profile, const std::string& param_name) const {
    return get_from_profile(normalize_profile(profile), param_name, 0);
}

bool ParamDescriptors::has(const std::string& param_name) const {
    return descriptors_.find(param_name) != descriptors_.end();
}

bool ParamDescriptors::has(const std::string& profile, const std::string& param_name) const {
    auto desc = get(profile, param_name);
    return desc.found;
}

std::vector<std::string> ParamDescriptors::get_group_params(const std::string& group_name) const {
    auto it = groups_.find(group_name);
    if (it != groups_.end()) return it->second.param_names;
    return {};
}

std::vector<std::string> ParamDescriptors::get_group_params(const std::string& profile,
                                                            const std::string& group_name) const {
    auto group = get_group(profile, group_name);
    return group.param_names;
}

GroupDescriptor ParamDescriptors::get_group(const std::string& group_name) const {
    auto it = groups_.find(group_name);
    if (it != groups_.end()) return it->second;
    return {};
}

GroupDescriptor ParamDescriptors::get_group(const std::string& profile, const std::string& group_name) const {
    auto groups = merged_profile_groups(normalize_profile(profile), 0);
    auto group_it = groups.find(group_name);
    if (group_it != groups.end()) return group_it->second;
    return get_group(group_name);
}

std::vector<std::string> ParamDescriptors::get_group_names() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : groups_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ParamDescriptors::get_group_names(const std::string& profile) const {
    auto groups = merged_profile_groups(normalize_profile(profile), 0);
    if (groups.empty()) return get_group_names();

    std::vector<std::string> names;
    for (const auto& [name, _] : groups) {
        names.push_back(name);
    }
    return names;
}

std::vector<GroupDescriptor> ParamDescriptors::get_all_groups() const {
    std::vector<GroupDescriptor> result;
    for (const auto& [_, group] : groups_) {
        result.push_back(group);
    }
    return result;
}

std::vector<GroupDescriptor> ParamDescriptors::get_all_groups(const std::string& profile) const {
    auto groups = merged_profile_groups(normalize_profile(profile), 0);
    if (groups.empty()) return get_all_groups();

    std::vector<GroupDescriptor> result;
    for (const auto& [_, group] : groups) {
        result.push_back(group);
    }
    return result;
}

std::string ParamDescriptors::validate(const std::string& param_name, float value) const {
    auto desc = get(param_name);
    if (!desc.found) return ""; // No descriptor = no validation

    if (desc.type == "enum") {
        bool found = false;
        for (const auto& opt : desc.options) {
            if (opt.value == static_cast<int>(value)) {
                found = true;
                break;
            }
        }
        if (!found) return "Invalid option value: " + std::to_string(static_cast<int>(value));
    }

    if (desc.type == "int" || desc.type == "float") {
        if (desc.min_value != desc.max_value) {
            if (value < desc.min_value || value > desc.max_value) {
                return "Value must be between " + std::to_string(desc.min_value) +
                       " and " + std::to_string(desc.max_value);
            }
        }
    }

    if (desc.type == "bool") {
        if (value != 0.0f && value != 1.0f) {
            return "Must be 0 (false) or 1 (true)";
        }
    }

    return "";
}

std::string ParamDescriptors::validate(const std::string& profile,
                                       const std::string& param_name,
                                       float value) const {
    auto desc = get(profile, param_name);
    if (!desc.found) return "";

    if (desc.type == "enum") {
        bool found = false;
        for (const auto& opt : desc.options) {
            if (opt.value == static_cast<int>(value)) {
                found = true;
                break;
            }
        }
        if (!found) return "Invalid option value: " + std::to_string(static_cast<int>(value));
    }

    if (desc.type == "int" || desc.type == "float") {
        if (desc.min_value != desc.max_value) {
            if (value < desc.min_value || value > desc.max_value) {
                return "Value must be between " + std::to_string(desc.min_value) +
                       " and " + std::to_string(desc.max_value);
            }
        }
    }

    if (desc.type == "bool") {
        if (value != 0.0f && value != 1.0f) {
            return "Must be 0 (false) or 1 (true)";
        }
    }

    return "";
}

std::string ParamDescriptors::enum_label(const std::string& param_name, int value) const {
    auto desc = get(param_name);
    if (!desc.found) return "";

    for (const auto& opt : desc.options) {
        if (opt.value == value) return opt.label;
    }
    return "";
}

std::string ParamDescriptors::enum_label(const std::string& profile,
                                         const std::string& param_name,
                                         int value) const {
    auto desc = get(profile, param_name);
    if (!desc.found) return "";

    for (const auto& opt : desc.options) {
        if (opt.value == value) return opt.label;
    }
    return "";
}

// ================================================================
// JSON loading
// ================================================================

bool ParamDescriptors::load_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        MGC_LOG_ERROR("[ParamDesc] Cannot open %s", path.c_str());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) {
        MGC_LOG_ERROR("[ParamDesc] JSON parse error");
        return false;
    }

    cJSON* groups = cJSON_GetObjectItem(root, "groups");
    if (!groups || !cJSON_IsObject(groups)) {
        cJSON_Delete(root);
        return false;
    }

    if (!parse_groups(groups, descriptors_, groups_)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON* profiles = cJSON_GetObjectItem(root, "profiles");
    if (profiles && cJSON_IsObject(profiles)) {
        cJSON* profile_iter = nullptr;
        cJSON_ArrayForEach(profile_iter, profiles) {
            if (!profile_iter || !cJSON_IsObject(profile_iter) || !profile_iter->string) continue;
            cJSON* profile_groups = cJSON_GetObjectItem(profile_iter, "groups");
            auto profile_key = normalize_profile(profile_iter->string);

            cJSON* extends = cJSON_GetObjectItem(profile_iter, "extends");
            if (extends && cJSON_IsString(extends)) {
                profile_extends_[profile_key] = normalize_profile(extends->valuestring);
            }

            if (!profile_groups || !cJSON_IsObject(profile_groups)) continue;

            std::map<std::string, ParamDescriptor> descriptors;
            std::map<std::string, GroupDescriptor> groups_for_profile;
            if (parse_groups(profile_groups, descriptors, groups_for_profile)) {
                profile_descriptors_[profile_key] = std::move(descriptors);
                profile_groups_[profile_key] = std::move(groups_for_profile);
            }
        }
    }

    cJSON_Delete(root);
    return true;
}

bool ParamDescriptors::load_translation_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) {
        MGC_LOG_WARN("[ParamDesc] Translation JSON parse error: %s", path.c_str());
        return false;
    }

    cJSON* groups = cJSON_GetObjectItem(root, "groups");
    if (groups && cJSON_IsObject(groups)) {
        cJSON* group_iter = nullptr;
        cJSON_ArrayForEach(group_iter, groups) {
            apply_group_translation(group_iter, groups_);
            cJSON* params = cJSON_GetObjectItem(group_iter, "params");
            if (params && cJSON_IsObject(params)) {
                cJSON* param_iter = nullptr;
                cJSON_ArrayForEach(param_iter, params) {
                    apply_param_translation(param_iter, descriptors_);
                }
            }
        }
    }

    cJSON* profiles = cJSON_GetObjectItem(root, "profiles");
    if (profiles && cJSON_IsObject(profiles)) {
        cJSON* profile_iter = nullptr;
        cJSON_ArrayForEach(profile_iter, profiles) {
            if (!profile_iter || !profile_iter->string) continue;
            auto profile = normalize_profile(profile_iter->string);

            cJSON* profile_groups = cJSON_GetObjectItem(profile_iter, "groups");
            if (!profile_groups || !cJSON_IsObject(profile_groups)) continue;

            auto group_map = profile_groups_.find(profile);
            auto desc_map = profile_descriptors_.find(profile);
            if (group_map == profile_groups_.end() || desc_map == profile_descriptors_.end()) continue;

            cJSON* group_iter = nullptr;
            cJSON_ArrayForEach(group_iter, profile_groups) {
                apply_group_translation(group_iter, group_map->second);
                cJSON* params = cJSON_GetObjectItem(group_iter, "params");
                if (params && cJSON_IsObject(params)) {
                    cJSON* param_iter = nullptr;
                    cJSON_ArrayForEach(param_iter, params) {
                        apply_param_translation(param_iter, desc_map->second);
                    }
                }
            }
        }
    }

    cJSON_Delete(root);
    return true;
}

void ParamDescriptors::apply_group_translation(cJSON* group_obj,
                                               std::map<std::string, GroupDescriptor>& groups) {
    if (!group_obj || !group_obj->string) return;
    auto it = groups.find(group_obj->string);
    if (it == groups.end()) return;

    cJSON* display_name = cJSON_GetObjectItem(group_obj, "display_name");
    cJSON* description = cJSON_GetObjectItem(group_obj, "description");
    if (display_name && cJSON_IsString(display_name)) it->second.display_name = display_name->valuestring;
    if (description && cJSON_IsString(description)) it->second.description = description->valuestring;
}

void ParamDescriptors::apply_param_translation(cJSON* param_obj,
                                               std::map<std::string, ParamDescriptor>& descriptors) {
    if (!param_obj || !param_obj->string) return;
    auto it = descriptors.find(param_obj->string);
    if (it == descriptors.end()) return;

    cJSON* display_name = cJSON_GetObjectItem(param_obj, "display_name");
    cJSON* description = cJSON_GetObjectItem(param_obj, "description");
    if (display_name && cJSON_IsString(display_name)) it->second.display_name = display_name->valuestring;
    if (description && cJSON_IsString(description)) it->second.description = description->valuestring;

    cJSON* options = cJSON_GetObjectItem(param_obj, "options");
    if (!options || !cJSON_IsObject(options)) return;

    for (auto& option : it->second.options) {
        const auto key = std::to_string(option.value);
        cJSON* option_obj = cJSON_GetObjectItem(options, key.c_str());
        if (!option_obj || !cJSON_IsObject(option_obj)) continue;
        cJSON* label = cJSON_GetObjectItem(option_obj, "label");
        cJSON* option_desc = cJSON_GetObjectItem(option_obj, "description");
        if (label && cJSON_IsString(label)) option.label = label->valuestring;
        if (option_desc && cJSON_IsString(option_desc)) option.description = option_desc->valuestring;
    }
}

bool ParamDescriptors::parse_groups(cJSON* groups,
                                    std::map<std::string, ParamDescriptor>& descriptors,
                                    std::map<std::string, GroupDescriptor>& groups_out) const {
    cJSON* group_iter = nullptr;
    cJSON_ArrayForEach(group_iter, groups) {
        std::string group_name = group_iter->string;
        GroupDescriptor group;
        group.name = group_name;

        cJSON* display_name = cJSON_GetObjectItem(group_iter, "display_name");
        cJSON* description = cJSON_GetObjectItem(group_iter, "description");
        if (display_name && cJSON_IsString(display_name)) group.display_name = display_name->valuestring;
        if (description && cJSON_IsString(description)) group.description = description->valuestring;

        // Instance-based groups (SERIAL, SERVO, RC)
        cJSON* instance_prefix = cJSON_GetObjectItem(group_iter, "instance_prefix");
        cJSON* instance_count = cJSON_GetObjectItem(group_iter, "instance_count");
        cJSON* params_obj = cJSON_GetObjectItem(group_iter, "parameters");

        if (instance_prefix && instance_count && params_obj && cJSON_IsObject(params_obj)) {
            std::string prefix = instance_prefix->valuestring;
            int count = instance_count->valueint;

            cJSON* param_iter = nullptr;
            cJSON_ArrayForEach(param_iter, params_obj) {
                std::string suffix = param_iter->string; // e.g., "_PROTOCOL"
                cJSON* options = cJSON_GetObjectItem(param_iter, "options");
                cJSON* type = cJSON_GetObjectItem(param_iter, "type");
                cJSON* reboot = cJSON_GetObjectItem(param_iter, "requires_reboot");
                cJSON* desc_text = cJSON_GetObjectItem(param_iter, "description");
                cJSON* disp_name = cJSON_GetObjectItem(param_iter, "display_name");

                for (int i = 1; i <= count; i++) {
                    std::string param_name = prefix + std::to_string(i) + suffix;
                    ParamDescriptor desc;
                    desc.param_name = param_name;
                    desc.group = group_name;
                    desc.found = true;

                    if (disp_name && cJSON_IsString(disp_name)) {
                        desc.display_name = prefix + " " + std::to_string(i) + " " + disp_name->valuestring;
                    } else {
                        desc.display_name = param_name;
                    }

                    if (desc_text && cJSON_IsString(desc_text)) {
                        desc.description = desc_text->valuestring;
                    } else {
                        desc.description = "Parameter " + param_name;
                    }

                    if (type && cJSON_IsString(type)) desc.type = type->valuestring;
                    cJSON* category = cJSON_GetObjectItem(param_iter, "category");
                    cJSON* read_only = cJSON_GetObjectItem(param_iter, "read_only");
                    cJSON* increment = cJSON_GetObjectItem(param_iter, "increment");
                    cJSON* unit = cJSON_GetObjectItem(param_iter, "unit");
                    cJSON* min_value = cJSON_GetObjectItem(param_iter, "min");
                    cJSON* max_value = cJSON_GetObjectItem(param_iter, "max");
                    cJSON* default_value = cJSON_GetObjectItem(param_iter, "default");
                    if (category && cJSON_IsString(category)) desc.category = category->valuestring;
                    if (read_only && cJSON_IsBool(read_only)) desc.read_only = cJSON_IsTrue(read_only);
                    if (reboot && cJSON_IsBool(reboot)) desc.requires_reboot = cJSON_IsTrue(reboot);
                    if (increment && cJSON_IsNumber(increment)) desc.increment = static_cast<float>(increment->valuedouble);
                    if (unit && cJSON_IsString(unit)) desc.unit = unit->valuestring;
                    if (min_value && cJSON_IsNumber(min_value)) desc.min_value = static_cast<float>(min_value->valuedouble);
                    if (max_value && cJSON_IsNumber(max_value)) desc.max_value = static_cast<float>(max_value->valuedouble);
                    if (default_value && cJSON_IsNumber(default_value)) desc.default_value = static_cast<float>(default_value->valuedouble);

                    // Parse options
                    if (options && cJSON_IsArray(options)) {
                        cJSON* opt_iter = nullptr;
                        cJSON_ArrayForEach(opt_iter, options) {
                            ParamOption opt;
                            cJSON* v = cJSON_GetObjectItem(opt_iter, "value");
                            cJSON* l = cJSON_GetObjectItem(opt_iter, "label");
                            cJSON* d = cJSON_GetObjectItem(opt_iter, "description");
                            if (v && cJSON_IsNumber(v)) opt.value = v->valueint;
                            if (l && cJSON_IsString(l)) opt.label = l->valuestring;
                            if (d && cJSON_IsString(d)) opt.description = d->valuestring;
                            desc.options.push_back(opt);
                        }
                    }

                    descriptors[param_name] = desc;
                    group.param_names.push_back(param_name);
                }
            }
        }

        // Fixed-parameter groups (battery, failsafe, gps, etc.)
        cJSON* fixed_params = cJSON_GetObjectItem(group_iter, "fixed_params");
        if (fixed_params && cJSON_IsObject(fixed_params)) {
            cJSON* fp_iter = nullptr;
            cJSON_ArrayForEach(fp_iter, fixed_params) {
                std::string param_name = fp_iter->string;
                ParamDescriptor desc;
                desc.param_name = param_name;
                desc.group = group_name;
                desc.found = true;

                cJSON* dn = cJSON_GetObjectItem(fp_iter, "display_name");
                cJSON* dt = cJSON_GetObjectItem(fp_iter, "description");
                cJSON* tp = cJSON_GetObjectItem(fp_iter, "type");
                cJSON* mn = cJSON_GetObjectItem(fp_iter, "min");
                cJSON* mx = cJSON_GetObjectItem(fp_iter, "max");
                cJSON* df = cJSON_GetObjectItem(fp_iter, "default");
                cJSON* un = cJSON_GetObjectItem(fp_iter, "unit");
                cJSON* rb = cJSON_GetObjectItem(fp_iter, "requires_reboot");
                cJSON* ro = cJSON_GetObjectItem(fp_iter, "read_only");
                cJSON* cat = cJSON_GetObjectItem(fp_iter, "category");
                cJSON* inc = cJSON_GetObjectItem(fp_iter, "increment");
                cJSON* opts = cJSON_GetObjectItem(fp_iter, "options");

                if (dn && cJSON_IsString(dn)) desc.display_name = dn->valuestring;
                else desc.display_name = param_name;
                if (dt && cJSON_IsString(dt)) desc.description = dt->valuestring;
                else desc.description = "Parameter " + param_name;
                if (tp && cJSON_IsString(tp)) desc.type = tp->valuestring;
                else desc.type = "int";
                if (mn && cJSON_IsNumber(mn)) desc.min_value = static_cast<float>(mn->valuedouble);
                if (mx && cJSON_IsNumber(mx)) desc.max_value = static_cast<float>(mx->valuedouble);
                if (df && cJSON_IsNumber(df)) desc.default_value = static_cast<float>(df->valuedouble);
                if (un && cJSON_IsString(un)) desc.unit = un->valuestring;
                if (rb && cJSON_IsBool(rb)) desc.requires_reboot = cJSON_IsTrue(rb);
                if (ro && cJSON_IsBool(ro)) desc.read_only = cJSON_IsTrue(ro);
                if (cat && cJSON_IsString(cat)) desc.category = cat->valuestring;
                if (inc && cJSON_IsNumber(inc)) desc.increment = static_cast<float>(inc->valuedouble);

                if (opts && cJSON_IsArray(opts)) {
                    cJSON* opt_iter = nullptr;
                    cJSON_ArrayForEach(opt_iter, opts) {
                        ParamOption opt;
                        cJSON* v = cJSON_GetObjectItem(opt_iter, "value");
                        cJSON* l = cJSON_GetObjectItem(opt_iter, "label");
                        cJSON* d = cJSON_GetObjectItem(opt_iter, "description");
                        if (v && cJSON_IsNumber(v)) opt.value = v->valueint;
                        if (l && cJSON_IsString(l)) opt.label = l->valuestring;
                        if (d && cJSON_IsString(d)) opt.description = d->valuestring;
                        desc.options.push_back(opt);
                    }
                }

                descriptors[param_name] = desc;
                group.param_names.push_back(param_name);
            }
        }

        groups_out[group_name] = group;
    }
    return true;
}

std::string ParamDescriptors::find_json_file() const {
    std::vector<std::string> search_paths = {
        mg::core::platform::get_module_directory() +
            "../Resources/ParameterDescriptors.json",
        "ParameterDescriptors.json",
        "build/ParameterDescriptors.json",
        "core/src/config/ParameterDescriptors.json",
        "../core/src/config/ParameterDescriptors.json",
        "../../core/src/config/ParameterDescriptors.json",
    };

    for (const auto& path : search_paths) {
        std::ifstream f(path);
        if (f.is_open()) return path;
    }

    // Try using platform filesystem
    auto data_path = mg::core::platform::find_data_file("ParameterDescriptors.json");
    if (!data_path.empty()) return data_path;

    return "";
}

std::string ParamDescriptors::find_translation_file() const {
    std::vector<std::string> search_paths = {
        mg::core::platform::get_module_directory() +
            "../Resources/ParameterTranslations.zh-CN.json",
        "ParameterTranslations.zh-CN.json",
        "build/ParameterTranslations.zh-CN.json",
        "core/src/config/ParameterTranslations.zh-CN.json",
        "../core/src/config/ParameterTranslations.zh-CN.json",
        "../../core/src/config/ParameterTranslations.zh-CN.json",
    };

    for (const auto& path : search_paths) {
        std::ifstream f(path);
        if (f.is_open()) return path;
    }

    auto data_path = mg::core::platform::find_data_file("ParameterTranslations.zh-CN.json");
    if (!data_path.empty()) return data_path;

    return "";
}

std::string ParamDescriptors::normalize_profile(const std::string& profile) const {
    std::string result;
    result.reserve(profile.size());
    for (char c : profile) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (result == "fixedwing" || result == "fixed_wing" || result == "arduplane") return "plane";
    if (result == "arducopter" || result == "quadrotor" || result == "multicopter") return "copter";
    if (result == "quadplane") return "vtol";
    if (result.empty()) return "default";
    return result;
}

ParamDescriptor ParamDescriptors::get_from_profile(const std::string& profile,
                                                   const std::string& param_name,
                                                   int depth) const {
    if (depth > 4) return get(param_name);

    auto profile_it = profile_descriptors_.find(profile);
    if (profile_it != profile_descriptors_.end()) {
        auto param_it = profile_it->second.find(param_name);
        if (param_it != profile_it->second.end()) return param_it->second;
    }

    auto extends_it = profile_extends_.find(profile);
    if (extends_it != profile_extends_.end()) {
        auto inherited = get_from_profile(extends_it->second, param_name, depth + 1);
        if (inherited.found) return inherited;
    }

    return get(param_name);
}

std::map<std::string, GroupDescriptor> ParamDescriptors::merged_profile_groups(const std::string& profile,
                                                                              int depth) const {
    if (depth > 4) return {};

    std::map<std::string, GroupDescriptor> result;
    auto extends_it = profile_extends_.find(profile);
    if (extends_it != profile_extends_.end()) {
        result = merged_profile_groups(extends_it->second, depth + 1);
    }

    auto profile_it = profile_groups_.find(profile);
    if (profile_it != profile_groups_.end()) {
        for (const auto& [name, group] : profile_it->second) {
            result[name] = group;
        }
    }
    return result;
}

} // namespace mg::core::config
