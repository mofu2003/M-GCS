import 'package:flutter/foundation.dart';

import '../ffi_response.dart';
import 'payload_backend.dart';
import 'payload_models.dart';
import 'video_storage.dart';

/// 相机 / 云台 / 视频源页的状态机。
///
/// 负责四件事：
/// - 读取相机与云台设备清单，并在多设备时按 `componentId` 选择当前查看的设备；
/// - 区分「已查询、当前无设备」（native `available:false`，合法状态）与
///   「调用失败」（`success:false` 规范错误对象）——两者在界面上措辞不同；
/// - 发送拍照 / 录像 / 变焦 / 云台指向一类的命令，成功只表示数据包已发出，
///   不猜测飞控是否执行，界面上用「已发送」而不是「已成功」；
/// - 会话切换时丢弃全部缓存，避免把上一架飞机的设备状态写回界面。
///
/// 命令接口没有 ACK，所以控制器不把「发送成功」当成「功能成功」；
/// 真实结果（录像中、拍摄计数、姿态）由下一次 [refresh] 读回。
class PayloadController extends ChangeNotifier {
  PayloadController({required this.backend});

  final PayloadBackend backend;

  CameraStatus? _cameraStatus;
  GimbalStatus? _gimbalStatus;
  GimbalAttitude? _gimbalAttitude;
  VideoStatus? _videoStatus;
  List<VideoStreamInfo> _videoStreams = const [];
  VideoRendererStatus? _rendererStatus;
  VideoRecordingStatus? _recordingStatus;

  /// 当前查看的相机组件 id；null 表示「第一台 / 活动目标」。
  int? _selectedCameraId;

  /// 当前查看的云台组件 id；null 表示「第一台 / 活动目标」。
  int? _selectedGimbalId;

  bool _loading = false;
  bool _busy = false;
  String? _listError;
  String? _actionError;
  String? _actionMessage;

  int? _sessionId;
  bool _disposed = false;

  CameraStatus? get cameraStatus => _cameraStatus;
  GimbalStatus? get gimbalStatus => _gimbalStatus;
  GimbalAttitude? get gimbalAttitude => _gimbalAttitude;
  VideoStatus? get videoStatus => _videoStatus;
  List<VideoStreamInfo> get videoStreams => _videoStreams;

  /// 本机解码后端状态（不是飞控能力）。未连接时也照常读取。
  VideoRendererStatus? get videoRendererStatus => _rendererStatus;

  /// 本机录制状态（不是飞控能力）。未连接时也照常读取。
  VideoRecordingStatus? get recordingStatus => _recordingStatus;

  /// 是否正在把本地码流写入文件。
  bool get isRecording => _recordingStatus?.recording ?? false;

  int? get selectedCameraId => _selectedCameraId;
  int? get selectedGimbalId => _selectedGimbalId;

  /// 发现的相机 / 云台清单（来自读接口的设备数组，不依赖是否选中）。
  List<CameraDevice> get cameras => _cameraStatus?.devices ?? const [];
  List<GimbalDevice> get gimbals => _gimbalStatus?.devices ?? const [];

  bool get loading => _loading;
  bool get busy => _busy;

  /// 列表级错误：读接口失败或未连接。无设备不算错误。
  String? get listError => _listError;
  String? get actionError => _actionError;

  /// 命令已发送一类的信息提示（非错误），由页面调 [clearActionMessage] 清掉。
  String? get actionMessage => _actionMessage;

  bool get connected => backend.isConnected;

  /// 绑定/切换连接会话。会话变化会丢弃全部设备状态与提示。
  void bindSession(int? sessionId) {
    if (_sessionId == sessionId) return;
    _sessionId = sessionId;
    _cameraStatus = null;
    _gimbalStatus = null;
    _gimbalAttitude = null;
    _videoStatus = null;
    _videoStreams = const [];
    _rendererStatus = null;
    _recordingStatus = null;
    _selectedCameraId = null;
    _selectedGimbalId = null;
    _listError = null;
    _actionError = null;
    _actionMessage = null;
    _loading = false;
    _busy = false;
    notifyListeners();
  }

