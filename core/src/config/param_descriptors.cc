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

#include "cJSON.h"
#include "core/util/json.h"

namespace mg::core::config {

namespace {

bool contains_non_ascii(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= 0x80;
    });
}

std::vector<std::string> split_parameter_name(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, '_')) {
        if (!token.empty()) result.push_back(token);
    }
    return result;
}
std::string translate_parameter_token(
    const std::string& token,
    const std::map<std::string, std::string>& terms) {
    auto exact = terms.find(token);
    if (exact != terms.end()) return exact->second;

    size_t first_digit = token.size();
    while (first_digit > 0 && std::isdigit(static_cast<unsigned char>(token[first_digit - 1]))) {
        first_digit--;
    }
    if (first_digit > 0 && first_digit < token.size()) {
        auto base = terms.find(token.substr(0, first_digit));
        if (base != terms.end()) {
            return base->second + " " + token.substr(first_digit);
        }
    }

    size_t first_alpha = 0;
    while (first_alpha < token.size() &&
           std::isdigit(static_cast<unsigned char>(token[first_alpha]))) {
        first_alpha++;
    }
    if (first_alpha > 0 && first_alpha < token.size()) {
        auto base = terms.find(token.substr(first_alpha));
        if (base != terms.end()) {
            return token.substr(0, first_alpha) + " " + base->second;
        }
    }
    return token;
}

} // namespace

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
    const auto normalized = normalize_profile(profile);
    return apply_fallback_translation(normalized, get_from_profile(normalized, param_name, 0));
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
    const auto normalized = normalize_profile(profile);
    auto groups = merged_profile_groups(normalized, 0);
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

ParamCoverage ParamDescriptors::coverage(
    const std::string& profile,
    const std::vector<std::string>& parameter_names) const {
    ParamCoverage result;
    result.total = static_cast<int>(parameter_names.size());
    for (const auto& name : parameter_names) {
        if (get(profile, name).found) {
            result.described++;
        } else {
            result.fallback++;
            result.missing.push_back(name);
        }
    }
    std::sort(result.missing.begin(), result.missing.end());
    return result;
}

std::string ParamDescriptors::validate(const std::string& param_name, float value) const {
    auto desc = get(param_name);
    if (!desc.found) return ""; // No descriptor = no validation

    if (desc.read_only) return "Parameter is read-only";

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

    if (desc.read_only) return "Parameter is read-only";

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

    cJSON* root = mg::core::json::parse(content.c_str());
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

    load_roles(root);

    cJSON_Delete(root);
    return true;
}

/// 读取 `roles` 段：角色名由前端定义（与固件无关），参数名由插件提供（S6）。
///
/// 结构：
///   "roles": { "profiles": { "ardupilot": {"roles": {...}},
///                            "copter": {"extends": "ardupilot", "roles": {...}} } }
/// 缺失该段不是错误：没有角色表的固件继续用其它已登记接口。
void ParamDescriptors::load_roles(cJSON* root) {
    if (!root) return;
    cJSON* roles = cJSON_GetObjectItem(root, "roles");
    if (!roles || !cJSON_IsObject(roles)) return;

    cJSON* profiles = cJSON_GetObjectItem(roles, "profiles");
    if (!profiles || !cJSON_IsObject(profiles)) return;

    cJSON* profile_iter = nullptr;
    cJSON_ArrayForEach(profile_iter, profiles) {
        if (!profile_iter || !cJSON_IsObject(profile_iter) || !profile_iter->string) continue;
        const auto profile_key = normalize_profile(profile_iter->string);

        // `extends` 只在角色表内部解析，与描述符的 `profiles.<p>.extends`
        // 相互独立（如 ardupilot 这一层在描述符里并不存在）。
        cJSON* extends = cJSON_GetObjectItem(profile_iter, "extends");
        if (extends && cJSON_IsString(extends) && extends->valuestring) {
            role_extends_[profile_key] = normalize_profile(extends->valuestring);
        }

        cJSON* profile_roles = cJSON_GetObjectItem(profile_iter, "roles");
        if (!profile_roles || !cJSON_IsObject(profile_roles)) continue;

        auto& target = role_sets_[profile_key];
        cJSON* role_iter = nullptr;
        cJSON_ArrayForEach(role_iter, profile_roles) {
            if (!role_iter || !role_iter->string) continue;
            if (!cJSON_IsString(role_iter) || !role_iter->valuestring) continue;
            if (role_iter->string[0] == '\0') continue;
            target[role_iter->string] = role_iter->valuestring;
        }
    }

    size_t total = 0;
    for (const auto& entry : role_sets_) total += entry.second.size();
    if (total > 0) {
        MGC_LOG_INFO("[ParamDesc] Loaded %zu parameter roles across %zu profiles",
                     total, role_sets_.size());
    }
}

