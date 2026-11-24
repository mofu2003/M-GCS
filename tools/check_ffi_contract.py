#!/usr/bin/env python3
"""Check the public API, Dart bindings and optional compiled library."""
import argparse
import ctypes
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent.parent
UNBOUND_BASELINE = ROOT / "tools/ffi_unbound_baseline.json"
# 目录里的文件只要提到符号名就算"有测试引用"。这是粗粒度代理：它证明存在
# 一个会加载/调用该导出的测试或手工脚本，不代表行为已被完整覆盖。
TEST_ROOTS = ("core/tests", "tests", "flutter_shell/test")
# 公开导出的定位表。若分类符号被 Dart 绑定或引用了不存在的导出，检查会要求重新评估定位。
INTERNAL_SYMBOL_NOTES = {
    "mg_error_string": "ABI 基础设施：错误码→字符串常量，供原生调用方与测试使用，无 UI",
}

# 公开导出但已被更新的接口取代的历史路径。原先这些符号保留在 ABI 中（退役候选）。
# 现已全部物理删除，登记进 RETIRED_SYMBOLS 防止误恢复；本表留作结构性占位，
# 未来若出现“已被取代但暂不删除”的导出，可在此登记定位说明。
SUPERSEDED_SYMBOL_NOTES = {}

# 公开功能保留在当前 ABI 中，但按版本范围延期到 0.3.0 之后。
DEFERRED_SYMBOL_NOTES = {
}

# 旧 telemetry series 记录器的退役候选。仅作治理标记，不代表已确认没有仓库外
# ABI 使用者，也不在本次删除 ABI。
RETIREMENT_CANDIDATE_NOTES = {
}


def classified_symbols():
    """返回 {symbol: (类别, 定位)}，覆盖所有已定性的公开导出。"""
    notes = {}
    for symbol, note in INTERNAL_SYMBOL_NOTES.items():
        notes[symbol] = ("内部能力", note)
    for symbol, note in SUPERSEDED_SYMBOL_NOTES.items():
        notes[symbol] = ("被取代", note)
    for symbol, note in DEFERRED_SYMBOL_NOTES.items():
        notes[symbol] = ("0.3.0 之后延期", note)
    for symbol, note in RETIREMENT_CANDIDATE_NOTES.items():
        notes[symbol] = ("退役候选", note)
    return notes


