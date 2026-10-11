#!/usr/bin/env python3
"""Check the public API, Dart bindings, catalog and optional compiled library."""
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
CATALOG = ROOT / "bridge/ffi/API_CATALOG.md"
CAPABILITIES = ROOT / "docs/capabilities.md"
CAPABILITY_START = "<!-- capabilities:start -->"
CAPABILITY_END = "<!-- capabilities:end -->"

# 每个公开头文件（= 责任模块）的交付状态与说明。计数由脚本从源码与绑定推导，
# 状态与说明在此处人工维护；两者一旦不一致，preflight 会让本文件失效并要求重跑
# --write-catalog。取值：已交付 / 部分接入 / 待接入 / 内部能力。
CAPABILITY_METADATA = {
    "ffi_api_configuration.h": ("部分接入", "连接与校准子集已进入配置页；参数组描述符（`mg_config_get_param_roles` 语义角色表）经 Dart 绑定进入 PID、校准、飞行模式与解锁检查等配置页，由前端按语义角色取固件参数名（safety.md S6）；电调/舵机执行器仍为 FFI 内部能力；7 个 PX4 便捷封装（执行器/模式槽/安全/调参）定位为内部能力、无独立 UI 计划——通用参数页配合 `px4` 描述符 profile 已覆盖同一批参数，见 `bridge/ffi/API_CATALOG.md` 的「已定性的内部公开导出」一节与 docs/features/px4-parity.md S5。"),
    "ffi_api_connection.h": ("已交付", "串口/UDP/TCP 连接、断开、链路状态与本地回环由连接面板和状态栏消费。"),
    "ffi_api_core.h": ("已交付", "生命周期、版本、错误信息与整机快照由前端启动路径消费。"),
    "ffi_api_environment.h": ("部分接入", "SITL 启停（`mg_sitl_*`）定位为内部能力、无前端 UI 计划（见 bridge/ffi/API_CATALOG.md 的「已定性的内部公开导出」）；地形缓存与剖面查询（`mg_terrain_query*`、`mg_terrain_cache_*`）已接前端 backend 与在线 provider（open-elevation），但尚未接入页面，前端入口待定，见 docs/features/survey-and-terrain.md。"),
    "ffi_api_firmware.h": ("部分接入", "固件识别接在连接流程；刷写与校验保持 FFI 内部能力，未接 UI。"),
    "ffi_api_flight.h": ("部分接入", "生产入口是 submit/poll 与 get_modes/get_mode_mapping；同步 arm/set_mode/rtl 等仅作内部与测试用途。"),
    "ffi_api_geofence.h": ("部分接入", "围栏读写进入任务面板；Rally 点集合仍为 FFI 内部能力。"),
    "ffi_api_logging.h": ("部分接入", "机载 DataFlash 日志列表/下载/擦除（「机载日志」页）、链路 `.tlog` 录制/离线回放（「飞行记录」页）与 `mg_fs_*` 通用 MAVLink FTP 文件浏览/收发（「机载文件」页）均已接入；仍未接入的是事件日志服务与会话文件日志（`mg_event_*`/`mg_session_log_*`），见 docs/features/flight-logging-and-replay.md。"),
    "ffi_api_mission.h": ("已交付", "任务规划、上传、围栏/Rally 下发与航点编辑走任务面板与地图；survey/corridor 生成（`mg_survey_generate*`）接入配置工作区「测绘规划」页，见 docs/features/survey-and-terrain.md。"),
    "ffi_api_params.h": ("部分接入", "参数读写、文件导入导出接入配置页；批量下载进度与描述符映射部分留在 FFI 内部。"),
    "ffi_api_payload.h": ("部分接入", "相机/云台/视频元数据接入配置工作区「相机与云台」页（`payload`，S2/S3/S4）：设备清单、拍照/录像/变焦/云台指向/回中与视频流元数据。本地视频解码与录制/截图（S3/S4，ADR-0008）已接入：`mg_video_renderer_status`/`mg_video_poll_frame` 渲染最新 RGBA 帧，`mg_video_start_recording`/`mg_video_stop_recording`/`mg_video_recording_status`/`mg_video_capture_frame` 把正在播放的码流复制到本地文件（Matroska 容器）并按容量上限清理最旧文件；没有编译进后端时 `available:false`/`backend:\"none\"` 并如实说明。S1 的读契约返回真实适配器缓存，无设备时 `success:true`+`available:false`，见 docs/features/video-camera-gimbal.md。"),
    "ffi_api_telemetry.h": ("部分接入", "遥测快照、姿态、GPS、电池进入顶栏/仪表盘；波形与部分导航量仍为 FFI 内部能力。"),
}

