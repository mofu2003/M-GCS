#pragma once

#ifdef __ANDROID__

#include "core/transport/link_interface.h"
#include "core/transport/link_config.h"
#include "core/transport/link_factory.h"
#include "core/platform/platform_mutex.h"

#include <jni.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace mg::core::transport {

/// Android USB Host serial link following QGroundControl's JNI architecture.
///
/// Java owns UsbManager/UsbSerialPort and SerialInputOutputManager. This class
/// only bridges ILink send/receive to the Java manager and keeps a token-based
/// registry so JNI callbacks cannot dereference a destroyed link.
class AndroidUsbSerialLink final : public ILink {
public:
    explicit AndroidUsbSerialLink(const LinkDescriptor& descriptor);
    ~AndroidUsbSerialLink() override;

    bool open() override;
    void close() override;
    bool is_open() const override;
    bool send(const uint8_t* data, size_t size) override;
    void set_receive_callback(ReceiveCallback cb) override;

    static bool discover(std::vector<DiscoveredDevice>& devices);
    static void on_java_new_data(uint64_t token, const uint8_t* data, size_t size);
    static void on_java_disconnected(uint64_t token);
    static void on_java_exception(uint64_t token, const std::string& message);

private:
    static uint64_t register_link(AndroidUsbSerialLink* link);
    static void unregister_link(uint64_t token);
    static AndroidUsbSerialLink* lookup_link(uint64_t token);

    LinkDescriptor descriptor_;
    std::atomic<bool> opened_{false};
    std::atomic<bool> closed_{false};
    uint64_t token_ = 0;
    int device_id_ = -1;
    ReceiveCallback receive_cb_;
    mg::core::platform::Mutex cb_mtx_;
};

bool initialize_android_serial_jni(JavaVM* vm, JNIEnv* env);

} // namespace mg::core::transport

#endif
