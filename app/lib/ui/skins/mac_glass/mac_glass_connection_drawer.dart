import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_close_button.dart';
import 'mac_glass_menu.dart';
import 'mac_glass_tokens.dart';

final class MacGlassConnectionDrawer extends StatefulWidget {
  const MacGlassConnectionDrawer({
    required this.model,
    required this.onConnectPressed,
    required this.onDisconnectPressed,
    required this.onClose,
    required this.onTransportChanged,
    this.onSerialPortChanged,
    this.onBaudRateChanged,
    super.key,
  });

  final FrontstageConnectionModel model;
  final VoidCallback onConnectPressed;
  final VoidCallback onDisconnectPressed;
  final VoidCallback onClose;
  final ValueChanged<FrontstageConnectionTransport> onTransportChanged;
  final ValueChanged<String>? onSerialPortChanged;
  final ValueChanged<int>? onBaudRateChanged;

  @override
  State<MacGlassConnectionDrawer> createState() =>
      _MacGlassConnectionDrawerState();
}

final class _MacGlassConnectionDrawerState
    extends State<MacGlassConnectionDrawer> {
  final TextEditingController _udpAddressController = TextEditingController(
    text: '0.0.0.0',
  );
  final TextEditingController _udpPortController = TextEditingController(
    text: '14550',
  );
  final TextEditingController _tcpAddressController = TextEditingController(
    text: '127.0.0.1',
  );
  final TextEditingController _tcpPortController = TextEditingController(
    text: '5760',
  );

  @override
  void dispose() {
    _udpAddressController.dispose();
    _udpPortController.dispose();
    _tcpAddressController.dispose();
    _tcpPortController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final selected = widget.model.selectedTransport;
    return ClipRRect(
      key: const ValueKey('mg-connection-drawer'),
      borderRadius: BorderRadius.circular(24),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.64),
            border: Border.all(color: Colors.white.withValues(alpha: 0.88)),
            borderRadius: BorderRadius.circular(24),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Padding(
            padding: const EdgeInsets.all(14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: SegmentedButton<FrontstageConnectionTransport>(
                        key: const ValueKey('mg-connection-transport-selector'),
                        showSelectedIcon: false,
                        segments: const [
                          ButtonSegment(
                            value: FrontstageConnectionTransport.serial,
                            label: Text('Serial'),
                          ),
                          ButtonSegment(
                            value: FrontstageConnectionTransport.udp,
                            label: Text('UDP'),
                          ),
                          ButtonSegment(
                            value: FrontstageConnectionTransport.tcp,
                            label: Text('TCP'),
                          ),
                          ButtonSegment(
                            value: FrontstageConnectionTransport.bluetooth,
                            label: Text('蓝牙'),
                          ),
                        ],
                        selected: {selected},
                        onSelectionChanged: (selection) {
                          widget.onTransportChanged(selection.single);
                        },
                        style: _segmentedStyle(),
                      ),
                    ),
                    const SizedBox(width: 8),
                    MacGlassCloseButton(
                      key: const ValueKey('mg-close-connection'),
                      tooltip: '关闭连接面板',
                      onPressed: widget.onClose,
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                Expanded(
                  child: SingleChildScrollView(
                    child: AnimatedSwitcher(
                      duration: const Duration(milliseconds: 180),
                      child: _TransportBody(
                        key: ValueKey(selected),
                        selected: selected,
                        model: widget.model,
                        udpAddressController: _udpAddressController,
                        udpPortController: _udpPortController,
                        tcpAddressController: _tcpAddressController,
                        tcpPortController: _tcpPortController,
                        onSerialPortChanged: widget.onSerialPortChanged,
                        onBaudRateChanged: widget.onBaudRateChanged,
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 10),
                SizedBox(
                  key: const ValueKey('mg-connection-action'),
                  height: 44,
                  child: FilledButton(
                    onPressed:
                        selected != FrontstageConnectionTransport.serial ||
                            !widget.model.serialPorts.any(
                              (port) => port.path == widget.model.portLabel,
                            )
                        ? null
                        : widget.model.connected || widget.model.busy
                        ? widget.onDisconnectPressed
                        : widget.onConnectPressed,
                    style: FilledButton.styleFrom(
                      padding: const EdgeInsets.symmetric(horizontal: 14),
                      textStyle: const TextStyle(
                        fontSize: 13,
                        fontWeight: FontWeight.w700,
                      ),
                      backgroundColor: MacGlassTokens.black,
                      foregroundColor: Colors.white,
                      disabledBackgroundColor: Colors.black.withValues(
                        alpha: 0.16,
                      ),
                      disabledForegroundColor: MacGlassTokens.muted,
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(15),
                      ),
                    ),
                    child: Text(
                      widget.model.connected
                          ? '断开'
                          : widget.model.busy
                          ? '取消连接'
                          : '连接',
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  ButtonStyle _segmentedStyle() {
    return ButtonStyle(
      visualDensity: VisualDensity.compact,
      tapTargetSize: MaterialTapTargetSize.shrinkWrap,
      minimumSize: WidgetStateProperty.all(const Size(0, 36)),
      padding: WidgetStateProperty.all(
        const EdgeInsets.symmetric(horizontal: 6),
      ),
      textStyle: WidgetStateProperty.all(
        const TextStyle(fontSize: 11.5, fontWeight: FontWeight.w800),
      ),
      backgroundColor: WidgetStateProperty.resolveWith((states) {
        if (states.contains(WidgetState.selected)) {
          return MacGlassTokens.black;
        }
        return Colors.white.withValues(alpha: 0.36);
      }),
      foregroundColor: WidgetStateProperty.resolveWith((states) {
        if (states.contains(WidgetState.selected)) {
          return Colors.white;
        }
        return MacGlassTokens.ink;
      }),
      side: WidgetStateProperty.all(
        BorderSide(color: Colors.white.withValues(alpha: 0.74)),
      ),
    );
  }
}

final class _TransportBody extends StatelessWidget {
  const _TransportBody({
    required this.selected,
    required this.model,
    required this.udpAddressController,
    required this.udpPortController,
    required this.tcpAddressController,
    required this.tcpPortController,
    this.onSerialPortChanged,
    this.onBaudRateChanged,
    super.key,
  });

  final FrontstageConnectionTransport selected;
  final FrontstageConnectionModel model;
  final TextEditingController udpAddressController;
  final TextEditingController udpPortController;
  final TextEditingController tcpAddressController;
  final TextEditingController tcpPortController;
  final ValueChanged<String>? onSerialPortChanged;
  final ValueChanged<int>? onBaudRateChanged;

  @override
  Widget build(BuildContext context) {
    return switch (selected) {
      FrontstageConnectionTransport.serial => _SerialPage(
        model: model,
        onPortChanged: onSerialPortChanged,
        onBaudRateChanged: onBaudRateChanged,
      ),
      FrontstageConnectionTransport.udp => _NetworkPage(
        title: 'UDP 监听',
        modeLabel: '监听模式',
        addressController: udpAddressController,
        portController: udpPortController,
      ),
      FrontstageConnectionTransport.tcp => _NetworkPage(
        title: 'TCP 连接',
        modeLabel: '远端地址',
        addressController: tcpAddressController,
        portController: tcpPortController,
      ),
      FrontstageConnectionTransport.bluetooth => const _BluetoothPage(),
    };
  }
}

final class _SerialPage extends StatelessWidget {
  const _SerialPage({
    required this.model,
    this.onPortChanged,
    this.onBaudRateChanged,
  });

  final FrontstageConnectionModel model;
  final ValueChanged<String>? onPortChanged;
  final ValueChanged<int>? onBaudRateChanged;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (model.errorMessage != null) ...[
          _ConnectionInterruptedNotice(message: model.errorMessage!),
          const SizedBox(height: 12),
        ],
        const Text(
          '串口连接',
          style: TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
        ),
        const SizedBox(height: 14),
        SizedBox(
          key: const ValueKey('mg-connection-port-row'),
          height: 48,
          child: _MacGlassSelectField<String>(
            triggerKey: const ValueKey('mg-connection-port-selector'),
            label: '端口',
            value: model.serialPorts.any((port) => port.path == model.portLabel)
                ? model.portLabel
                : null,
            hint: '请选择串口',
            items: model.serialPorts
                .map(
                  (port) => DropdownMenuItem(
                    value: port.path,
                    child: port.name == port.path
                        ? Text(port.path)
                        : Text('${port.name} (${port.path})'),
                  ),
                )
                .toList(growable: false),
            onChanged: onPortChanged,
          ),
        ),
        const SizedBox(height: 8),
        SizedBox(
          key: const ValueKey('mg-connection-baud-row'),
          height: 48,
          child: _MacGlassSelectField<int>(
            triggerKey: const ValueKey('mg-connection-baud-selector'),
            label: '波特率',
            value: int.tryParse(model.baudRateLabel) ?? 115200,
            items: const [9600, 57600, 115200, 230400, 460800, 921600]
                .map(
                  (baudRate) => DropdownMenuItem(
                    value: baudRate,
                    child: Text('$baudRate'),
                  ),
                )
                .toList(growable: false),
            onChanged: onBaudRateChanged,
          ),
        ),
      ],
    );
  }
}

final class _ConnectionInterruptedNotice extends StatelessWidget {
  const _ConnectionInterruptedNotice({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      key: const ValueKey('mg-connection-disconnected-notice'),
      decoration: BoxDecoration(
        color: MacGlassTokens.red.withValues(alpha: 0.09),
        border: Border.all(color: MacGlassTokens.red.withValues(alpha: 0.22)),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.all(10),
        child: Row(
          children: [
            const Icon(
              Icons.link_off_rounded,
              size: 16,
              color: MacGlassTokens.red,
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                message,
                style: const TextStyle(
                  color: MacGlassTokens.red,
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

final class _MacGlassSelectField<T> extends StatelessWidget {
  const _MacGlassSelectField({
    required this.triggerKey,
    required this.label,
    required this.value,
    required this.items,
    required this.onChanged,
    this.hint,
  });

  final Key triggerKey;
  final String label;
  final T? value;
  final List<DropdownMenuItem<T>> items;
  final ValueChanged<T>? onChanged;
  final String? hint;

  @override
  Widget build(BuildContext context) {
    final selectedItem = items.where((item) => item.value == value).firstOrNull;
    final enabled = onChanged != null && items.isNotEmpty;
    return MenuAnchor(
      style: MacGlassMenu.style(minimumWidth: 302),
      alignmentOffset: const Offset(0, 6),
      menuChildren: [
        for (final item in items)
          MenuItemButton(
            onPressed: enabled && item.value != null
                ? () => onChanged!(item.value as T)
                : null,
            style: MacGlassMenu.itemStyle(selected: item.value == value),
            child: Row(
              children: [
                Icon(
                  item.value == value
                      ? Icons.check_rounded
                      : Icons.radio_button_unchecked_rounded,
                  size: 16,
                  color: item.value == value
                      ? MacGlassTokens.green
                      : MacGlassTokens.muted.withValues(alpha: 0.54),
                ),
                const SizedBox(width: 9),
                Expanded(
                  child: DefaultTextStyle.merge(
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(fontSize: 12.5),
                    child: item.child,
                  ),
                ),
              ],
            ),
          ),
      ],
      builder: (context, controller, child) => DecoratedBox(
        decoration: BoxDecoration(
          color: Colors.white.withValues(alpha: enabled ? 0.58 : 0.30),
          border: Border.all(color: Colors.white.withValues(alpha: 0.80)),
          borderRadius: BorderRadius.circular(15),
        ),
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            key: triggerKey,
            borderRadius: BorderRadius.circular(14),
            onTap: !enabled
                ? null
                : () {
                    if (controller.isOpen) {
                      controller.close();
                    } else {
                      controller.open();
                    }
                  },
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              child: Row(
                children: [
                  Expanded(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          label,
                          style: TextStyle(
                            fontSize: 10.5,
                            color: MacGlassTokens.muted.withValues(alpha: 0.92),
                          ),
                        ),
                        const SizedBox(height: 1),
                        DefaultTextStyle.merge(
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(
                            fontSize: 13,
                            color: enabled
                                ? MacGlassTokens.ink
                                : MacGlassTokens.muted,
                          ),
                          child: selectedItem?.child ?? Text(hint ?? ''),
                        ),
                      ],
                    ),
                  ),
                  Icon(
                    Icons.keyboard_arrow_down_rounded,
                    size: 18,
                    color: enabled ? MacGlassTokens.ink : MacGlassTokens.muted,
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

final class _NetworkPage extends StatelessWidget {
  const _NetworkPage({
    required this.title,
    required this.modeLabel,
    required this.addressController,
    required this.portController,
  });

  final String title;
  final String modeLabel;
  final TextEditingController addressController;
  final TextEditingController portController;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          title,
          style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
        ),
        const SizedBox(height: 14),
        _Field(label: modeLabel, controller: addressController),
        const SizedBox(height: 8),
        _Field(label: '端口', controller: portController),
      ],
    );
  }
}

final class _BluetoothPage extends StatelessWidget {
  const _BluetoothPage();

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        const Text(
          '蓝牙设备扫描',
          style: TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
        ),
        const SizedBox(height: 14),
        const _DeviceSkeleton(),
        const SizedBox(height: 8),
        const _DeviceSkeleton(),
        const SizedBox(height: 12),
        SizedBox(
          height: 40,
          child: OutlinedButton.icon(
            onPressed: null,
            icon: const Icon(Icons.bluetooth_searching_rounded, size: 18),
            label: const Text('扫描设备'),
          ),
        ),
      ],
    );
  }
}

final class _Field extends StatelessWidget {
  const _Field({required this.label, required this.controller});

  final String label;
  final TextEditingController controller;

  @override
  Widget build(BuildContext context) {
    return TextField(
      controller: controller,
      style: const TextStyle(fontSize: 13),
      decoration: InputDecoration(
        labelText: label,
        labelStyle: const TextStyle(fontSize: 12),
        isDense: true,
        constraints: const BoxConstraints.tightFor(height: 44),
        contentPadding: const EdgeInsets.symmetric(
          horizontal: 12,
          vertical: 10,
        ),
        filled: true,
        fillColor: Colors.white.withValues(alpha: 0.54),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(15),
          borderSide: BorderSide(color: Colors.white.withValues(alpha: 0.76)),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(15),
          borderSide: BorderSide(color: Colors.white.withValues(alpha: 0.76)),
        ),
      ),
    );
  }
}

final class _DeviceSkeleton extends StatelessWidget {
  const _DeviceSkeleton();

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.36),
        border: Border.all(color: Colors.white.withValues(alpha: 0.66)),
        borderRadius: BorderRadius.circular(15),
      ),
      child: const SizedBox(height: 44),
    );
  }
}
