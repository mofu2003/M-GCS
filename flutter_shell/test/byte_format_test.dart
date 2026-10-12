import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/logging/onboard_log_page.dart'
    show formatByteSize;

/// 字节数文案的统一口径。
///
/// 合并前 logging 与 camera 各有一份实现且行为已分叉：
/// * logging（onboard/file_browser/flight_log 三个页面用）没有 TB，`>= 100` 取 0 位小数；
/// * camera（录制读数用）有 TB，恒 1 位小数。
/// 现在统一到 [formatByteSize]：`>= 100` 取 0 位小数（列表行更紧凑），单位一直到 TB
/// （相机外置存储到 TB 级，合并后不能退化成 "2048 GB"）。
void main() {
  test('小于 1 KB 用字节', () {
    expect(formatByteSize(0), '0 B');
    expect(formatByteSize(512), '512 B');
    expect(formatByteSize(1023), '1023 B');
  });

  test('KB / MB 保留 1 位小数', () {
    expect(formatByteSize(1024), '1.0 KB');
    expect(formatByteSize(1536), '1.5 KB');
    expect(formatByteSize(1024 * 1024), '1.0 MB');
  });

  test('>= 100 取 0 位小数（与原 logging 口径一致，列表行更紧凑）', () {
    expect(formatByteSize(153600), '150 KB');
    expect(formatByteSize(100 * 1024), '100 KB');
    expect(formatByteSize(99 * 1024), '99.0 KB');
  });

  test('单位直到 TB（采纳原 camera 口径，避免退化成 2048 GB）', () {
    expect(formatByteSize(1024 * 1024 * 1024), '1.0 GB');
    expect(formatByteSize(2 * 1024 * 1024 * 1024 * 1024), '2.0 TB');
    expect(formatByteSize(100 * 1024 * 1024 * 1024 * 1024), '100 TB');
  });
}
