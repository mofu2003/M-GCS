package com.mgcs.mgcs_frontend;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Process;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

/** Android USB serial manager following QGroundControl's USB serial architecture. */
public final class MGCUsbSerialManager {
    private static final String TAG = "MGCUsbSerialManager";
    private static final String ACTION_USB_PERMISSION = "com.mgcs.mgcs_frontend.USB_PERMISSION";
    private static final String PORT_SUFFIX = "#p";
    private static final int READ_BUFFER_SIZE = 2048;

    private static final Object LOCK = new Object();
    private static Context appContext;
    private static UsbManager usbManager;
    private static PendingIntent permissionIntent;
    private static UsbSerialProber prober;
    private static boolean receiverRegistered;
    private static final List<UsbSerialDriver> drivers = new CopyOnWriteArrayList<>();
    private static final ConcurrentHashMap<Integer, DeviceResources> resources = new ConcurrentHashMap<>();

    private static native void nativeDeviceHasDisconnected(long token);
    private static native void nativeDeviceException(long token, String message);
    private static native void nativeDeviceNewData(long token, byte[] data);

    private static final class DeviceResources {
        UsbSerialDriver driver;
        UsbSerialPort port;
        UsbDeviceConnection connection;
        SerialInputOutputManager ioManager;
        long token;
        int portIndex;
    }

    private static final class DevicePort {
        final String baseName;
        final int portIndex;

        DevicePort(String baseName, int portIndex) {
            this.baseName = baseName;
            this.portIndex = Math.max(0, portIndex);
        }
    }

    private MGCUsbSerialManager() {}

    public static void initialize(Context context) {
        synchronized (LOCK) {
            if (usbManager != null || context == null) return;
            appContext = context.getApplicationContext();
            usbManager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
            if (usbManager == null) return;
            prober = new UsbSerialProber(UsbSerialProber.getDefaultProbeTable());
            setupPermissionIntent();
            registerReceiver();
            refreshDrivers();
        }
    }

    public static void cleanup(Context context) {
        synchronized (LOCK) {
            for (Integer id : resources.keySet()) close(id);
            drivers.clear();
            resources.clear();
            final Context unregisterContext = appContext != null ? appContext : context;
            if (receiverRegistered && unregisterContext != null) {
                try {
                    unregisterContext.unregisterReceiver(usbReceiver);
                } catch (IllegalArgumentException ignored) {
                }
                receiverRegistered = false;
            }
            permissionIntent = null;
            prober = null;
            usbManager = null;
            appContext = null;
        }
    }

    public static String[] availableDevicesInfo() {
        refreshDrivers();
        if (drivers.isEmpty()) return null;
        final java.util.ArrayList<String> result = new java.util.ArrayList<>();
        for (UsbSerialDriver driver : drivers) {
            final UsbDevice device = driver.getDevice();
            final List<UsbSerialPort> ports = driver.getPorts();
            for (UsbSerialPort port : ports) {
                final String name = exposedName(device, port.getPortNumber(), ports.size());
                result.add(
                    name + "\t" + safeString(device.getProductName()) + "\t" +
                    safeString(device.getManufacturerName()) + "\t" +
                    safeString(device.getSerialNumber()) + "\t" +
                    device.getProductId() + "\t" + device.getVendorId());
            }
        }
        return result.isEmpty() ? null : result.toArray(new String[0]);
    }

    public static int getDeviceId(String portName) {
        refreshDrivers();
        final DevicePort requested = parsePortName(portName);
        for (UsbSerialDriver driver : drivers) {
            final UsbDevice device = driver.getDevice();
            for (UsbSerialPort port : driver.getPorts()) {
                final DevicePort candidate =
                    new DevicePort(device.getDeviceName(), port.getPortNumber());
                if (candidate.baseName.equals(requested.baseName) &&
                    candidate.portIndex == requested.portIndex) {
                    return device.getDeviceId();
                }
            }
        }
        return -1;
    }

    public static void requestPermission(String portName) {
        if (usbManager == null) return;
        final UsbDevice device = findDeviceByPortName(portName);
        if (device != null && !usbManager.hasPermission(device) && permissionIntent != null) {
            usbManager.requestPermission(device, permissionIntent);
        }
    }

