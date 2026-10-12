import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../connection/native_core.dart';
import 'file_backend.dart';

/// `mg_fs_*` 的 FFI 实现。
///
/// 三条发起路径都只入队并立刻返回，真正的传输在 native 后台线程；
/// 本类不做任何轮询或等待，状态由 [transferStatus] 读取。
final class FileFfiBackend implements FileBackend {
  FileFfiBackend(
    this._library,
    this._core, {
    required bool Function() isConnected,
  }) {
    _isConnected = isConnected;
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _listDirectory = _lookupPath1('mg_fs_list_directory');
    _transferStatus = _lookupVoid('mg_fs_transfer_status');
    _cancel = _lookupVoid('mg_fs_cancel');
    _downloadFile = _lookupPath2('mg_fs_download_file');
    _uploadFile = _lookupPath2('mg_fs_upload_file');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;
  late final bool Function() _isConnected;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _listDirectory;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  _downloadFile;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  _uploadFile;
  late final Pointer<Utf8> Function(Pointer<Void>) _transferStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _cancel;

  Pointer<Utf8> Function(Pointer<Void>) _lookupVoid(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _lookupPath1(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
      >(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>) _lookupPath2(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
        >
      >(symbol)
      .asFunction();

  @override
  bool get isConnected => _isConnected();

  @override
  Map<String, Object?> listDirectory(String remotePath) {
    final remote = remotePath.toNativeUtf8();
    try {
      return _read(_listDirectory(_core, remote));
    } finally {
      malloc.free(remote);
    }
  }

  @override
  Map<String, Object?> downloadFile(String remotePath, String localPath) {
    final remote = remotePath.toNativeUtf8();
    final local = localPath.toNativeUtf8();
    try {
      return _read(_downloadFile(_core, remote, local));
    } finally {
      malloc.free(remote);
      malloc.free(local);
    }
  }

  @override
  Map<String, Object?> uploadFile(String localPath, String remotePath) {
    final local = localPath.toNativeUtf8();
    final remote = remotePath.toNativeUtf8();
    try {
      return _read(_uploadFile(_core, local, remote));
    } finally {
      malloc.free(local);
      malloc.free(remote);
    }
  }

  @override
  Map<String, Object?> transferStatus() => _read(_transferStatus(_core));

  @override
  Map<String, Object?> cancel() => _read(_cancel(_core));

  Map<String, Object?> _read(Pointer<Utf8> pointer) => readJsonMap(
    pointer,
    freeString: _freeString,
    invalidResponseMessage: '文件传输响应格式无效',
  );
}
