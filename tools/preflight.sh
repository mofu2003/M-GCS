#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
run_backend=1
run_frontend=1
run_macos_build=1
changed_mode=0
changed_ref="HEAD"
# 后端范围：full（37 套）| targets（只跑列出的套件）| none
backend_scope="full"
backend_targets=""
plan_reason=""
# 参数描述符改动即使不重跑整套 Flutter，也要重跑 S6 字面量守卫。
want_firmware_literals=0

usage() {
  cat <<'EOF'
用法：tools/preflight.sh [选项]

默认执行：
  1. git diff --check HEAD
  2. UI 设计 token 扫描
  3. 前端固件参数字面量守卫（S6）
  4. Flutter analyze + test
  5. 编译 build_check 并运行后端口禁
  6. macOS 上执行 flutter build macos --debug

选项：
  --frontend-only        只跑 token 扫描、Flutter analyze/test 和 macOS 构建
  --backend-only         只编译并运行后端口禁
  --no-macos-build       跳过 macOS debug 构建
  --changed              按改动挑最小范围（默认对比 HEAD，含未提交与未跟踪文件）
  --changed=<ref>        同上，但对比指定 ref（如 --changed=HEAD~3、--changed=main）
  -h, --help             显示本帮助

--changed 的规则：
  * 改到共享层（core/ 的 mavlink、bridge/ffi、共享测试夹具、CMake、未分类路径）
    一律**升级为后端全量**并打印原因——37 套后端测试都驱动同一个 adapter，
    定向跑会漏掉跨模块回归。
  * 改到某个后端测试文件只跑那一套；改到校准/任务模块只跑相关几套。
  * 只改 docs 时不编译后端、不跑 Flutter。
  * 提交前仍建议跑一次不带 --changed 的全量。
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --frontend-only)
      run_backend=0
      ;;
    --backend-only)
      run_frontend=0
      run_macos_build=0
      ;;
    --no-macos-build)
      run_macos_build=0
      ;;
    --changed)
      changed_mode=1
      ;;
    --changed=*)
      changed_mode=1
      changed_ref="${1#--changed=}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[preflight] 未知选项：$1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ "$run_backend" -eq 0 && "$run_frontend" -eq 0 ]]; then
  echo "[preflight] --frontend-only 与 --backend-only 不能同时使用。" >&2
  exit 2
fi

cd "$root_dir"

