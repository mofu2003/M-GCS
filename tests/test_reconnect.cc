/**
 * test_reconnect.cc - 终端仪表盘，测试断开重连
 *
 * 用法：test_reconnect.exe COM6
 *
 * 操作：拔掉 USB → 观察心跳丢失 + 自动重连 → 插回 → 观察恢复
 *       Ctrl+C 或 60 秒后自动退出
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <atomic>

static std::atomic<bool> g_running{true};

static const char* state_name(int s) {
    switch (s) {
        case 0: return "DISCONNECTED";
        case 1: return "CONNECTED";
        case 2: return "HEARTBEAT LOST";
        default: return "UNKNOWN";
    }
}

// 简单 JSON 字段提取
static bool jf_str(const char* json, const char* key, char* out, int n) {
    char s[64]; snprintf(s, sizeof(s), "\"%s\"", key);
    const char* p = strstr(json, s);
    if (!p) return false;
    p += strlen(s);
    while (*p == ':' || *p == '"' || *p == ' ') p++;
    int i = 0;
    while (*p && *p != '"' && *p != ',' && *p != '}' && i < n-1) out[i++] = *p++;
    out[i] = 0;
    return i > 0;
}
static float jf_f(const char* json, const char* key, float d) {
    char b[32]; return jf_str(json, key, b, sizeof(b)) ? strtof(b, nullptr) : d;
}
static int jf_i(const char* json, const char* key, int d) {
    char b[32]; return jf_str(json, key, b, sizeof(b)) ? (int)strtol(b, nullptr, 10) : d;
}

int main(int argc, char** argv) {
    const char* endpoint = nullptr;
    if (argc > 1) endpoint = argv[1];
    else endpoint = getenv("MG_TEST_SERIAL");

    if (!endpoint || !endpoint[0]) {
        printf("用法: test_reconnect.exe <端口>\n");
        printf("  串口: COM6\n");
        printf("  TCP:  tcp://192.168.1.1:5760\n");
        return 1;
    }

    char uri[128];
    if (strstr(endpoint, "://")) snprintf(uri, sizeof(uri), "%s", endpoint);
    else snprintf(uri, sizeof(uri), "serial://%s?baud=115200", endpoint);

    printf("=== mgroundcontrol 连接测试 ===\n");
    printf("目标: %s\n", uri);

    mg_core_t* core = mg_create();
    if (!core) { printf("FAIL: mg_create\n"); return 1; }

    printf("正在连接...\n");
    int vid = mg_vehicle_open(core, uri);
    if (vid < 0) { printf("连接失败！\n"); mg_destroy(core); return 1; }
    printf("车辆 ID: %d\n\n", vid);

    auto start = std::chrono::steady_clock::now();
    int last_state = -1;
    int reconnect_count = 0;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 直接轮询连接状态（比事件回调更可靠）
        char* state_json = mg_connection_get_state(core);
        int state = 0;  // disconnected
        if (state_json) {
            if (strstr(state_json, "\"connected\":true"))
                state = 1;
            else if (strstr(state_json, "\"heartbeat_lost\":true"))
                state = 2;
            mg_free_string(state_json);
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();

        // 状态变化提示
        if (state != last_state) {
            if (state == 1 && last_state != -1) {
                reconnect_count++;
                printf("\n** 已重新连接！ (第 %d 次) **\n\n", reconnect_count);
            } else if (state == 0 && last_state == 1) {
                printf("\n** 连接断开 **\n\n");
            } else if (state == 2) {
                printf("\n** 心跳丢失，尝试重连... **\n\n");
            }
            last_state = state;
        }

        printf("--- [%s] t=%llds ---\n", state_name(state), static_cast<long long>(elapsed));

        if (state == 1) {
            // 已连接：显示遥测
            char* att = mg_vehicle_get_attitude(core);
            char* gps = mg_vehicle_get_gps(core);
            char* bat = mg_vehicle_get_battery(core);
            char* vfr = mg_vehicle_get_vfr_hud(core);
            char* sum = mg_vehicle_get_summary(core);

            char mode[32] = "?"; bool armed = false;
            if (sum) {
                jf_str(sum, "flight_mode", mode, sizeof(mode));
                char a[8]; if (jf_str(sum, "armed", a, sizeof(a))) armed = (strcmp(a, "true") == 0);
            }

            float roll=0, pitch=0;
            if (att) { roll = jf_f(att, "roll_deg", 0); pitch = jf_f(att, "pitch_deg", 0); }

            int fix=0, sats=0;
            if (gps) { fix = jf_i(gps, "fix_type", 0); sats = jf_i(gps, "satellites", 0); }

            float volt=0; int remain=-1;
            if (bat) { volt = jf_f(bat, "voltage_v", 0); remain = jf_i(bat, "remaining_pct", -1); }

            float aspd=0, gspd=0, alt=0; int hdg=0, thr=0;
            if (vfr) { aspd = jf_f(vfr, "airspeed_m_s", 0); gspd = jf_f(vfr, "groundspeed_m_s", 0);
                       alt = jf_f(vfr, "alt_msl_m", 0); hdg = jf_i(vfr, "heading_deg", 0); thr = jf_i(vfr, "throttle_pct", 0); }

            printf("  模式: %-14s 解锁: %s\n", mode, armed ? "YES" : "NO");
            printf("  姿态: roll %+6.1f  pitch %+6.1f  yaw %3d\n", roll, pitch, hdg);
            printf("  速度: 空速 %5.1f m/s  地速 %5.1f m/s  油门 %d%%\n", aspd, gspd, thr);
            printf("  高度: %6.1f m\n", alt);
            printf("  GPS:  %s  卫星 %d\n", fix >= 3 ? "3D fix" : (fix >= 2 ? "2D" : "无"), sats);
            char remain_str[16];
            if (remain >= 0) snprintf(remain_str, sizeof(remain_str), "%d%%", remain);
            else snprintf(remain_str, sizeof(remain_str), "unknown");
            printf("  电池: %5.1fV  剩余 %s\n", volt, remain_str);

            mg_free_string(att); mg_free_string(gps); mg_free_string(bat);
            mg_free_string(vfr); mg_free_string(sum);

        } else {
            printf("  等待重连... (已重连 %d 次)\n", reconnect_count);
            printf("  提示：拔掉 USB 测试断连，再插回测试重连\n");
        }

        if (elapsed > 60) { printf("\n60 秒超时，退出。\n"); break; }
    }

    printf("\n断开连接...\n");
    mg_vehicle_close(core, vid);
    mg_destroy(core);
    printf("完成。\n");
    return 0;
}
