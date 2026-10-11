#!/usr/bin/env bash
# 在 ASan / TSan 下复测后端 offline|simulation 套件（开发计划阶段 1E）。
#
# 单独成脚本而不是塞进 preflight：sanitizer 构建需要把全部测试重新编译一遍，
# 比 Release 门禁慢得多，只应在触及锁、回调、线程或传输层的改动后运行。
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root_dir"

run_asan=1
run_tsan=1

usage() {
  cat <<'EOF'
用法：tools/sanitize.sh [选项]

默认依次在 build_asan 与 build_tsan 下配置、编译并运行
`ctest -L "offline|simulation" -LE "hardware|manual"`。

选项：
  --asan-only            只跑 AddressSanitizer
  --tsan-only            只跑 ThreadSanitizer
  -h, --help             显示本帮助

可用环境变量：
  MGCORE_BUILD_JOBS         覆盖编译并发（默认按内存与 CPU 折算，上限 8）
  MGCORE_SANITIZE_BUILD_TYPE 覆盖构建类型（默认 Debug）
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --asan-only) run_tsan=0 ;;
    --tsan-only) run_asan=0 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "[sanitize] 未知选项：$1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

build_type="${MGCORE_SANITIZE_BUILD_TYPE:-Debug}"

# 与 preflight 相同的并发上限理由：mavlink 重头文件并发过高会触发换页。
build_jobs="${MGCORE_BUILD_JOBS:-}"
if [[ -z "$build_jobs" ]]; then
  cpu_count="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
  mem_bytes="$(sysctl -n hw.memsize 2>/dev/null || echo 0)"
  mem_gb=$(( mem_bytes / 1073741824 ))
  if [[ "$mem_gb" -le 0 ]]; then
    mem_gb=8
  fi
  mem_jobs=$(( mem_gb / 2 ))
  if [[ "$mem_jobs" -lt 2 ]]; then
    mem_jobs=2
  fi
  build_jobs="$cpu_count"
  if [[ "$mem_jobs" -lt "$build_jobs" ]]; then
    build_jobs="$mem_jobs"
  fi
  if [[ "$build_jobs" -gt 8 ]]; then
    build_jobs=8
  fi
fi

if [[ ! -d third_party/mavlink/include/mavlink ]]; then
  echo "[sanitize] 缺少 MAVLink C 头，先执行 ./tools/generate-mavlink.sh" >&2
  exit 1
fi

run_sanitized() {
  local label="$1" build_dir="$2" option="$3"
  echo "[sanitize] 配置 ${label}（${build_dir}，${build_type}）"
  # 不构建共享库：CMakeLists 把 mgcore 的输出目录固定到 ${MGCORE_ROOT_DIR}/build，
  # 所有构建树共用同一个 build/libmgcore.dylib。sanitizer 树若在 ON 下构建共享目标，
  # 会用带 TSAN/ASAN 运行时的库覆盖 Release 产物，随后 Flutter 内嵌与 preflight 的
  # FFI 符号核对都会加载到错误的库。测试只链接 mgcore_static/mgcore_core_test，
  # 不需要共享目标。
  cmake -S . -B "$build_dir" \
    -DMGCORE_BUILD_TESTS=ON \
    -DMGCORE_BUILD_SHARED=OFF \
    -DCMAKE_BUILD_TYPE="$build_type" \
    "-D${option}=ON"
  echo "[sanitize] 编译 $label"
  cmake --build "$build_dir" --parallel "$build_jobs"
  echo "[sanitize] 运行 ${label}（offline|simulation）"
  # macOS 的 ASan 不支持 detect_leaks（会直接 abort 每个用例），因此不设 ASAN_OPTIONS。
  ctest --test-dir "$build_dir" --output-on-failure --no-tests=error \
    -L "offline|simulation" -LE "hardware|manual"
}

if [[ "$run_asan" -eq 1 ]]; then
  run_sanitized "AddressSanitizer" build_asan MGCORE_ENABLE_ASAN
fi
if [[ "$run_tsan" -eq 1 ]]; then
  run_sanitized "ThreadSanitizer" build_tsan MGCORE_ENABLE_TSAN
fi

echo "[sanitize] 全部检查通过。"