if [[ "$changed_mode" -eq 1 ]]; then
  if ! git rev-parse --verify --quiet "${changed_ref}^{commit}" >/dev/null; then
    echo "[preflight] --changed 的对比基准不存在：$changed_ref" >&2
    exit 2
  fi

  changed_list="$(
    {
      git diff --name-only "$changed_ref" --
      git ls-files --others --exclude-standard
    } | sort -u
  )"

  if [[ -z "$changed_list" ]]; then
    echo "[preflight] --changed：相对 $changed_ref 没有任何改动，无需检查。"
    exit 0
  fi

  backend_full=0
  backend_full_reason=""
  want_frontend=0
  want_macos=0

  add_target() {
    case " $backend_targets " in
      *" $1 "*) ;;
      *) backend_targets="$backend_targets $1" ;;
    esac
  }

  while IFS= read -r file; do
    [[ -z "$file" ]] && continue
    case "$file" in
      # 纯文档：只跑轻量检查
      docs/*|*.md|LICENSE|.gitignore)
        ;;
      # 前端
      flutter_shell/pubspec.yaml|flutter_shell/pubspec.lock|flutter_shell/macos/*)
        want_frontend=1
        want_macos=1
        ;;
      flutter_shell/*)
        want_frontend=1
        ;;
      tools/check_ui_tokens.dart)
        want_frontend=1
        ;;
      tools/check_firmware_literals.py|tools/firmware_literals_baseline.json)
        want_frontend=1
        ;;
      # 参数描述符是 S6 字面量守卫的事实来源，改了要重跑守卫。
      core/src/config/*.json)
        backend_full=1
        backend_full_reason="参数描述符 $file（S6 字面量守卫的事实来源）"
        want_firmware_literals=1
        ;;
      # 共享测试夹具：所有 sim 测试都用
      core/tests/mock_flight_controller.h|core/tests/*.h)
        backend_full=1
        backend_full_reason="共享测试夹具 $file"
        ;;
      # 单个后端测试文件：只跑它自己
      core/tests/*.cc)
        add_target "$(basename "$file" .cc)"
        ;;
      core/tests/*)
        backend_full=1
        backend_full_reason="未分类测试文件 $file"
        ;;
      # 共享层：37 套测试全部驱动同一个 adapter，定向跑会漏
      core/src/mavlink/*|core/include/core/mavlink/*)
        backend_full=1
        backend_full_reason="共享层 ${file}（后端测试全部经过同一个 MAVLink adapter）"
        ;;
      bridge/ffi/*|bridge/*)
        backend_full=1
        backend_full_reason="FFI 契约层 $file"
        ;;
      # 模块级：只跑相关套件
      core/src/calibration/*|core/include/core/calibration/*)
        add_target test_sim_calibration
        add_target test_state_machines
        ;;
      core/src/mission/*|core/include/core/mission/*)
        add_target test_mission_document
        add_target test_sim_mission
        add_target test_sim_mission_document
        add_target test_sim_fence_rally
        ;;
      third_party/*)
        echo "[preflight] 禁区：不允许修改 third_party/（见 AGENTS.md）：$file" >&2
        exit 2
        ;;
      # 其余（CMake、tools、core 其他模块、未分类）保守跑全量
      *)
        backend_full=1
        backend_full_reason="未分类路径 ${file}（保守起见跑全量）"
        ;;
    esac
  done <<< "$changed_list"

  file_count="$(printf '%s\n' "$changed_list" | grep -c . || true)"
  echo "[preflight] --changed：相对 $changed_ref 共 $file_count 个文件改动"

  if [[ "$run_frontend" -eq 0 ]]; then
    want_frontend=0
  fi
  if [[ "$want_frontend" -eq 0 ]]; then
    run_frontend=0
  fi
  if [[ "$want_macos" -eq 0 ]]; then
    run_macos_build=0
  fi

  if [[ "$run_backend" -eq 0 ]]; then
    backend_scope="none"
  elif [[ "$backend_full" -eq 1 ]]; then
    backend_scope="full"
    plan_reason="$backend_full_reason"
    echo "[preflight] 后端升级为全量：$backend_full_reason"
  elif [[ -n "$backend_targets" ]]; then
    backend_scope="targets"
    echo "[preflight] 后端定向：$(echo "$backend_targets" | xargs)"
  else
    backend_scope="none"
    echo "[preflight] 本次改动不涉及后端，跳过后端门禁"
  fi

  if [[ "$run_frontend" -eq 0 ]]; then
    echo "[preflight] 本次改动不涉及 flutter_shell，跳过 Flutter 检查"
  fi
fi

echo "[preflight] 检查补丁格式"
git diff --check HEAD
git diff --cached --check

echo "[preflight] 检查版本号一致性"
cmake_version="$(sed -n 's/^project(mgcore VERSION \([0-9][0-9.]*\) LANGUAGES CXX.*/\1/p' CMakeLists.txt)"
pubspec_version="$(sed -n 's/^version: \([0-9][0-9.]*\)+[0-9][0-9]*$/\1/p' flutter_shell/pubspec.yaml)"
ffi_version="$(sed -n 's/^#define MGCORE_VERSION_MAJOR \([0-9]*\)$/\1/p' bridge/ffi/ffi_api_core.h).$(sed -n 's/^#define MGCORE_VERSION_MINOR \([0-9]*\)$/\1/p' bridge/ffi/ffi_api_core.h).$(sed -n 's/^#define MGCORE_VERSION_PATCH \([0-9]*\)$/\1/p' bridge/ffi/ffi_api_core.h)"
if [[ -z "$cmake_version" || "$cmake_version" != "$pubspec_version" || "$cmake_version" != "$ffi_version" ]]; then
  echo "[preflight] 版本号不一致：CMake=$cmake_version pubspec=$pubspec_version ffi=$ffi_version" >&2
  exit 1