# 公开导出但没有前端绑定、且定位已明确为“内部能力”的符号（不计划接 UI）。
# 这些符号必须在 API_CATALOG.md 的「已定性的内部公开导出」一节里给出定位。
# 一旦被 Flutter 绑定（说明定位变了），或引用了不存在的导出，preflight 会失败，
# 要求同步更新定位，避免出现“公开导出但身份不明”的历史堆积。
INTERNAL_SYMBOL_NOTES = {
    "mg_px4_actuator_get_config": "PX4 执行器映射；内部能力（通用参数页覆盖 PWM_MAIN_FUNC1..16）",
    "mg_px4_actuator_set_config": "PX4 执行器映射；内部能力（通用参数页覆盖 PWM_MAIN_FUNC1..16）",
    "mg_px4_flight_mode_get_config": "PX4 模式槽只读视图（COM_FLTMODE1..6）；写入走通用参数页，避免与 RC 模式通道脱节",
    "mg_px4_safety_get": "PX4 安全断路器；内部能力（按飞控实际 CBRK_* 缓存报告，不伪造固定字段）",
    "mg_px4_safety_set": "PX4 安全断路器；内部能力（只写飞控实际存在的 CBRK_*）",
    "mg_px4_tuning_get": "PX4 姿态/角速度 PID；内部能力（通用参数页覆盖 MC_* 参数）",
    "mg_px4_tuning_set": "PX4 姿态/角速度 PID；内部能力（通用参数页覆盖 MC_* 参数）",
    "mg_sitl_start": "SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口",
    "mg_sitl_status": "SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口",
    "mg_sitl_stop": "SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口",
}


def capability_table(api, bindings):
    """Build the generated coverage table for docs/capabilities.md."""
    headers = sorted({owner for owner, _ in api.values()})
    rows = []
    for header in headers:
        symbols = [s for s, (owner, _) in api.items() if owner == header]
        wired = sum(1 for s in symbols if s in bindings)
        status, note = CAPABILITY_METADATA.get(header, ("待核对", "尚未登记状态。"))
        rows.append((header, len(symbols), wired, status, note))
    total = sum(r[1] for r in rows)
    total_wired = sum(r[2] for r in rows)
    lines = [
        "| 模块 | 公开导出 | 已接入前端 | 状态 | 说明 |",
        "| --- | --- | --- | --- | --- |",
    ]
    for header, n, w, status, note in rows:
        lines.append(f"| `{header}` | {n} | {w} | {status} | {note} |")
    lines.append(f"| **合计** | **{total}** | **{total_wired}** | — | 其余 {total - total_wired} 个公开导出尚未被前端绑定。 |")
    return lines


def sync_capability_doc(api, bindings, write):
    if not CAPABILITIES.exists():
        raise ValueError("docs/capabilities.md is missing; create it with the capability markers")
    text = CAPABILITIES.read_text()
    if CAPABILITY_START not in text or CAPABILITY_END not in text:
        raise ValueError("docs/capabilities.md is missing the capability markers")
    block = "\n".join([CAPABILITY_START, *capability_table(api, bindings), CAPABILITY_END])
    updated = re.sub(
        re.escape(CAPABILITY_START) + r".*?" + re.escape(CAPABILITY_END),
        lambda _: block,
        text,
        flags=re.S,
    )
    if write:
        CAPABILITIES.write_text(updated)
    elif updated != text:
        raise ValueError("docs/capabilities.md coverage table is stale; run --write-catalog")

