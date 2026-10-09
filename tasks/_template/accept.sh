#!/usr/bin/env bash
# 在实现者或验收者的 worktree 根目录运行。写卡后冻结，哈希记录在 card.md 中。
set -u

main_checkout="/Users/fansicheng/Projects/M-GCS"
root="$(git rev-parse --show-toplevel)"
if [ "$root" = "$main_checkout" ]; then
  echo "不要在主检出中运行，请在自己的 worktree 根目录运行。" >&2
  exit 2
fi
cd "$root"

failed=0
run() {
  local name="$1"
  shift
  echo "== $name"
  echo "\$ $*"
  "$@"
  local code=$?
  echo "EXIT=$code"
  [ "$code" -eq 0 ] || failed=$((failed + 1))
}

# 每个验收项一行。以下为示例，写卡时按任务替换。
run "补丁格式" git diff --check main...HEAD
# run "新行为测试" bash -c "cd flutter_shell && flutter test test/<file>_test.dart"
# run "后端门禁" ctest --test-dir build_check --output-on-failure -L "offline|simulation" -LE "hardware|manual"
# run "全量门禁" tools/preflight.sh --no-macos-build

echo "== 汇总：失败 $failed 项"
[ "$failed" -eq 0 ]
