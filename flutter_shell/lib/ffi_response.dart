/// FFI 响应的统一读取入口。
///
/// 规范的失败响应结构见 `docs/constraints/ffi-contract.md` 的 F4：
///
/// ```json
/// {"success":false,"error":{"code":-6,"name":"TIMEOUT","message":"..."}}
/// ```
///
/// `code` 是 `mg_error_t` 的数值，`name` 是稳定符号名，`message` 是可直接
/// 展示给用户的文本。任何 `success:false` 的响应都必须使用这一结构；调用方
/// 不需要、也不应该再判断 `error` 是字符串还是对象。
///
/// 没有 `success` 字段的查询结果（`valid`、`found` 之类的领域判定）是数据，
/// 不在这里解释。
library;

/// 是否为 `success:true` 的响应。
bool ffiSucceeded(Object? response) =>
    response is Map && response['success'] == true;

/// 取错误文案：优先规范错误对象的 `message`，其次 `name`，最后回退到 [fallback]。
String ffiErrorText(Object? response, String fallback) {
  if (response is Map) {
    final error = response['error'];
    if (error is Map) {
      final message = error['message'];
      if (message is String && message.isNotEmpty) return message;
      final name = error['name'];
      if (name is String && name.isNotEmpty) return name;
    }
  }
  return fallback;
}

/// 取 `mg_error_t` 数值码；没有规范错误对象时返回 null。
int? ffiErrorCode(Object? response) {
  if (response is Map) {
    final error = response['error'];
    if (error is Map) {
      final code = error['code'];
      if (code is int) return code;
    }
  }
  return null;
}