RETIRED_SYMBOLS = {
    "mg_cal_start", "mg_cal_cancel", "mg_cal_status", "mg_cal_history",
    "mg_cal_retry_info", "mg_cmd_arm", "mg_cmd_disarm", "mg_cmd_set_mode",
    "mg_cmd_rtl", "mg_cmd_takeoff", "mg_cmd_land", "mg_cmd_loiter",
    "mg_cmd_guided", "mg_command_emergency_stop", "mg_param_list",
    "mg_param_get", "mg_param_set", "mg_logging_get_entries",
    "mg_logging_get_stats", "mg_log_list", "mg_mission_request",
    "mg_firmware_request_version", "mg_firmware_get_version",
    "mg_geofence_get", "mg_geofence_set",
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
                text = file.read_text(errors="ignore")
            except OSError:
                continue
            names |= set(re.findall(r"\b(mg_\w+)\b", text))
    return names


def unbound_debt(api, bindings, tested):
    """Public exports with no frontend binding, no internal-only note and no test caller.

    这些符号是 A7 意义上的存量债务：既没进 UI，也没定性为内部能力，还没有任何
    测试引用。基线只允许缩小，不能让新导出悄悄加入。
    """
    debt = set(api) - set(bindings) - set(INTERNAL_SYMBOL_NOTES) - set(tested)
    return sorted(debt)


def sync_unbound_baseline(debt, write):
    payload = {
        "_comment": (
            "公开导出中「无前端绑定、无内部能力定性、无测试引用」的存量清单。"
            "基线只允许缩小：新增未定性导出会失败；已覆盖的符号必须从基线删除。"
            "语义与跟踪方式见 docs/constraints/known-deviations.md 的 KD-45。"
        ),
        "symbols": debt,
    }
    if write:
        UNBOUND_BASELINE.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n")
        return
    if not UNBOUND_BASELINE.exists():
        raise ValueError("tools/ffi_unbound_baseline.json is missing; run with --write-baseline")
    current = json.loads(UNBOUND_BASELINE.read_text()).get("symbols", [])
    added = sorted(set(debt) - set(current))
    removed = sorted(set(current) - set(debt))
    if added or removed:
        lines = []
        if added:
            lines.append(
                "new public exports with no UI binding, no internal-only note and no test caller "
                "(bind, test or register them, then rerun --write-baseline): " + ", ".join(added))
        if removed:
            lines.append(
                "baseline symbols are now bound or test-referenced; drop them from the baseline "
                "(rerun --write-baseline): " + ", ".join(removed))
        raise ValueError("; ".join(lines))


def frontend_bindings():
    bindings = {}
    for file in sorted((ROOT / "flutter_shell/lib").rglob("*.dart")):
        source = without_comments(file.read_text())
        for symbol in re.findall(r"['\"](mg_\w+)['\"]", source):
            bindings.setdefault(symbol, set()).add(str(file.relative_to(ROOT)))
    return bindings


def catalog_text(api, bindings):
    lines = [
        "# FFI API 目录", "",
        "由 `python3 tools/check_ffi_contract.py --write-catalog` 从公开头文件与 Flutter 绑定生成。",
        "运行 `python3 tools/check_ffi_contract.py --library build/libmgcore.dylib` 可核对编译库。",
        "Linux 使用 `build/libmgcore.so`，Windows 使用 `build/mgcore.dll`。", "",
        "`char*` 结果使用 `mg_free_string` 释放，`mg_error_string` 返回的常量除外。",
        "`success:false` 一律返回 {'success':false,'error':{'code':int,'name':str,'message':str}}。",
        "有 COMMAND_ACK 的命令只有 ACK 接受才回 success；SET_POSITION_TARGET_* 等无 ACK 命令只表示已发出。",
        "飞行指令的生产入口是 submit/poll；所有公开名称必须对应当前规范接口。",
        "任务上传的即时响应表示传输启动，最终结果需读取 operation 状态。",
        "符号检查不等于完整的参数类型或行为验证；真实交互另由 FFI smoke 和合约测试覆盖。", "",
        f"公开函数：{len(api)}；Flutter 绑定：{len(bindings)}。", "",
    ]
    for header in sorted({entry[0] for entry in api.values()}):
        lines += [f"## {header}", "", "| 声明 | Flutter 绑定位置 |", "| --- | --- |"]
        for symbol, (owner, signature) in sorted(api.items()):
            if owner != header:
                continue
            locations = "<br>".join(f"`{p}`" for p in sorted(bindings.get(symbol, []))) or "—"
            lines.append(f"| `{signature}` | {locations} |")
        lines.append("")
    if INTERNAL_SYMBOL_NOTES:
        lines += [
            "## 已定性的内部公开导出", "",
            "这些符号保留在 ABI 中，但定位明确为内部能力，没有前端 UI 计划；",
            "`tools/check_ffi_contract.py` 会在它们被 Flutter 绑定后报错，要求同步定位（见脚本的 `INTERNAL_SYMBOL_NOTES`）。", "",
            "| 符号 | 定位 |", "| --- | --- |",
        ]
        for symbol in sorted(INTERNAL_SYMBOL_NOTES):
            lines.append(f"| `{symbol}` | {INTERNAL_SYMBOL_NOTES[symbol]} |")
        lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-catalog", action="store_true")
    parser.add_argument(
        "--write-baseline",
        action="store_true",
        help="freeze the current unbound/untested export set into tools/ffi_unbound_baseline.json",
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
    now_bound = sorted(set(INTERNAL_SYMBOL_NOTES) & set(bindings))
    if now_bound:
        raise ValueError(
            "symbols marked internal-only are now bound in Dart; update INTERNAL_SYMBOL_NOTES: "
            + ", ".join(now_bound))
    unknown_notes = sorted(set(INTERNAL_SYMBOL_NOTES) - set(api))
    if unknown_notes:
        raise ValueError(
            "INTERNAL_SYMBOL_NOTES references unknown exports: " + ", ".join(unknown_notes))
    expected = catalog_text(api, bindings)
    if args.write_catalog:
        CATALOG.write_text(expected)
    elif not CATALOG.exists() or CATALOG.read_text() != expected:
        raise ValueError("API_CATALOG.md is stale; run with --write-catalog")
    sync_capability_doc(api, bindings, args.write_catalog)
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
        version_header = (ROOT / "bridge/ffi/ffi_api_core.h").read_text()
        expected_version = ".".join(
            re.search(rf"^#define MGCORE_VERSION_{part}\s+(\d+)$", version_header, re.M).group(1)
            for part in ("MAJOR", "MINOR", "PATCH")
        )
        library.mg_get_version.restype = ctypes.c_void_p
        library.mg_get_version.argtypes = []
        library.mg_free_string.argtypes = [ctypes.c_void_p]
        library.mg_free_string.restype = None
        pointer = library.mg_get_version()
        if not pointer:
            raise ValueError("library returned an empty version allocation")
        try:
            actual_version = ctypes.string_at(pointer).decode("utf-8")
        finally:
            library.mg_free_string(pointer)
        if actual_version != expected_version:
            raise ValueError(f"stale library version: {actual_version}, expected {expected_version}")
        print(f"[ffi-contract] library version: {actual_version}")
    print(f"[ffi-contract] unbound/untested export debt: {len(debt)} (baseline tracked)")
    print(f"[ffi-contract] {len(api)} declarations and {len(bindings)} Dart bindings agree"
          + (" with the compiled library" if args.library else ""))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"[ffi-contract] {error}", file=sys.stderr)
        sys.exit(1)