fi
echo "[preflight] 版本一致：$cmake_version"

changelog_version="$(sed -n 's/^## \[\([0-9][0-9.]*\)\] - .*/\1/p' CHANGELOG.md | head -n 1)"
if [[ "$changelog_version" != "$cmake_version" ]]; then
  echo "[preflight] 最新发布条目与版本不一致：CHANGELOG=$changelog_version CMake=$cmake_version" >&2
  exit 1
fi

# 判定一个动态库是否依赖 sanitizer 运行时。
# CMakeLists 把 mgcore 的输出目录固定到仓库根 build/，所有构建树共用同一个文件；
# 如果之前有构建树以 sanitizer 构建过共享目标，这里可能拿到带 TSAN/ASAN 的库。
ffi_library_is_sanitized() {
  local lib="$1" deps=""
  case "$(uname -s)" in
    Darwin) deps="$(otool -L "$lib" 2>/dev/null || true)" ;;
    *) deps="$( (ldd "$lib" 2>/dev/null || readelf -d "$lib" 2>/dev/null) || true)" ;;
  esac
  grep -Eq 'clang_rt\.(tsan|asan)|libtsan|libasan' <<<"$deps"
}

echo "[preflight] 检查 FFI 声明与前端绑定"
"${PYTHON_BIN:-python3}" tools/check_ffi_contract.py

# 编译并发上限。
# 不带数字的 `cmake --build --parallel` 等于不限并发：16 GB 机器上 10+ 个 clang
# 同时编译 mavlink 重头文件的 TU 会触发换页，实测 165 s，而限成 8 路只要 26 s。
# 按内存给一半，再不超过实测最优点。
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
echo "[preflight] 编译并发：${build_jobs}（可用 MGCORE_BUILD_JOBS 覆盖）"

if [[ "$run_frontend" -eq 1 || "$want_firmware_literals" -eq 1 ]]; then
  echo "[preflight] 检查前端固件参数字面量（S6）"
  "${PYTHON_BIN:-python3}" tools/check_firmware_literals.py
fi

if [[ "$run_frontend" -eq 1 ]]; then
  command -v dart >/dev/null 2>&1 || {
    echo "[preflight] 未找到 dart；请先安装 Flutter SDK。" >&2
    exit 1
  }
  command -v flutter >/dev/null 2>&1 || {
    echo "[preflight] 未找到 flutter。" >&2
    exit 1
  }

  echo "[preflight] 检查 UI 设计 token"
  dart tools/check_ui_tokens.dart

  echo "[preflight] Flutter analyze"
  (
    cd flutter_shell
    flutter analyze lib test
  )

  echo "[preflight] Flutter test"
  (
    cd flutter_shell
    flutter test
  )
fi

if [[ "$run_backend" -eq 1 && "$backend_scope" != "none" ]]; then
  if [[ ! -d build_check ]]; then
    cat >&2 <<'EOF'
[preflight] 缺少 build_check。
请先执行：
  ./tools/generate-mavlink.sh
  cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build_check --config Release