  /// 切换当前查看的相机组件 id（null = 第一台），并立即重新读取状态。
  Future<void> selectCamera(int? componentId) async {
    if (_selectedCameraId == componentId) return;
    _selectedCameraId = componentId;
    _cameraStatus = null;
    notifyListeners();
    await refresh();
  }

  /// 切换当前查看的云台组件 id（null = 第一台），并立即重新读取状态。
  Future<void> selectGimbal(int? componentId) async {
    if (_selectedGimbalId == componentId) return;
    _selectedGimbalId = componentId;
    _gimbalStatus = null;
    _gimbalAttitude = null;
    notifyListeners();
    await refresh();
  }

  /// 读取相机、云台、姿态与视频流状态。
  ///
  /// 未连接时给出确定错误并清空缓存；已连接时把各读接口的错误合并展示，
  /// 但某个接口失败不影响其它接口的可用数据。
  Future<void> refresh() async {
    if (_disposed || _loading) return;
    _loading = true;
    _listError = null;
    notifyListeners();

    final errors = <String>[];
    // 本机解码/录制与飞控连接无关：即使未连接也要如实报告「本平台有没有解码后端」。
    _readRenderer(errors);
    _readRecording(errors);

    if (!backend.isConnected) {
      _cameraStatus = null;
      _gimbalStatus = null;
      _gimbalAttitude = null;
      _videoStatus = null;
      _videoStreams = const [];
      errors.insert(0, '未连接飞控，无法读取相机与云台状态');
      _listError = errors.join('；');
      _loading = false;
      notifyListeners();
      return;
    }

    try {
      _readCamera(errors);
      _readGimbal(errors);
      _readGimbalAttitude(errors);
      _readVideo(errors);
    } finally {
      _listError = errors.isEmpty ? null : errors.join('；');
      _loading = false;
      notifyListeners();
    }
  }

  /// 读取本机解码后端状态。`available:false` 是合法能力状态，不算错误。
  void _readRenderer(List<String> errors) {
    try {
      final response = backend.rendererStatus();
      if (!ffiSucceeded(response)) {
        errors.add(ffiErrorText(response, '读取本地视频解码状态失败'));
        return;
      }
      _rendererStatus = VideoRendererStatus.fromJson(response);
    } on Object catch (error) {
      errors.add('读取本地视频解码状态失败：$error');
    }
  }

  /// 读取本机录制状态。它是一个「本机后端」状态，未连接飞控时也照常读取。
  void _readRecording(List<String> errors) {
    try {
      final response = backend.videoRecordingStatus();
      if (!ffiSucceeded(response)) {
        errors.add(ffiErrorText(response, '读取本地录制状态失败'));
        return;
      }
      _recordingStatus = VideoRecordingStatus.fromJson(response);
    } on Object catch (error) {
      errors.add('读取本地录制状态失败：$error');
    }
  }

  void _readCamera(List<String> errors) {
    try {
      var response = backend.cameraStatus(_selectedCameraId ?? 0);
      if (!ffiSucceeded(response)) {
        errors.add(ffiErrorText(response, '读取相机状态失败'));
        return;
      }
      var status = CameraStatus.fromJson(response);
      // 选中的相机已消失（换机/重发现）就退回第一台，避免界面停在一台不存在的设备。
      if (_selectedCameraId != null &&
          !status.devices.any((d) => d.componentId == _selectedCameraId)) {
        _selectedCameraId = null;
        response = backend.cameraStatus(0);
        if (ffiSucceeded(response)) status = CameraStatus.fromJson(response);
      }
      _cameraStatus = status;
    } on Object catch (error) {
      errors.add('读取相机状态失败：$error');
    }
  }

  void _readGimbal(List<String> errors) {
    try {
      var response = backend.gimbalStatus(_selectedGimbalId ?? 0);
      if (!ffiSucceeded(response)) {
        errors.add(ffiErrorText(response, '读取云台状态失败'));
        return;
      }
      var status = GimbalStatus.fromJson(response);
      if (_selectedGimbalId != null &&
          !status.devices.any((d) => d.componentId == _selectedGimbalId)) {
        _selectedGimbalId = null;
        response = backend.gimbalStatus(0);
        if (ffiSucceeded(response)) status = GimbalStatus.fromJson(response);
      }
      _gimbalStatus = status;
    } on Object catch (error) {
      errors.add('读取云台状态失败：$error');
    }
  }