RETIRED_SYMBOLS = {
    "mg_cal_start", "mg_cal_cancel", "mg_cal_status", "mg_cal_history",
    "mg_cal_retry_info", "mg_cmd_arm", "mg_cmd_disarm", "mg_cmd_set_mode",
    "mg_cmd_rtl", "mg_cmd_takeoff", "mg_cmd_land", "mg_cmd_loiter",
    "mg_cmd_guided", "mg_command_emergency_stop", "mg_param_list",
    "mg_param_get", "mg_param_set", "mg_logging_get_entries",
    "mg_logging_get_stats", "mg_log_list", "mg_mission_request",
    "mg_firmware_request_version", "mg_firmware_get_version",
    "mg_geofence_get", "mg_geofence_set",
    # 2026-10 PX4/围栏/测绘/地形删除：这些导出已从源码物理删除
    "mg_geofence_get_config",
    "mg_geofence_check",
    "mg_fence_enable",
    "mg_survey_generate",
    "mg_survey_generate_corridor",
    "mg_terrain_query",
    "mg_terrain_query_line",
    "mg_terrain_cache_update",
    "mg_terrain_cache_clear",
    "mg_px4_actuator_get_config",
    "mg_px4_actuator_set_config",
    "mg_px4_flight_mode_get_config",
    "mg_px4_safety_get",
    "mg_px4_safety_set",
    "mg_px4_tuning_get",
    "mg_px4_tuning_set",
    # 2026-10 死代码清理：旧 MissionManager / 参数文件 / 围栏直编 / 连接 / 串口 / RC 校准导出
    "mg_mission_add_item",
    "mg_mission_update_item",
    "mg_mission_remove_item",
    "mg_mission_move_item",
    "mg_mission_validate",
    "mg_mission_get_stats",
    "mg_mission_export_json",
    "mg_mission_import_json",
    "mg_mission_export_csv",
    "mg_mission_import_csv",
    "mg_mission_get_templates",
    "mg_mission_start",
    "mg_params_save_to_file",
    "mg_params_load_from_file",
    "mg_params_get_changed",
    "mg_geofence_set_config",
    "mg_geofence_add_point",
    "mg_geofence_remove_point",
    "mg_geofence_get_points",
    "mg_geofence_validate",
    "mg_geofence_export_json",
    "mg_geofence_import_json",
    "mg_geofence_export_kml",
    "mg_fence_clear",
    "mg_rally_clear",
    "mg_connection_connect_tcp",
    "mg_stream_request",
    "mg_send_heartbeat",
    "mg_serial_get_config",
    "mg_serial_set_baud",
    "mg_serial_set_protocol",
    "mg_rc_calibrate_start",
    "mg_rc_calibrate_status",
    "mg_rc_calibrate_save",
    # 2026-10 未绑定导出清理：无前端绑定、无调用方、无未来用途
    "mg_actuator_get_limits",
    "mg_actuator_set_function",
    "mg_actuator_set_limits",
    "mg_rc_set_mapping",
    "mg_rc_set_failsafe",
    "mg_config_validate_param",
    "mg_config_apply_preset",
    "mg_config_get_presets",
    "mg_discover_devices",
    "mg_calibration_start_airspeed",
    "mg_event_log_get_status",
    "mg_session_log_start",
    "mg_session_log_status",
    "mg_session_log_stop",
    "mg_mission_set_current",
    "mg_telemetry_append",
    "mg_telemetry_clear",
    "mg_telemetry_get_series",
    "mg_telemetry_get_series_names",
    "mg_telemetry_get_stats",
    "mg_telemetry_register_series",
    # 2026-10 未绑定导出清理（第二批）：无前端绑定、无内部调用方、无未来用途
    "mg_actuator_get_mapping", "mg_actuator_motor_test",
    "mg_calibration_get_history", "mg_calibration_get_retry_info",
    "mg_camera_set_focus", "mg_command_gimbal_configure",
    "mg_command_guided_velocity", "mg_command_landing_gear",
    "mg_command_stop_roi", "mg_command_terminate_flight",
    "mg_config_get_group_descriptor", "mg_config_get_groups",
    "mg_config_get_metadata_coverage", "mg_config_read_group",
    "mg_config_write_params", "mg_rc_get_calibration", "mg_rc_get_failsafe",
    "mg_rc_get_mapping", "mg_serial_get_configs",
    "mg_connection_check_heartbeat", "mg_connection_connect",
    "mg_connection_connect_udp", "mg_connection_disconnect",
    "mg_connection_get_heartbeat_age", "mg_connection_get_state",
    "mg_vehicle_close", "mg_vehicle_count", "mg_get_version",
    "mg_operations_get_snapshot_for_vehicle", "mg_set_event_callback",
    "mg_firmware_flash", "mg_firmware_flash_status",
    "mg_battery_failsafe_configure", "mg_failsafe_get_status",
    "mg_flight_get_available_modes", "mg_flight_get_loiter_radius",
    "mg_flight_get_mode_history", "mg_flight_get_mode_mapping",
    "mg_flight_get_mode_status", "mg_flight_get_takeoff_altitude",
    "mg_flight_set_loiter_radius", "mg_flight_set_mode",
    "mg_flight_set_takeoff_altitude", "mg_mission_get_current",
    "mg_mission_get_plan", "mg_mission_get_progress", "mg_mission_upload",
    "mg_fence_get_plan", "mg_rally_get_plan",
    "mg_params_commit_change_set", "mg_params_get_refresh_status",
    "mg_params_prepare_change_set", "mg_params_refresh", "mg_params_set",
    "mg_preflight_check", "mg_sensor_decode_device_id",
    "mg_sensor_decode_type_param", "mg_sensors_get_statustext",
    "mg_sensors_identify_all",
    "mg_event_log_get_events", "mg_event_log_start", "mg_event_log_stop",
    "mg_vehicle_get_adsb_traffic", "mg_vehicle_get_battery",
    "mg_vehicle_get_distance_sensors", "mg_vehicle_get_esc_status",
    "mg_vehicle_get_flight_time", "mg_vehicle_get_gps",
    "mg_vehicle_get_highres_imu", "mg_vehicle_get_imu",
    "mg_vehicle_get_prearm_check", "mg_vehicle_get_pressure",
    "mg_vehicle_get_rc_channels", "mg_vehicle_get_servo_output",
    "mg_vehicle_get_vibration", "mg_vehicle_get_wind_estimate",
    "mg_waveform_clear", "mg_param_file_export", "mg_param_file_export_csv",
    "mg_param_file_import", "mg_param_file_import_csv",
    "mg_sitl_start", "mg_sitl_status", "mg_sitl_stop",
    "mg_health_get_all", "mg_rc_get_channel", "mg_servo_get_output",
}