std::map<std::string, std::string> ParamDescriptors::roles(const std::string& profile) const {
    // 从叶子沿 `extends` 冒泡到根，再自根向叶子合并：子 profile 覆盖父 profile。
    std::vector<std::string> chain;
    std::set<std::string> seen;
    std::string current = normalize_profile(profile);
    for (int depth = 0; depth < 8 && !current.empty(); depth++) {
        if (!seen.insert(current).second) break;  // 配置里的环：到此为止
        chain.push_back(current);
        auto extends = role_extends_.find(current);
        if (extends == role_extends_.end()) break;
        current = extends->second;
    }

    std::map<std::string, std::string> merged;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto own = role_sets_.find(*it);
        if (own == role_sets_.end()) continue;
        for (const auto& role : own->second) merged[role.first] = role.second;
    }
    return merged;
}

bool ParamDescriptors::load_translation_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    cJSON* root = mg::core::json::parse(content.c_str());
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

    cJSON* fallback = cJSON_GetObjectItem(root, "fallback");
    if (fallback && cJSON_IsObject(fallback)) {
        cJSON* fallback_profiles = cJSON_GetObjectItem(fallback, "profiles");
        if (fallback_profiles && cJSON_IsArray(fallback_profiles)) {
            cJSON* profile = nullptr;
            cJSON_ArrayForEach(profile, fallback_profiles) {
                if (cJSON_IsString(profile)) {
                    fallback_translation_profiles_.insert(normalize_profile(profile->valuestring));
                }
            }
        }

        cJSON* terms = cJSON_GetObjectItem(fallback, "terms");
        if (terms && cJSON_IsObject(terms)) {
            cJSON* term = nullptr;
            cJSON_ArrayForEach(term, terms) {
                if (term->string && cJSON_IsString(term)) {
                    fallback_translation_terms_[term->string] = term->valuestring;
                }
            }
        }

        cJSON* option_labels = cJSON_GetObjectItem(fallback, "option_labels");
        if (option_labels && cJSON_IsObject(option_labels)) {
            cJSON* label = nullptr;
            cJSON_ArrayForEach(label, option_labels) {
                if (label->string && cJSON_IsString(label)) {
                    fallback_option_labels_[label->string] = label->valuestring;
                }
            }
        }

        cJSON* option_terms = cJSON_GetObjectItem(fallback, "option_terms");
        if (option_terms && cJSON_IsObject(option_terms)) {
            cJSON* term = nullptr;
            cJSON_ArrayForEach(term, option_terms) {
                if (term->string && cJSON_IsString(term)) {
                    fallback_option_terms_[term->string] = term->valuestring;
                }
            }
            fallback_option_terms_sorted_.assign(
                fallback_option_terms_.begin(), fallback_option_terms_.end());
            std::sort(fallback_option_terms_sorted_.begin(),
                      fallback_option_terms_sorted_.end(),
                      [](const auto& left, const auto& right) {
                          return left.first.size() > right.first.size();
                      });
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
                            if (l && cJSON_IsString(l)) {
                                opt.label = l->valuestring;
                                opt.english_label = opt.label;
                            }
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
                        if (l && cJSON_IsString(l)) {
                            opt.label = l->valuestring;
                            opt.english_label = opt.label;
                        }
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

std::string ParamDescriptors::find_data_path(const std::string& filename,
                                               const std::string& anchor_path) const {
    std::vector<std::string> search_paths;

    // Same-directory lookup first: the main descriptor JSON and every companion
    // data file ship next to each other in the bundle.
    if (!anchor_path.empty()) {
        const auto slash = anchor_path.find_last_of('/');
        const std::string dir =
            (slash == std::string::npos) ? std::string() : anchor_path.substr(0, slash + 1);
        search_paths.push_back(dir + filename);
    }
    search_paths.push_back(
        mg::core::platform::get_module_directory() + "../Resources/" + filename);
    search_paths.push_back(filename);
    search_paths.push_back("build/" + filename);
    search_paths.push_back("core/src/config/" + filename);
    search_paths.push_back("../core/src/config/" + filename);
    search_paths.push_back("../../core/src/config/" + filename);

    for (const auto& path : search_paths) {
        std::ifstream f(path);
        if (f.is_open()) return path;
    }

    // Try using platform filesystem
    auto data_path = mg::core::platform::find_data_file(filename.c_str());
    if (!data_path.empty()) return data_path;

    return "";
}

std::string ParamDescriptors::find_json_file() const {
    return find_data_path("ParameterDescriptors.json", "");
}

std::string ParamDescriptors::find_translation_file() const {
    return find_data_path("ParameterTranslations.zh-CN.json", "");
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

ParamDescriptor ParamDescriptors::apply_fallback_translation(
    const std::string& profile,
    ParamDescriptor descriptor) const {
    if (!descriptor.found ||
        fallback_translation_profiles_.find(profile) == fallback_translation_profiles_.end()) {
        return descriptor;
    }

    if (!contains_non_ascii(descriptor.display_name)) {
        descriptor.display_name = fallback_display_name(profile, descriptor);
    }

    if (!contains_non_ascii(descriptor.description)) {
        // 刻意**不回填** ArduPilot 的英文原文：那份 @Description 文本是 GPLv3 源码的
        // 一部分，与 M-GCS 的 MIT 许可不兼容，不得进入本仓库或随包分发。
        // 需要英文原始说明时，参照 Mission Planner 的做法在运行时从上游获取。
        // 见 core/src/config/ParameterDescriptors.json 的 metadata_note。
        descriptor.description = "用于配置“" + descriptor.display_name +
                                 "”。修改前请核对参数范围、单位和 ArduPilot 官方说明。";
    }

    for (auto& option : descriptor.options) {
        option.label = fallback_option_label(option.label, option.english_label);
    }
    return descriptor;
}

std::string ParamDescriptors::fallback_option_label(
    const std::string& label,
    const std::string& english_label) const {
    std::string translated = label;
    if (!contains_non_ascii(translated)) {
        auto exact = fallback_option_labels_.find(translated);
        if (exact != fallback_option_labels_.end()) {
            translated = exact->second;
        } else {
            for (const auto& [english, chinese] : fallback_option_terms_sorted_) {
                size_t offset = 0;
                while ((offset = translated.find(english, offset)) != std::string::npos) {
                    translated.replace(offset, english.size(), chinese);
                    offset += chinese.size();
                }
            }
        }
    }

    const auto& original = english_label.empty() ? label : english_label;
    if (!contains_non_ascii(translated) || original.empty() ||
        translated.find(original) != std::string::npos) {
        return translated;
    }
    return translated + " (" + original + ")";
}

std::string ParamDescriptors::fallback_group_display_name(
    const std::string& profile,
    const std::string& group,
    int depth) const {
    if (depth > 4) return group;

    auto profile_it = profile_groups_.find(profile);
    if (profile_it != profile_groups_.end()) {
        auto group_it = profile_it->second.find(group);
        if (group_it != profile_it->second.end() && !group_it->second.display_name.empty()) {
            return group_it->second.display_name;
        }
    }

    auto extends_it = profile_extends_.find(profile);
    if (extends_it != profile_extends_.end()) {
        return fallback_group_display_name(extends_it->second, group, depth + 1);
    }
    return group;
}

std::string ParamDescriptors::fallback_display_name(
    const std::string& profile,
    const ParamDescriptor& descriptor) const {
    std::string group_label = fallback_group_display_name(profile, descriptor.group, 0);
    if (group_label.empty()) group_label = descriptor.group;

    std::string remainder = descriptor.param_name;
    if (!descriptor.group.empty() && remainder.rfind(descriptor.group, 0) == 0) {
        remainder.erase(0, descriptor.group.size());
    }
    while (!remainder.empty() && remainder.front() == '_') remainder.erase(0, 1);

    std::string translated_remainder;
    for (const auto& token : split_parameter_name(remainder)) {
        if (!translated_remainder.empty()) translated_remainder += " ";
        translated_remainder += translate_parameter_token(token, fallback_translation_terms_);
    }

    if (group_label.empty()) return translated_remainder.empty() ? descriptor.param_name : translated_remainder;
    if (translated_remainder.empty()) return group_label;
    return group_label + " · " + translated_remainder;
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