  void _readGimbalAttitude(List<String> errors) {
    try {
      final response = backend.gimbalAttitude(_selectedGimbalId ?? 0);
      if (!ffiSucceeded(response)) {
        errors.add(ffiErrorText(response, '读取云台姿态失败'));
        return;
      }
      _gimbalAttitude = GimbalAttitude.fromJson(response);
    } on Object catch (error) {
      errors.add('读取云台姿态失败：$error');
    }
  }

  void _readVideo(List<String> errors) {
    try {
      final statusResponse = backend.videoStatus();
      if (ffiSucceeded(statusResponse)) {
        _videoStatus = VideoStatus.fromJson(statusResponse);
      } else {
        errors.add(ffiErrorText(statusResponse, '读取视频流状态失败'));
      }
    } on Object catch (error) {
      errors.add('读取视频流状态失败：$error');
    }
    try {
      // 更完整的流定义（VIDEO_STREAM_INFORMATION）。无连接时 native 返回
      // NOT_CONNECTED，前面已按 isConnected 拦截，这里只在失败时保留旧列表。
      final streamsResponse = backend.videoStreams();
      if (ffiSucceeded(streamsResponse)) {
        _videoStreams = [
          for (final raw
              in (streamsResponse['streams'] as List<Object?>? ?? const []))
            if (raw is Map)
              VideoStreamInfo.fromJson(Map<String, Object?>.from(raw)),
        ];
      }
    } on Object catch (error) {
      errors.add('读取视频流定义失败：$error');
    }
  }

  Future<void> takePhoto() => _send(
    () => backend.cameraTakePhoto(_selectedCameraId ?? 0),
    '已发送拍照指令，等待相机上报结果',
  );

  Future<void> startRecording() => _send(
    () => backend.cameraStartVideo(_selectedCameraId ?? 0),
    '已发送开始录像指令',
  );

  Future<void> stopRecording() =>
      _send(() => backend.cameraStopVideo(_selectedCameraId ?? 0), '已发送停止录像指令');

  Future<void> stopCapture() => _send(
    () => backend.cameraStopCapture(_selectedCameraId ?? 0),
    '已发送停止拍摄指令',
  );

  Future<void> setZoom(double zoomLevel) => _send(
    () => backend.cameraSetZoom(_selectedCameraId ?? 0, zoomLevel),
    '已发送变焦指令',
  );

  Future<void> setMode(int mode) => _send(
    () => backend.cameraSetMode(_selectedCameraId ?? 0, mode),
    '已发送模式切换指令',
  );

  Future<void> requestCameraInfo() => _send(
    () => backend.cameraRequestInfo(_selectedCameraId ?? 0),
    '已请求相机上报信息',
  );

  Future<void> requestGimbalInfo() =>
      _send(() => backend.gimbalRequestInfo(), '已请求云台上报信息');

  /// 云台指向：pitch/yaw 单位为度。命令发往当前飞机的云台管理器。
  Future<void> gimbalControl({
    required double pitchDeg,
    required double yawDeg,
  }) => _send(() => backend.gimbalControl(pitchDeg, yawDeg), '已发送云台指向指令');

  Future<void> gimbalRecenter() =>
      _send(() => backend.gimbalRecenter(), '已发送云台回中指令');

  /// 启动本地视频源。没有编译进解码后端时 native 返回 `NOT_SUPPORTED`；
  /// 这里不吞掉这个结果，错误会照实显示为「本地解码未支持」。
  ///
  /// 这是本机操作，不依赖飞控连接（与相机命令不同），所以不走 [requiresVehicle] 检查。
  Future<void> startVideo(String uri) async {
    final trimmed = uri.trim();
    if (trimmed.isEmpty) {
      _actionError = '请先填写视频源地址';
      notifyListeners();
      return;
    }
    await _send(
      () => backend.videoStart(trimmed),
      '已请求启动本地视频源',
      requiresVehicle: false,
    );
    _refreshRendererQuietly();
  }