EOF
    exit 1
  fi

  if ! grep -Eq '^MGCORE_BUILD_TESTS(:BOOL)?=ON$' build_check/CMakeCache.txt; then
    echo "[preflight] build_check 未启用 MGCORE_BUILD_TESTS=ON，请删除该目录后用 -DMGCORE_BUILD_TESTS=ON 重新配置。" >&2
    exit 1
  fi

  command -v cmake >/dev/null 2>&1 || {
    echo "[preflight] 未找到 cmake。" >&2
    exit 1
  }
  command -v ctest >/dev/null 2>&1 || {
    echo "[preflight] 未找到 ctest。" >&2
    exit 1
  }

  if [[ "$backend_scope" == "targets" ]]; then
    # 只保留 ctest 真正注册过的套件，避免拼错目标名导致构建失败。
    known_suites=" $(ctest --test-dir build_check -N -L "offline|simulation" -LE "hardware|manual" 2>/dev/null |
      sed -n 's/^ *Test *#[0-9]*: *//p' | tr '\n' ' ') "
    resolved=""
    for candidate in $backend_targets; do
      case "$known_suites" in
        *" $candidate "*) resolved="$resolved $candidate" ;;
        *) echo "[preflight] 未注册的测试套件 ${candidate}，升级为后端全量" >&2; backend_scope="full" ;;
      esac
    done
    if [[ "$backend_scope" == "targets" ]]; then
      if [[ -z "${resolved// }" ]]; then
        echo "[preflight] 定向集合为空，跳过后端门禁"
        backend_scope="none"
      else
        backend_targets="$resolved"
      fi
    fi
  fi

  if [[ "$backend_scope" == "none" ]]; then
    echo "[preflight] 跳过后端门禁"
  elif [[ "$backend_scope" == "targets" ]]; then
    echo "[preflight] 编译后端测试（定向）"
    cmake --build build_check --config Release --parallel "$build_jobs" --target $backend_targets

    filter="^($(echo "$backend_targets" | xargs | tr ' ' '|'))$"
    echo "[preflight] 后端 offline/simulation 门禁（定向：$(echo "$backend_targets" | xargs)）"
    ctest --test-dir build_check --output-on-failure \
      --no-tests=error \
      -R "$filter" \
      -L "offline|simulation" \
      -LE "hardware|manual"
  else
    echo "[preflight] 编译后端测试"
    cmake --build build_check --config Release --parallel "$build_jobs"

    echo "[preflight] 后端 offline/simulation 门禁"
    ctest --test-dir build_check --output-on-failure \
      --no-tests=error \
      -L "offline|simulation" \
      -LE "hardware|manual"
  fi
fi

if [[ "$run_macos_build" -eq 1 ]]; then
  if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "[preflight] Flutter macOS debug 构建"
    (
      cd flutter_shell
      flutter build macos --debug
    )
  else
    echo "[preflight] 非 macOS 环境，跳过 Flutter macOS debug 构建。"
  fi
fi

if [[ "$run_backend" -eq 1 && "$backend_scope" == "full" ]]; then
  case "$(uname -s)" in
    Darwin) ffi_library="build/libmgcore.dylib" ;;
    Linux) ffi_library="build/libmgcore.so" ;;
    *) ffi_library="build/mgcore.dll" ;;
  esac
  # build/ 是所有构建树共用的共享库输出目录。若别处以 sanitizer 构建过，make 的
  # “已最新”判断会跳过 Release 重链接，load 带 sanitizer 运行时的库会直接 abort。
  # 发现后删除该产物并用 build_check 重新链接 Release 版本。
  if [[ -f "$ffi_library" ]] && ffi_library_is_sanitized "$ffi_library"; then
    echo "[preflight] $ffi_library 带 sanitizer 运行时，重新以 Release 链接 mgcore"
    rm -f "$ffi_library"
    cmake --build build_check --config Release --target mgcore --parallel "$build_jobs"
  fi
  "${PYTHON_BIN:-python3}" tools/check_ffi_contract.py --library "$ffi_library"
  if [[ "$run_frontend" -eq 1 ]]; then
    echo "[preflight] Flutter 与刚编译后端的 FFI 联调"
    (
      cd flutter_shell
      MGCS_FLEET_LIBRARY="$root_dir/$ffi_library" \
        flutter test --no-pub test/fleet_ffi_smoke_test.dart
    )
  fi
fi

echo "[preflight] 全部检查通过。"
