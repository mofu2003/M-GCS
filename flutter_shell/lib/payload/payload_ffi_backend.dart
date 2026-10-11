import 'dart:convert';
import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'payload_backend.dart';

/// `mg_camera_*` / `mg_gimbal_*` / `mg_video_*` 的 FFI 实现。
///
/// 所有读接口返回的 JSON 由 `mg_free_string` 释放（ffi-contract F3）；
/// 调用方只拿到解析后的 Map，不接触指针所有权。
final class PayloadFfiBackend implements PayloadBackend {
  PayloadFfiBackend(
    this._library,
    this._core, {
    required bool Function() isConnected,
  }) {
    _isConnected = isConnected;
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _cameraGetStatus = _lookupInt('mg_camera_get_status');
    _cameraRequestInfo = _lookupInt('mg_camera_request_info');
    _cameraTakePhoto = _lookupInt('mg_camera_take_photo');
    _cameraStartVideo = _lookupInt('mg_camera_start_video');
    _cameraStopVideo = _lookupInt('mg_camera_stop_video');
    _cameraStopCapture = _lookupInt('mg_camera_stop_capture');
    _gimbalGetStatus = _lookupInt('mg_gimbal_get_status');
    _gimbalGetAttitude = _lookupInt('mg_gimbal_get_attitude');
    _gimbalRequestInfo = _lookup1('mg_gimbal_request_info');
    _gimbalRecenter = _lookup1('mg_command_gimbal_recenter');
    _videoStatus = _lookup1('mg_video_status');
    _videoGetStreams = _lookup1('mg_video_get_streams');
    _videoStop = _lookup1('mg_video_stop');
    _videoRendererStatus = _lookup1('mg_video_renderer_status');
    _videoPollFrame = _library
        .lookup<
          NativeFunction<
            Int32 Function(
              Pointer<Void>,
              Pointer<Uint8>,
              Int32,
              Pointer<Int32>,
              Pointer<Int32>,
              Pointer<Int64>,
              Pointer<Uint64>,
            )
          >
        >('mg_video_poll_frame')
        .asFunction();
    _cameraSetZoom = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Float)>
        >('mg_camera_set_zoom')
        .asFunction();
    _cameraSetMode = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Int32)>
        >('mg_camera_set_mode')
        .asFunction();
    _gimbalControl = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Float, Float)>
        >('mg_command_gimbal_control')
        .asFunction();
    _videoStart = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
        >('mg_video_start')
        .asFunction();
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;
  late final bool Function() _isConnected;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraGetStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraRequestInfo;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraTakePhoto;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraStartVideo;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraStopVideo;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _cameraStopCapture;
  late final Pointer<Utf8> Function(Pointer<Void>, int, double) _cameraSetZoom;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _cameraSetMode;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _gimbalGetStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _gimbalGetAttitude;
  late final Pointer<Utf8> Function(Pointer<Void>) _gimbalRequestInfo;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double)
  _gimbalControl;
  late final Pointer<Utf8> Function(Pointer<Void>) _gimbalRecenter;
  late final Pointer<Utf8> Function(Pointer<Void>) _videoStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _videoGetStreams;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _videoStart;
  late final Pointer<Utf8> Function(Pointer<Void>) _videoStop;
  late final Pointer<Utf8> Function(Pointer<Void>) _videoRendererStatus;
  late final int Function(
    Pointer<Void>,
    Pointer<Uint8>,
    int,
    Pointer<Int32>,
    Pointer<Int32>,
    Pointer<Int64>,
    Pointer<Uint64>,
  )
  _videoPollFrame;

  /// 帧轮询复用同一块原生缓冲：容量不足时按 native 回报的尺寸扩容重试，
  /// 避免每帧都 malloc/free 一大块内存。释放见 [dispose]。
  Pointer<Uint8> _frameBuffer = nullptr;
  int _frameCapacity = 0;
  bool _disposed = false;

  Pointer<Utf8> Function(Pointer<Void>, int) _lookupInt(String symbol) =>
      _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            symbol,
          )
          .asFunction();

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  @override
  bool get isConnected => _isConnected();

  @override
  Map<String, Object?> cameraStatus(int cameraId) =>
      _read(_cameraGetStatus(_core, cameraId));

  @override
  Map<String, Object?> cameraRequestInfo(int cameraId) =>
      _read(_cameraRequestInfo(_core, cameraId));

  @override
  Map<String, Object?> cameraTakePhoto(int cameraId) =>
      _read(_cameraTakePhoto(_core, cameraId));

  @override
  Map<String, Object?> cameraStartVideo(int cameraId) =>
      _read(_cameraStartVideo(_core, cameraId));

  @override
  Map<String, Object?> cameraStopVideo(int cameraId) =>
      _read(_cameraStopVideo(_core, cameraId));

  @override
  Map<String, Object?> cameraStopCapture(int cameraId) =>
      _read(_cameraStopCapture(_core, cameraId));

  @override
  Map<String, Object?> cameraSetZoom(int cameraId, double zoomLevel) =>
      _read(_cameraSetZoom(_core, cameraId, zoomLevel));

  @override
  Map<String, Object?> cameraSetMode(int cameraId, int mode) =>
      _read(_cameraSetMode(_core, cameraId, mode));

  @override
  Map<String, Object?> gimbalStatus(int gimbalId) =>
      _read(_gimbalGetStatus(_core, gimbalId));

  @override
  Map<String, Object?> gimbalAttitude(int gimbalId) =>
      _read(_gimbalGetAttitude(_core, gimbalId));

  @override
  Map<String, Object?> gimbalRequestInfo() => _read(_gimbalRequestInfo(_core));

  @override
  Map<String, Object?> gimbalControl(double pitchDeg, double yawDeg) =>
      _read(_gimbalControl(_core, pitchDeg, yawDeg));

  @override
  Map<String, Object?> gimbalRecenter() => _read(_gimbalRecenter(_core));

  @override
  Map<String, Object?> videoStatus() => _read(_videoStatus(_core));

  @override
  Map<String, Object?> videoStreams() => _read(_videoGetStreams(_core));

  @override
  Map<String, Object?> videoStart(String uri) {
    final nativeUri = uri.toNativeUtf8();
    try {
      return _read(_videoStart(_core, nativeUri));
    } finally {
      malloc.free(nativeUri);
    }
  }

  @override
  Map<String, Object?> videoStop() => _read(_videoStop(_core));

  @override
  Map<String, Object?> rendererStatus() => _read(_videoRendererStatus(_core));

  @override
  VideoFrameData? pollFrame() {
    if (_disposed) return null;
    final outWidth = calloc<Int32>();
    final outHeight = calloc<Int32>();
    final outPts = calloc<Int64>();
    final outSeq = calloc<Uint64>();
    try {
      if (_frameBuffer == nullptr) _growFrameBuffer(1);
      var copied = _videoPollFrame(
        _core,
        _frameBuffer,
        _frameCapacity,
        outWidth,
        outHeight,
        outPts,
        outSeq,
      );
      if (copied < 0) {
        // 容量不足：native 已在 out_* 里回报所需尺寸，扩容后重试一次。
        final width = outWidth.value;
        final height = outHeight.value;
        if (width <= 0 || height <= 0) return null;
        _growFrameBuffer(width * height * 4);
        copied = _videoPollFrame(
          _core,
          _frameBuffer,
          _frameCapacity,
          outWidth,
          outHeight,
          outPts,
          outSeq,
        );
      }
      if (copied <= 0) return null;
      return VideoFrameData(
        width: outWidth.value,
        height: outHeight.value,
        rgba: Uint8List.fromList(_frameBuffer.asTypedList(copied)),
        ptsUs: outPts.value,
        sequence: outSeq.value,
      );
    } finally {
      calloc.free(outWidth);
      calloc.free(outHeight);
      calloc.free(outPts);
      calloc.free(outSeq);
    }
  }

  void _growFrameBuffer(int bytes) {
    final needed = bytes < 64 * 1024 ? 64 * 1024 : bytes;
    if (_frameBuffer != nullptr && needed <= _frameCapacity) return;
    if (_frameBuffer != nullptr) malloc.free(_frameBuffer);
    _frameBuffer = malloc<Uint8>(needed);
    _frameCapacity = needed;
  }

  @override
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    if (_frameBuffer != nullptr) {
      malloc.free(_frameBuffer);
      _frameBuffer = nullptr;
    }
    _frameCapacity = 0;
  }

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    final Object? raw;
    try {
      raw = jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
    if (raw is! Map) throw const FormatException('相机/云台响应格式无效');
    return Map<String, Object?>.from(raw);
  }
}