    public static int open(String portName, int baudRate, long token) {
        if (usbManager == null) return -1;
        final UsbDevice device = findDeviceByPortName(portName);
        if (device == null || !usbManager.hasPermission(device)) return -1;
        final UsbSerialDriver driver = findDriverByDevice(device);
        if (driver == null) return -1;
        final DevicePort requested = parsePortName(portName);
        final List<UsbSerialPort> ports = driver.getPorts();
        if (requested.portIndex < 0 || requested.portIndex >= ports.size()) return -1;
        final UsbSerialPort port = ports.get(requested.portIndex);

        close(device.getDeviceId());
        final UsbDeviceConnection connection = usbManager.openDevice(device);
        if (connection == null) return -1;
        try {
            port.open(connection);
            port.setParameters(baudRate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
        } catch (IOException exception) {
            safeClose(port, connection);
            return -1;
        }

        final DeviceResources resource = new DeviceResources();
        resource.driver = driver;
        resource.port = port;
        resource.connection = connection;
        resource.token = token;
        resource.portIndex = requested.portIndex;
        resources.put(device.getDeviceId(), resource);
        return device.getDeviceId();
    }

    public static boolean startIoManager(int deviceId) {
        final DeviceResources resource = resources.get(deviceId);
        if (resource == null || resource.ioManager != null) return resource != null;
        resource.ioManager = new SerialInputOutputManager(
            resource.port,
            new SerialInputOutputManager.Listener() {
                @Override
                public void onNewData(byte[] data) {
                    if (data != null && data.length > 0) {
                        nativeDeviceNewData(resource.token, data);
                    }
                }

                @Override
                public void onRunError(Exception exception) {
                    nativeDeviceException(resource.token, exception.getMessage());
                    close(deviceId);
                    nativeDeviceHasDisconnected(resource.token);
                }
            });
        resource.ioManager.setReadBufferSize(READ_BUFFER_SIZE);
        resource.ioManager.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
        resource.ioManager.start();
        return true;
    }

    public static boolean stopIoManager(int deviceId) {
        final DeviceResources resource = resources.get(deviceId);
        if (resource == null) return false;
        final SerialInputOutputManager manager = resource.ioManager;
        resource.ioManager = null;
        if (manager != null) {
            manager.stop();
            manager.setListener(null);
        }
        return true;
    }

    public static int write(int deviceId, byte[] data, int length, int timeoutMs) {
        final DeviceResources resource = resources.get(deviceId);
        if (resource == null || resource.port == null || !resource.port.isOpen()) return -1;
        try {
            resource.port.write(data, Math.min(length, data.length), timeoutMs);
            return Math.min(length, data.length);
        } catch (IOException exception) {
            nativeDeviceException(resource.token, exception.getMessage());
            return -1;
        }
    }

    public static boolean close(int deviceId) {
        final DeviceResources resource = resources.remove(deviceId);
        if (resource == null) return false;
        stopIoManager(resource);
        safeClose(resource.port, resource.connection);
        return true;
    }

    private static void stopIoManager(DeviceResources resource) {
        if (resource == null || resource.ioManager == null) return;
        resource.ioManager.stop();
        resource.ioManager.setListener(null);
        resource.ioManager = null;
    }

    private static void safeClose(UsbSerialPort port, UsbDeviceConnection connection) {
        if (port != null) {
            try {
                port.close();
            } catch (IOException ignored) {
            }
        }
        if (connection != null) connection.close();
    }

    private static void refreshDrivers() {
        if (usbManager == null || prober == null) return;
        try {
            final List<UsbSerialDriver> found = prober.findAllDrivers(usbManager);
            drivers.clear();
            drivers.addAll(found);
        } catch (SecurityException ignored) {
        }
    }

    private static UsbSerialDriver findDriverByDevice(UsbDevice device) {
        for (UsbSerialDriver driver : drivers) {
            if (driver.getDevice().getDeviceId() == device.getDeviceId()) return driver;
        }
        return null;
    }

    private static UsbDevice findDeviceByPortName(String portName) {
        final DevicePort requested = parsePortName(portName);
        for (UsbSerialDriver driver : drivers) {
            if (driver.getDevice().getDeviceName().equals(requested.baseName)) {
                return driver.getDevice();
            }
        }
        return null;
    }

    private static DevicePort parsePortName(String portName) {
        if (portName == null) return new DevicePort("", 0);
        final int split = portName.lastIndexOf(PORT_SUFFIX);
        if (split <= 0) return new DevicePort(portName, 0);
        try {
            return new DevicePort(
                portName.substring(0, split),
                Integer.parseInt(portName.substring(split + PORT_SUFFIX.length())));
        } catch (NumberFormatException ignored) {
            return new DevicePort(portName, 0);
        }
    }

    private static String exposedName(UsbDevice device, int portIndex, int portCount) {
        return portCount > 1 ? device.getDeviceName() + PORT_SUFFIX + portIndex : device.getDeviceName();
    }

    private static String safeString(String value) {
        return value == null ? "" : value;
    }

    private static void setupPermissionIntent() {
        final Intent intent = new Intent(ACTION_USB_PERMISSION).setPackage(appContext.getPackageName());
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) flags |= PendingIntent.FLAG_IMMUTABLE;
        permissionIntent = PendingIntent.getBroadcast(appContext, 0, intent, flags);
    }

    private static void registerReceiver() {
        final IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        filter.addAction(ACTION_USB_PERMISSION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            appContext.registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            appContext.registerReceiver(usbReceiver, filter);
        }
        receiverRegistered = true;
    }

    private static final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                final UsbDevice device = getUsbDevice(intent);
                if (device != null) {
                    final DeviceResources resource = resources.get(device.getDeviceId());
                    if (resource != null) {
                        final long token = resource.token;
                        close(device.getDeviceId());
                        nativeDeviceHasDisconnected(token);
                    }
                }
            }
            refreshDrivers();
        }
    };

    @SuppressWarnings("deprecation")
    private static UsbDevice getUsbDevice(Intent intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class);
        }
        return intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
    }
}
