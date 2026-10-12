/**
 * test_terrain.cc - Terrain elevation cache and line sampling tests
 *
 * 地形层不发起网络请求：高程由前端 provider 拉取后经 mg_terrain_cache_update
 * 写入。本测试覆盖缓存写入/查询、行剖面采样、缺值上报与清空，以及
 * mg_terrain_query / mg_terrain_query_line 的 FFI 数据契约（valid/missing/complete）。
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "core/terrain/terrain_store.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include "mgcs_test_check.h"

using namespace mg::core::terrain;

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key) != nullptr;
}

/// 读取 `"key":<number>` 的数值；找不到则返回 NAN。
static double json_number(const char* json, const char* key) {
    if (!json) return NAN;
    std::string needle = std::string("\"") + key + "\":";
    const char* p = strstr(json, needle.c_str());
    if (!p) return NAN;
    return atof(p + needle.size());
}

// ================================================================
// Unit: TerrainStore cache + line sampling
// ================================================================

static void test_store_cache() {
    fprintf(stderr, "\n[Group] TerrainStore cache\n");

    TerrainStore store;
    CHECK(store.size() == 0, "new store is empty");
    CHECK(!store.lookup(39.9, 116.4).has_value(), "lookup on empty store misses");

    store.store(39.9, 116.4, 52.0f);
    auto hit = store.lookup(39.9, 116.4);
    CHECK(hit.has_value(), "store then lookup hits");
    CHECK(hit.has_value() && std::fabs(hit.value() - 52.0f) < 0.01f, "cached elevation matches");

    // 0.01 degree grid: 39.9001 rounds to the same cell as 39.9.
    auto same_cell = store.lookup(39.9001, 116.4001);
    CHECK(same_cell.has_value(), "coordinate within the same 0.01 deg cell hits");

    CHECK(store.size() == 1, "distinct cells counted separately");
    CHECK(store.hit_rate() > 0.0, "hit rate reflects hits");

    store.clear();
    CHECK(store.size() == 0, "clear empties the cache");
}

static void test_store_batch() {
    fprintf(stderr, "\n[Group] TerrainStore bulk write\n");

    TerrainStore store;
    std::vector<TerrainPoint> batch;
    for (int i = 0; i < 5; ++i) {
        TerrainPoint pt;
        pt.lat_deg = 30.0 + i * 0.1;
        pt.lon_deg = 110.0 + i * 0.1;
        pt.elevation_m = static_cast<float>(100 + i);
        pt.valid = true;
        batch.push_back(pt);
    }
    // 无效点应被跳过。
    TerrainPoint bad;
    bad.lat_deg = 1.0;
    bad.lon_deg = 1.0;
    bad.valid = false;
    batch.push_back(bad);

    size_t stored = store.store_batch(batch);
    CHECK(stored == 5, "store_batch skips invalid points");
    CHECK(store.size() == 5, "store_batch caches every valid point");
}

static void test_sample_line() {
    fprintf(stderr, "\n[Group] TerrainStore line sampling\n");

    TerrainStore store;
    // 只缓存线的两端点，中间的采样点应当被上报为缺失。
    store.store(30.0, 110.0, 10.0f);
    store.store(30.5, 110.5, 20.0f);

    TerrainProfile profile = store.sample_line(30.0, 110.0, 30.5, 110.5, 11);
    CHECK(profile.points.size() == 11, "sample_line returns requested sample count");
    CHECK(!profile.complete(), "profile is incomplete when interior samples miss");
    CHECK(profile.missing.size() == 9, "only the two cached endpoints are valid");
    CHECK(profile.points.front().valid && std::fabs(profile.points.front().elevation_m - 10.0f) < 0.01f,
          "first sample uses cached elevation");
    CHECK(profile.points.back().valid && std::fabs(profile.points.back().elevation_m - 20.0f) < 0.01f,
          "last sample uses cached elevation");
    CHECK(!profile.points[5].valid, "midpoint is flagged invalid while missing");

    // 填充剩下来的采样点后，剖面应完整。
    for (size_t idx : profile.missing) {
        const auto& pt = profile.points[idx];
        store.store(pt.lat_deg, pt.lon_deg, 15.0f);
    }
    TerrainProfile refilled = store.sample_line(30.0, 110.0, 30.5, 110.5, 11);
    CHECK(refilled.complete(), "profile is complete after refill");
    CHECK(refilled.missing.empty(), "no missing indices once cached");

    // 采样数下限保护：<2 也至少给出 2 个点。
    TerrainProfile tiny = store.sample_line(30.0, 110.0, 30.5, 110.5, 1);
    CHECK(tiny.points.size() == 2, "sample_line clamps to a minimum of two samples");
}

// ================================================================
// FFI: mg_terrain_query / mg_terrain_query_line / cache update/clear
// ================================================================

static void test_terrain_ffi() {
    fprintf(stderr, "\n[Group] Terrain FFI bridge\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // 空缓存：valid=false，elevation=null，不是 success:false。
    char* r = mg_terrain_query(core, 39.9, 116.4);
    CHECK(r != nullptr, "mg_terrain_query returns non-null on miss");
    CHECK(json_has(r, "\"valid\":false"), "miss reports valid=false");
    CHECK(json_has(r, "\"elevation\":null"), "miss reports elevation=null");
    CHECK(!json_has(r, "\"success\":false"), "miss is a data answer, not an error");
    if (r) mg_free_string(r);

    // 批量写入后应命中。
    const char* update =
        R"({"points":[{"lat":39.9,"lon":116.4,"elevation":52.42},)"
        R"({"lat":38.0,"lon":100.0,"elevation":60.0},)"
        R"({"lat":"bad","lon":116.4,"elevation":1.0}]})";
    r = mg_terrain_cache_update(core, update);
    CHECK(r != nullptr, "mg_terrain_cache_update returns non-null");
    CHECK(json_has(r, "\"stored\":2"), "malformed entry is skipped, two stored");
    CHECK(json_has(r, "\"cache_size\":2"), "cache_size reflects stored points");
    if (r) mg_free_string(r);

    r = mg_terrain_query(core, 39.9, 116.4);
    CHECK(json_has(r, "\"valid\":true"), "cached query reports valid=true");
    CHECK(std::fabs(json_number(r, "elevation") - 52.42) < 0.01, "cached query returns the elevation");
    if (r) mg_free_string(r);

    // 补上更长剖面的另一个端点，构造 0.1 度长、11 个采样点的线。
    r = mg_terrain_cache_update(core, R"({"points":[{"lat":40.0,"lon":116.5,"elevation":70.0}]})");
    if (r) mg_free_string(r);

    // 行剖面：端点命中，中间缺失。
    r = mg_terrain_query_line(core, 39.9, 116.4, 40.0, 116.5, 11);
    CHECK(r != nullptr, "mg_terrain_query_line returns non-null");
    CHECK(json_has(r, "\"complete\":false"), "partial cache yields complete=false");
    CHECK(json_has(r, "\"missing_count\":9"), "missing_count reports interior samples");
    CHECK(json_has(r, "\"points\":["), "profile carries a points array");
    CHECK(json_has(r, "\"missing\":["), "profile carries a missing index array");
    if (r) mg_free_string(r);

    // 非法 JSON 走错误契约。
    r = mg_terrain_cache_update(core, "{bad json");
    CHECK(r != nullptr && json_has(r, "\"success\":false"), "invalid JSON returns success=false");
    if (r) mg_free_string(r);

    r = mg_terrain_cache_update(core, R"({"nope":[]})");
    CHECK(r != nullptr && json_has(r, "\"success\":false"), "missing points array returns success=false");
    if (r) mg_free_string(r);

    // 清空后查询应重新回到未命中。
    r = mg_terrain_cache_clear(core);
    CHECK(r != nullptr && json_has(r, "\"success\":true"), "cache_clear returns success");
    if (r) mg_free_string(r);

    r = mg_terrain_query(core, 39.9, 116.4);
    CHECK(json_has(r, "\"valid\":false"), "cache is empty after clear");
    if (r) mg_free_string(r);

    // mg_destroy 必须在清空缓存后仍然干净（TerrainStore 挂在 core 上）。
    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== Terrain Simulation Tests ===\n");
    test_store_cache();
    test_store_batch();
    test_sample_line();
    test_terrain_ffi();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
