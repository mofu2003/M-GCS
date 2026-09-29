#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root_dir"

tmp_dir="$(mktemp -d)"
cleanup() {
  rm -f "$tmp_dir"/*
  rmdir "$tmp_dir"
}
trap cleanup EXIT

all_exports="$tmp_dir/all_exports"
frontend_used="$tmp_dir/frontend_used"
frontend_exports="$tmp_dir/frontend_exports"
test_touched="$tmp_dir/test_touched"
test_exports="$tmp_dir/test_exports"
all_used="$tmp_dir/all_used"

: > "$all_exports"

for header in bridge/ffi/ffi_api*.h; do
  domain="$(basename "$header" .h)"
  domain="${domain#ffi_api_}"
  exports_file="$tmp_dir/exports_$domain"

  awk '
    /^MGCORE_API/ {
      while (match($0, /mg_[A-Za-z0-9_]+[ \t]*\(/)) {
        name = substr($0, RSTART, RLENGTH)
        sub(/[ \t]*\($/, "", name)
        print name
        $0 = substr($0, RSTART + RLENGTH)
      }
    }
  ' "$header" |
    sort -u > "$exports_file"
  cat "$exports_file" >> "$all_exports"
done

sort -u "$all_exports" -o "$all_exports"

cat > "$tmp_dir/extract_frontend.awk" <<'AWK'
{
  line = $0
  while (match(line, /["']mg_[A-Za-z0-9_]+/)) {
    tok = substr(line, RSTART, RLENGTH)
    sub(/^["']/, "", tok)
    print tok
    line = substr(line, RSTART + RLENGTH)
  }
}
AWK

cat > "$tmp_dir/extract_tests.awk" <<'AWK'
{
  line = $0
  # 匹配裸符号而不是只匹配调用点：测试里也会把 FFI 入口作为函数指针传递
  # （例如 call_with_pump(mg_serial_get_uart_names)），只认 `name(` 会漏统计。
  # 伪符号（mg_core_t 等类型名）会在与导出表求交集时被过滤掉。
  while (match(line, /mg_[A-Za-z0-9_]+/)) {
    tok = substr(line, RSTART, RLENGTH)
    print tok
    line = substr(line, RSTART + RLENGTH)
  }
}
AWK

find flutter_shell/lib -type f -name '*.dart' \
  -exec awk -f "$tmp_dir/extract_frontend.awk" {} + |
  sort -u > "$frontend_used"

find core/tests tests -type f \( -name '*.cc' -o -name '*.h' \) \
  -exec awk -f "$tmp_dir/extract_tests.awk" {} + |
  sort -u > "$test_touched"

comm -12 "$all_exports" "$frontend_used" > "$frontend_exports"
comm -12 "$all_exports" "$test_touched" > "$test_exports"

cat "$frontend_exports" "$test_exports" |
  sort -u > "$all_used"

commit="$(git rev-parse --short HEAD)"
branch="$(git branch --show-current)"
dirty_count="$(
  git status --porcelain --untracked-files=no | wc -l | tr -d ' '
)"

total_exports="$(wc -l < "$all_exports" | tr -d ' ')"
frontend_count="$(wc -l < "$frontend_exports" | tr -d ' ')"
test_count="$(wc -l < "$test_exports" | tr -d ' ')"
untouched_count="$(
  comm -23 "$all_exports" "$all_used" | wc -l | tr -d ' '
)"

echo "# 自动审计统计"
echo
echo "- branch: \`$branch\`"
echo "- commit: \`$commit\`"
echo "- tracked working-tree changes: \`$dirty_count\`"
echo "- FFI exports: \`$total_exports\`"
echo "- frontend used: \`$frontend_count\`"
echo "- test touched: \`$test_count\`"
echo "- neither frontend nor tests: \`$untouched_count\`"
echo
echo "| 领域头 | 导出 | 前端在用 | 仅测试 | 两者皆无 |"
echo "| --- | ---: | ---: | ---: | ---: |"

for header in bridge/ffi/ffi_api*.h; do
  domain="$(basename "$header" .h)"
  domain="${domain#ffi_api_}"
  exports_file="$tmp_dir/exports_$domain"

  export_count="$(wc -l < "$exports_file" | tr -d ' ')"
  frontend_count_domain="$(
    comm -12 "$exports_file" "$frontend_exports" | wc -l | tr -d ' '
  )"
  untouched_count_domain="$(
    comm -23 "$exports_file" "$all_used" | wc -l | tr -d ' '
  )"
  # 三列互斥：仅测试 = 导出 - 前端在用 - 两者皆无。
  # 直接用 comm -12 exports test_exports 会把"前端也在用"的算进来，导致三列之和超过导出数。
  test_only_count_domain=$((export_count - frontend_count_domain - untouched_count_domain))

  echo "| $domain | $export_count | $frontend_count_domain | $test_only_count_domain | $untouched_count_domain |"
done

echo
echo "## 既无前端调用也无测试触达"
echo
comm -23 "$all_exports" "$all_used" | sed 's/^/- `/; s/$/`/'