def without_comments(source):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)


def public_api():
    entries = {}
    for header in sorted((ROOT / "bridge/ffi").glob("ffi_api*.h")):
        source = without_comments(header.read_text(encoding="utf-8-sig"))
        for declaration in re.findall(r"^MGCORE_API\s+[^;]+;", source, re.M):
            name = re.search(r"\b(mg_\w+)\s*\(", declaration)
            if name:
                symbol = name.group(1)
                if symbol in entries:
                    raise ValueError(f"duplicate declaration: {symbol}")
                signature = " ".join(declaration.split()).removeprefix("MGCORE_API ")
                entries[symbol] = (header.name, signature)
    return entries


def test_referenced_symbols():
    """Symbols named anywhere under the test roots (coarse "has a caller" proxy)."""
    names = set()
    for rel in TEST_ROOTS:
        base = ROOT / rel
        if not base.exists():
            continue
        for file in base.rglob("*"):
            if not file.is_file():
                continue
            try:
                text = file.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            names |= set(re.findall(r"\b(mg_\w+)\b", text))
    return names


def unbound_debt(api, bindings, tested):
    """Public exports with no frontend binding, classification or test caller.

    这些符号是 A7 意义上的存量债务：既没进 UI，也没有明确分类，还没有任何测试引用。
    基线只允许缩小，不能让新导出悄悄加入。
    """
    debt = set(api) - set(bindings) - set(classified_symbols()) - set(tested)
    return sorted(debt)


def sync_unbound_baseline(debt, write):
    payload = {
        "_comment": (
            "公开导出中「无前端绑定、无分类、无测试引用」的存量清单。"
            "基线只允许缩小：新增未分类导出会失败；已覆盖或分类的符号必须从基线删除。"
        ),
        "symbols": debt,
    }
    if write:
        text = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
        tmp = UNBOUND_BASELINE.with_suffix(".json.tmp")
        tmp.write_text(text, encoding="utf-8")
        tmp.replace(UNBOUND_BASELINE)
        return
    if not UNBOUND_BASELINE.exists():
        raise ValueError("tools/ffi_unbound_baseline.json is missing; run with --write-baseline")
    current = json.loads(UNBOUND_BASELINE.read_text(encoding="utf-8")).get("symbols", [])
    added = sorted(set(debt) - set(current))
    removed = sorted(set(current) - set(debt))
    if added or removed:
        lines = []
        if added:
            lines.append(
                "new public exports with no UI binding, classification or test caller "
                "(bind, test or classify them, then rerun --write-baseline): " + ", ".join(added))
        if removed:
            lines.append(
                "baseline symbols are now bound, classified or test-referenced; drop them from the baseline "
                "(rerun --write-baseline): " + ", ".join(removed))
        raise ValueError("; ".join(lines))


