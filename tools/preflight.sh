#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
run_backend=1
run_frontend=1
run_macos_build=1

usage() {
  cat <<'EOF'
用法：tools/preflight.sh [选项]

默认执行：
  1. git diff --check HEAD
  2. UI 设计 token 扫描
  3. Flutter analyze + test
  4. 编译 build_check 并运行后端口禁
  5. macOS 上执行 flutter build macos --debug

选项：
  --frontend-only   只跑 token 扫描、Flutter analyze/test 和 macOS 构建
  --backend-only    只编译并运行后端口禁
  --no-macos-build  跳过 macOS debug 构建
  -h, --help        显示本帮助
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

if [[ "$run_backend" -eq 1 ]]; then
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

  echo "[preflight] 编译后端测试"
  cmake --build build_check --config Release --parallel

  echo "[preflight] 后端 offline/simulation 门禁"
  ctest --test-dir build_check --output-on-failure \
    --no-tests=error \
    -L "offline|simulation" \
    -LE "hardware|manual"
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

echo "[preflight] 全部检查通过。"
