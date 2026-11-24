import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:url_launcher/url_launcher.dart';

/// 网络和文件打开都可替换，控制器测试不依赖真实更新服务器。
abstract interface class UpdateBackend {
  Future<Map<String, Object?>> fetchManifest(Uri uri);

  Future<void> download(
    Uri uri,
    File target, {
    void Function(int received, int? total)? onProgress,
  });

  Future<bool> openFile(File file);
}

final class HttpUpdateBackend implements UpdateBackend {
  const HttpUpdateBackend();

  static const Duration _connectTimeout = Duration(seconds: 15);
  static const Duration _responseTimeout = Duration(seconds: 20);

  @override
  Future<Map<String, Object?>> fetchManifest(Uri uri) async {
    final client = HttpClient()..connectionTimeout = _connectTimeout;
    try {
      final request = await client.getUrl(uri);
      request.headers.set(HttpHeaders.acceptHeader, 'application/json');
      request.headers.set(HttpHeaders.userAgentHeader, 'M-GCS-Updater/1.0');
      final response = await request.close().timeout(_responseTimeout);
      _ensureSecureRedirects(response);
      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('更新服务器返回 HTTP ${response.statusCode}');
      }
      final body = await response
          .transform(utf8.decoder)
          .join()
          .timeout(_responseTimeout);
      final decoded = jsonDecode(body);
      if (decoded is! Map) {
        throw const FormatException('更新清单不是 JSON 对象');
      }
      return Map<String, Object?>.from(decoded);
    } finally {
      client.close(force: true);
    }
  }

  @override
  Future<void> download(
    Uri uri,
    File target, {
    void Function(int received, int? total)? onProgress,
  }) async {
    final client = HttpClient()..connectionTimeout = _connectTimeout;
    IOSink? sink;
    try {
      final request = await client.getUrl(uri);
      request.followRedirects = true;
      request.headers.set(HttpHeaders.userAgentHeader, 'M-GCS-Updater/1.0');
      final response = await request.close().timeout(_responseTimeout);
      _ensureSecureRedirects(response);
      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('更新包下载返回 HTTP ${response.statusCode}');
      }
      // 自动解压后 Content-Length 是压缩前的长度，不能用于完整性判断。
      final total =
          response.compressionState ==
                  HttpClientResponseCompressionState.decompressed ||
              response.contentLength <= 0
          ? null
          : response.contentLength;
      await target.parent.create(recursive: true);
      sink = target.openWrite();
      var received = 0;
      await for (final chunk in response.timeout(_responseTimeout)) {
        sink.add(chunk);
        received += chunk.length;
        onProgress?.call(received, total);
      }
      await sink.flush();
      await sink.close();
      sink = null;
      if (total != null && received != total) {
        throw const HttpException('更新包下载不完整');
      }
    } on Object {
      await sink?.close();
      if (await target.exists()) await target.delete();
      rethrow;
    } finally {
      client.close(force: true);
    }
  }

  @override
  Future<bool> openFile(File file) =>
      launchUrl(Uri.file(file.path), mode: LaunchMode.externalApplication);

  static void _ensureSecureRedirects(HttpClientResponse response) {
    for (final redirect in response.redirects) {
      final uri = redirect.location;
      if (uri.scheme != 'https' ||
          uri.host.isEmpty ||
          uri.userInfo.isNotEmpty) {
        throw const HttpException('更新服务器重定向到不安全的地址');
      }
    }
  }
}
