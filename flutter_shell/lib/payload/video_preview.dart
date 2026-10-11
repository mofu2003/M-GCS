import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter/widgets.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'payload_backend.dart';
import 'payload_controller.dart';

/// 本地视频预览。
///
/// 只在 [PayloadController.videoRendererStatus] 报告 `running` 时轮询最新帧
/// （上限约 30 fps），用 [ui.decodeImageFromPixels] 把 native 交回的 RGBA8888
/// 帧渲染出来。没有可用后端、或还没收到帧时，画面区显示确定文案，不画假画面、
/// 不谎报正在播放。
///
/// 页面仍然只经 [PayloadController]：帧数据通过 `controller.pollFrame()` 取，
/// 这个组件不接触 backend 或 `dart:ffi`（U1/A5）。
class VideoPreview extends StatefulWidget {
  const VideoPreview({super.key, required this.controller, this.height = 200});

  final PayloadController controller;

  /// 画面区高度；宽度撑满父容器。
  final double height;

  @override
  State<VideoPreview> createState() => _VideoPreviewState();
}

class _VideoPreviewState extends State<VideoPreview> {
  /// 帧轮询间隔（约 30 fps 上限）。真正的节流来自序列号去重。
  static const Duration _frameInterval = Duration(milliseconds: 33);

  Timer? _timer;
  ui.Image? _image;
  bool _decoding = false;
  int _lastSequence = -1;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onControllerChanged);
    _syncTimer();
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onControllerChanged);
    _timer?.cancel();
    _image?.dispose();
    super.dispose();
  }

  void _onControllerChanged() {
    if (!mounted) return;
    _syncTimer();
  }

  void _syncTimer() {
    final running = widget.controller.videoRendererStatus?.running ?? false;
    if (running && _timer == null) {
      _lastSequence = -1;
      _timer = Timer.periodic(_frameInterval, (_) => _poll());
    } else if (!running && _timer != null) {
      _timer!.cancel();
      _timer = null;
      _lastSequence = -1;
    }
    if (mounted) setState(() {});
  }

  Future<void> _poll() async {
    if (_decoding || !mounted) return;
    final frame = widget.controller.pollFrame();
    if (frame == null) return;
    if (frame.sequence == _lastSequence) return;
    _lastSequence = frame.sequence;
    _decoding = true;
    try {
      final image = await _decode(frame);
      if (!mounted) {
        image.dispose();
        return;
      }
      final previous = _image;
      setState(() => _image = image);
      previous?.dispose();
    } on Object catch (_) {
      // 单帧解码失败不升级为页面错误，等待下一帧即可。
    } finally {
      _decoding = false;
    }
  }

  static Future<ui.Image> _decode(VideoFrameData frame) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(
      frame.rgba,
      frame.width,
      frame.height,
      ui.PixelFormat.rgba8888,
      completer.complete,
    );
    return completer.future;
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final image = _image;
    return Container(
      height: widget.height,
      decoration: BoxDecoration(
        color: colors.surfaceMuted,
        border: Border.all(color: colors.divider),
        borderRadius: BorderRadius.circular(8),
      ),
      clipBehavior: Clip.antiAlias,
      child: image == null
          ? Center(child: _placeholder(context))
          : Center(
              child: AspectRatio(
                aspectRatio: image.width / image.height,
                child: RawImage(image: image, fit: BoxFit.contain),
              ),
            ),
    );
  }

  Widget _placeholder(BuildContext context) {
    final running = widget.controller.videoRendererStatus?.running ?? false;
    return Text(
      running ? '等待首个视频帧…' : '未播放',
      style: MgTypography.sans(
        MgTypography.caption.copyWith(color: context.mgColors.textSecondary),
      ),
    );
  }
}
