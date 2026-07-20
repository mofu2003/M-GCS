#define MGCORE_EXPORTS
/**
 * ffi_param_file.cc - Parameter file I/O
 *
 * 参数文件导入导出，.param 格式读写。
 *
 * mg_params_save_to_file, mg_params_load_from_file
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "core/config/param_file_io.h"

#include <string>
#include <vector>

MGCORE_API char* mg_params_save_to_file(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0')
        return fail(MG_ERROR_INVALID_PARAM, "null filepath");

    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "not connected");

    // Convert cached params to ParamEntry format
    auto all = mav->all_params();
    std::vector<mg::core::config::ParamEntry> entries;
    for (auto& p : all) {
        mg::core::config::ParamEntry e;
        e.name = p.name;
        e.value = p.value;
        e.type = p.type;
        entries.push_back(e);
    }

    // Export to .param format
    std::string content = mg::core::config::ParamFileIO::export_param(entries);

    // Write to file
    FILE* f = fopen(filepath, "w");
    if (!f) return fail(MG_ERROR_SEND_FAILED, "cannot open file for writing");
    size_t written = fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    if (written != content.size())
        return fail(MG_ERROR_SEND_FAILED, "incomplete write");
    return ok();
}

MGCORE_API char* mg_params_load_from_file(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0')
        return fail(MG_ERROR_INVALID_PARAM, "null filepath");

    // Read file
    FILE* f = fopen(filepath, "r");
    if (!f) return fail(MG_ERROR_INVALID_PARAM, "cannot open file");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return fail(MG_ERROR_INVALID_PARAM, "cannot determine file size");
    }
    fseek(f, 0, SEEK_SET);
    std::string content(static_cast<size_t>(size), '\0');
    size_t nread = fread(&content[0], 1, static_cast<size_t>(size), f);
    fclose(f);
    if (nread != static_cast<size_t>(size))
        return fail(MG_ERROR_SEND_FAILED, "incomplete read");

    auto params = mg::core::config::ParamFileIO::import_param(content);
    if (params.empty())
        return fail(MG_ERROR_INVALID_PARAM, "no params found in file");

    // Apply parsed parameters to the vehicle
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "not connected");

    int applied = 0;
    for (const auto& p : params) {
        if (mav->set_param(active_target_sys(core), active_target_comp(core),
                           p.name.c_str(), p.value, p.type)) {
            applied++;
        }
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("count", (int)params.size());
    jb.field("applied", applied);
    jb.end_object();
    return to_cstr(jb.str());
}
