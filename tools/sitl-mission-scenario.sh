#!/usr/bin/env bash
#
# 运行端到端 SITL 任务场景并落盘一份可复算的证据记录。
#
# 用法：
#   tools/sitl-mission-scenario.sh [选项]
#
# 选项：
#   --ardupilot <dir>    ArduPilot 源码/构建目录（默认 $ARDUPILOT_DIR 或
#                        /Users/fansicheng/Projects/ardupilot）
#   --vehicle <name>     SITL 机型二进制（默认 arducopter）
#   --port <n>           MAVLink UDP 端口（默认 14550）
#   --model <name>       仿真机架（默认 quad）
#   --evidence <path>    证据输出文件（默认 build_check/sitl-scenario-evidence.md；
#                        证据是运行产物，默认不落进仓库。要发布时显式指到 docs/ 下。）
#   --keep              跑完不杀 SITL（调试用）
#
# 退出码 = 场景驱动的退出码（0 表示全部断言通过）。
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root_dir"

ardupilot_dir="${ARDUPILOT_DIR:-/Users/fansicheng/Projects/ardupilot}"
vehicle="arducopter"
port=14550
model="quad"
evidence="$root_dir/build_check/sitl-scenario-evidence.md"
keep=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ardupilot) ardupilot_dir="$2"; shift 2 ;;
        --vehicle)   vehicle="$2"; shift 2 ;;
        --port)      port="$2"; shift 2 ;;
        --model)     model="$2"; shift 2 ;;
        --evidence)  evidence="$2"; shift 2 ;;
        --keep)      keep=1; shift ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

sitl_bin="$ardupilot_dir/build/sitl/bin/$vehicle"
if [[ ! -x "$sitl_bin" ]]; then
    echo "SITL binary not found: $sitl_bin" >&2
    echo "Set --ardupilot <dir> or ARDUPILOT_DIR, and build SITL first." >&2
    exit 2
fi

driver="$root_dir/build_check/test_sitl_mission_scenario"
if [[ ! -x "$driver" ]]; then
    echo "scenario driver not built: $driver" >&2
    echo "Configure with -DMGCORE_BUILD_TESTS=ON and build target test_sitl_mission_scenario." >&2
    exit 2
fi

sitl_log="$(mktemp -t mgcs-sitl-XXXXXX.log)"
serial_dev="udpclient:127.0.0.1:$port"
echo "[sitl] launching $sitl_bin --model $model -I0 --serial0 $serial_dev"
"$sitl_bin" --model "$model" -I0 --serial0 "$serial_dev" >"$sitl_log" 2>&1 &
sitl_pid=$!

cleanup() {
    if [[ "$keep" -eq 0 ]] && kill -0 "$sitl_pid" 2>/dev/null; then
        kill "$sitl_pid" 2>/dev/null || true
        wait "$sitl_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "[sitl] waiting for first heartbeat..."
ready=0
for _ in $(seq 1 40); do
    if grep -q "UDP connection \|Init Gyro\|EKF2 IMU0 is using GPS\|ArduPilot Ready" "$sitl_log" 2>/dev/null; then
        ready=1
        break
    fi
    if ! kill -0 "$sitl_pid" 2>/dev/null; then
        echo "[sitl] process exited early; log:" >&2
        tail -40 "$sitl_log" >&2
        exit 3
    fi
    sleep 0.5
done
if [[ "$ready" -eq 0 ]]; then
    echo "[sitl] warning: readiness marker not seen, continuing anyway" >&2
fi
sleep 3

echo "[scenario] running driver against udp://127.0.0.1:$port"
set +e
MG_SITL_ENDPOINT="127.0.0.1:$port" "$driver" 2>&1 | tee "$evidence.stdout"
driver_rc=${PIPESTATUS[0]}
set -e

{
    echo "# SITL 任务场景证据"
    echo
    echo "- 时间：$(date '+%Y-%m-%d %H:%M:%S %Z')"
    echo "- 目标：$sitl_bin --model $model -I0 --serial0 $serial_dev"
    echo "- 驱动：build_check/test_sitl_mission_scenario"
    echo "- 退出码：$driver_rc"
    echo "- 复算：tools/sitl-mission-scenario.sh --vehicle $vehicle --model $model --port $port --evidence ${evidence#$root_dir/}"
    echo
    echo '```text'
    # 只保留断言与证据行，去掉心跳/日志噪声；需要完整日志时直接跑驱动。
    grep -E '\[PASS\]|\[FAIL\]|\[EVIDENCE\]|\[SKIP\]|=== SITL scenario:' "$evidence.stdout" || true
    echo '```'
} > "$evidence"
python3 -c "import os,sys; os.remove(sys.argv[1])" "$evidence.stdout"

echo "[scenario] evidence written to $evidence (exit=$driver_rc)"
exit "$driver_rc"