def frontend_bindings():
    bindings = {}
    for file in sorted((ROOT / "flutter_shell/lib").rglob("*.dart")):
        source = without_comments(file.read_text(encoding="utf-8"))
        for symbol in re.findall(r"['\"](mg_\w+)['\"]", source):
            bindings.setdefault(symbol, set()).add(str(file.relative_to(ROOT)))
    return bindings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write-baseline",
        action="store_true",
        help="freeze the current unbound, unclassified, untested export debt",
    )
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    api = public_api()
    bindings = frontend_bindings()
    retired_declarations = RETIRED_SYMBOLS & set(api)
    if retired_declarations:
        raise ValueError("retired FFI declarations remain: " + ", ".join(sorted(retired_declarations)))
    missing = set(bindings) - set(api)
    if missing:
        raise ValueError("undeclared Dart bindings: " + ", ".join(sorted(missing)))
    retired_bindings = RETIRED_SYMBOLS & set(bindings)
    if retired_bindings:
        raise ValueError("Dart still binds retired FFI symbols: " + ", ".join(sorted(retired_bindings)))
    note_sets = [
        set(INTERNAL_SYMBOL_NOTES), set(SUPERSEDED_SYMBOL_NOTES),
        set(DEFERRED_SYMBOL_NOTES), set(RETIREMENT_CANDIDATE_NOTES),
    ]
    dup_notes = sorted(set().union(*(
        note_sets[i] & note_sets[j]
        for i in range(len(note_sets)) for j in range(i + 1, len(note_sets))
    )))
    if dup_notes:
        raise ValueError("symbols classified more than once: " + ", ".join(dup_notes))
    notes = classified_symbols()
    now_bound = sorted(set(notes) & set(bindings))
    if now_bound:
        raise ValueError(
            "classified symbols are now bound in Dart; review their classification notes: "
            + ", ".join(now_bound))
    unknown_notes = sorted(set(notes) - set(api))
    if unknown_notes:
        raise ValueError(
            "classified symbol notes reference unknown exports: " + ", ".join(unknown_notes))
    debt = unbound_debt(api, bindings, test_referenced_symbols())
    sync_unbound_baseline(debt, args.write_baseline)
    if args.library:
        library = ctypes.CDLL(str(args.library.resolve()))
        absent = []
        for symbol in sorted(api):
            try:
                getattr(library, symbol)
            except AttributeError:
                absent.append(symbol)
        if absent:
            raise ValueError("library missing exports: " + ", ".join(absent))
        still_exported = []
        for symbol in sorted(RETIRED_SYMBOLS):
            try:
                getattr(library, symbol)
            except AttributeError:
                continue
            still_exported.append(symbol)
        if still_exported:
            raise ValueError("library still exports retired FFI symbols: " + ", ".join(still_exported))
        # 库的导出集合必须与公开头完全一致：上面已逐个 dlsym 过，任何缺失都会
        # 在前面报出，因此旧的版本号探针（依赖已删除的 mg_get_version）不再需要。
        version_header = (ROOT / "bridge/ffi/ffi_api_core.h").read_text(encoding="utf-8")
        version = ".".join(
            re.search(rf"^#define MGCORE_VERSION_{part}\s+(\d+)$", version_header, re.M).group(1)
            for part in ("MAJOR", "MINOR", "PATCH")
        )
        print(f"[ffi-contract] library exports match header v{version}")
    print(f"[ffi-contract] unbound/untested export debt: {len(debt)} (baseline tracked)")
    print(f"[ffi-contract] {len(api)} declarations and {len(bindings)} Dart bindings agree"
          + (" with the compiled library" if args.library else ""))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"[ffi-contract] {error}", file=sys.stderr)
        sys.exit(1)