  Future<void> stopVideo() async {
    await _send(() => backend.videoStop(), '已停止本地视频源', requiresVehicle: false);
    _refreshRendererQuietly();
  }

  /// 开始本地录制：把正在播放的码流写进 [settings] 目录下的一个新文件。
  ///
  /// 这是本机操作，不需要飞控连接；但需要本地视频源已经在跑，否则 native 会返回
  /// `INVALID_STATE`。启用容量上限时，先把目录按「越旧越先删」压回上限内。
  Future<void> startLocalRecording(VideoStorageSettings settings) async {
    if (_disposed) return;
    if (isRecording) {
      noteInvalidInput('已在录制中，请先停止当前录制');
      return;
    }
    final directory = settings.directory.trim();
    if (directory.isEmpty) {
      noteInvalidInput('请先设置录制目录');
      return;
    }
    final dirError = await ensureDirectory(directory);
    if (dirError != null) {
      noteInvalidInput(dirError);
      return;
    }
    if (settings.limitEnabled) {
      await pruneOldRecordings(
        directory: directory,
        maxMegabytes: settings.maxMegabytes,
      );
    }
    final path = '$directory/${recordingFileName(DateTime.now())}';
    await _send(
      () => backend.videoStartRecording(path),
      '已开始本地录制：$path',
      requiresVehicle: false,
    );
    await refreshRecordingStatus();
  }

  /// 停止本地录制并读回最终状态（文件大小/时长）。
  Future<void> stopLocalRecording() async {
    await _send(
      () => backend.videoStopRecording(),
      '已停止本地录制',
      requiresVehicle: false,
    );
    await refreshRecordingStatus();
  }

  /// 把当前帧存成截图。同样是本机操作，与飞控连接无关。
  Future<void> captureLocalFrame(VideoStorageSettings settings) async {
    if (_disposed) return;
    final directory = settings.directory.trim();
    if (directory.isEmpty) {
      noteInvalidInput('请先设置录制目录');
      return;
    }
    final dirError = await ensureDirectory(directory);
    if (dirError != null) {
      noteInvalidInput(dirError);
      return;
    }
    final path = '$directory/${snapshotFileName(DateTime.now())}';
    await _send(
      () => backend.videoCaptureFrame(path),
      '已保存截图：$path',
      requiresVehicle: false,
    );
  }

  /// 只刷新本机录制状态，不影响设备列表。
  Future<void> refreshRecordingStatus() async {
    if (_disposed) return;
    _readRecording(<String>[]);
    notifyListeners();
  }

  /// 取本机解码器的最新一帧；没有新帧或没有后端时返回 null。
  /// 页面预览走这条缝，因此页面仍然只接触控制器，不接触 backend（U1）。
  VideoFrameData? pollFrame() => backend.pollFrame();

  /// 命令后刷新运行状态，失败不污染列表错误区（命令结果已在 action 区展示）。
  void _refreshRendererQuietly() {
    if (_disposed) return;
    _readRenderer(<String>[]);
    notifyListeners();
  }

  Future<void> _send(
    Map<String, Object?> Function() action,
    String okMessage, {
    bool requiresVehicle = true,
  }) async {
    if (_disposed) return;
    if (requiresVehicle && !backend.isConnected) {
      _actionError = '未连接飞控，无法发送指令';
      notifyListeners();
      return;
    }
    _busy = true;
    _actionError = null;
    _actionMessage = null;
    notifyListeners();
    try {
      final response = action();
      if (ffiSucceeded(response)) {
        _actionMessage = okMessage;
      } else {
        _actionError = ffiErrorText(response, '指令发送失败');
      }
    } on Object catch (error) {
      _actionError = '指令发送异常：$error';
    } finally {
      _busy = false;
      notifyListeners();
    }
  }

  /// 页面输入不合法时复用同一条提示通道，避免每个控件各造一套错误文案。
  void noteInvalidInput(String message) {
    _actionError = message;
    notifyListeners();
  }

  void clearActionError() {
    if (_actionError == null) return;
    _actionError = null;
    notifyListeners();
  }

  void clearActionMessage() {
    if (_actionMessage == null) return;
    _actionMessage = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }
}
