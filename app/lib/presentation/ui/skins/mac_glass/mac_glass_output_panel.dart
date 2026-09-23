import 'package:flutter/material.dart';

import '../../../../application/connection_models.dart';
import 'mac_glass_close_button.dart';
import 'mac_glass_tokens.dart';

final class MacGlassOutputPanel extends StatelessWidget {
  const MacGlassOutputPanel({
    required this.messages,
    required this.connected,
    required this.onClose,
    this.disconnectMessage,
    super.key,
  });

  final List<FlightControllerMessage> messages;
  final bool connected;
  final String? disconnectMessage;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      key: const ValueKey('mg-flight-info-popover'),
      height: 226,
      child: Stack(
        clipBehavior: Clip.none,
        children: [
          Positioned(
            top: 1,
            right: 30,
            child: Transform.rotate(
              angle: 0.785398,
              child: DecoratedBox(
                key: const ValueKey('mg-flight-info-anchor'),
                decoration: BoxDecoration(
                  color: const Color(0xE8FFFFFF),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.82),
                  ),
                  borderRadius: BorderRadius.circular(3),
                ),
                child: const SizedBox.square(dimension: 14),
              ),
            ),
          ),
          Positioned.fill(
            top: 7,
            child: ClipRRect(
              borderRadius: BorderRadius.circular(18),
              child: BackdropFilter(
                filter: MacGlassTokens.strongBlur,
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: const Color(0xD9FFFFFF),
                    border: Border.all(
                      color: Colors.white.withValues(alpha: 0.82),
                    ),
                    borderRadius: BorderRadius.circular(18),
                    boxShadow: [
                      BoxShadow(
                        color: MacGlassTokens.black.withValues(alpha: 0.13),
                        blurRadius: 32,
                        offset: const Offset(0, 14),
                        spreadRadius: -6,
                      ),
                      BoxShadow(
                        color: Colors.white.withValues(alpha: 0.58),
                        blurRadius: 1,
                        offset: const Offset(0, -1),
                      ),
                    ],
                  ),
                  child: Column(
                    children: [
                      SizedBox(
                        height: 54,
                        child: Padding(
                          padding: const EdgeInsets.fromLTRB(16, 7, 8, 5),
                          child: Row(
                            children: [
                              const Expanded(
                                child: Text(
                                  '飞控信息',
                                  style: TextStyle(
                                    fontSize: 14,
                                    fontWeight: FontWeight.w800,
                                  ),
                                ),
                              ),
                              MacGlassCloseButton(
                                key: const ValueKey('mg-close-flight-info'),
                                tooltip: '关闭飞控信息',
                                onPressed: onClose,
                              ),
                            ],
                          ),
                        ),
                      ),
                      Divider(
                        height: 1,
                        color: MacGlassTokens.ink.withValues(alpha: 0.07),
                      ),
                      Expanded(
                        child: !connected
                            ? _DisconnectedFlightInfo(
                                message: disconnectMessage ?? '飞控未连接',
                              )
                            : messages.isEmpty
                            ? const SizedBox()
                            : ListView.separated(
                                padding: const EdgeInsets.symmetric(
                                  horizontal: 16,
                                  vertical: 10,
                                ),
                                itemCount: messages.length,
                                separatorBuilder: (_, _) =>
                                    const SizedBox(height: 8),
                                itemBuilder: (context, index) {
                                  final message = messages.reversed.elementAt(
                                    index,
                                  );
                                  final time = message.receivedAt;
                                  return Text(
                                    '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}:${time.second.toString().padLeft(2, '0')}  ${message.text}',
                                    style: const TextStyle(fontSize: 12),
                                  );
                                },
                              ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

final class _DisconnectedFlightInfo extends StatelessWidget {
  const _DisconnectedFlightInfo({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return Center(
      key: const ValueKey('mg-flight-info-disconnected-state'),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            Icons.link_off_rounded,
            size: 24,
            color: MacGlassTokens.muted.withValues(alpha: 0.78),
          ),
          const SizedBox(height: 8),
          const Text(
            '未连接飞控',
            style: TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
          ),
          const SizedBox(height: 3),
          Text(
            message,
            style: const TextStyle(fontSize: 11, color: MacGlassTokens.muted),
          ),
        ],
      ),
    );
  }
}
