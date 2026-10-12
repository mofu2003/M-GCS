#include "core/transport/android_usb_serial_link.h"

#ifdef __ANDROID__

#include "core/log/logging.h"

#include <jni.h>

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mg::core::transport {

namespace {

constexpr const char* kManagerClass = "com/mgcs/mgcs_frontend/MGCUsbSerialManager";

JavaVM* g_vm = nullptr;
jclass g_manager_class = nullptr;
jmethodID g_available_devices = nullptr;
jmethodID g_get_device_id = nullptr;
jmethodID g_open = nullptr;
jmethodID g_close = nullptr;
jmethodID g_start_io = nullptr;
jmethodID g_stop_io = nullptr;
jmethodID g_write = nullptr;
jmethodID g_request_permission = nullptr;

class JniEnvScope {
public:
    JniEnvScope() {
        if (!g_vm) return;
        if (g_vm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6) == JNI_OK) {
            return;
        }
        if (g_vm->AttachCurrentThread(&env_, nullptr) == JNI_OK) attached_ = true;
    }

    ~JniEnvScope() {
        if (attached_ && g_vm) g_vm->DetachCurrentThread();
    }

    JNIEnv* get() const { return env_; }

private:
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};

std::string java_string(JNIEnv* env, jstring value) {
    if (!env || !value) return "";
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return "";
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jstring to_jstring(JNIEnv* env, const std::string& value) {
    return env ? env->NewStringUTF(value.c_str()) : nullptr;
}

void clear_exception(JNIEnv* env, const char* context) {
    if (!env || !env->ExceptionCheck()) return;
    env->ExceptionDescribe();
    env->ExceptionClear();
    MGC_LOG_ERROR("[AndroidSerial] JNI exception in %s", context);
}

void JNICALL native_disconnected(JNIEnv*, jclass, jlong token) {
    AndroidUsbSerialLink::on_java_disconnected(static_cast<uint64_t>(token));
}

void JNICALL native_exception(JNIEnv* env, jclass, jlong token, jstring message) {
    AndroidUsbSerialLink::on_java_exception(
        static_cast<uint64_t>(token), java_string(env, message));
}

void JNICALL native_new_data(JNIEnv* env, jclass, jlong token, jbyteArray data) {
    if (!data) return;
    const jsize length = env->GetArrayLength(data);
    if (length <= 0) return;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    if (!env->ExceptionCheck()) {
        AndroidUsbSerialLink::on_java_new_data(
            static_cast<uint64_t>(token), bytes.data(), bytes.size());
    } else {
        env->ExceptionClear();
    }
}

void register_native_methods(JNIEnv* env) {
    if (!env || !g_manager_class) return;
    const JNINativeMethod methods[] = {
        {"nativeDeviceHasDisconnected", "(J)V", reinterpret_cast<void*>(native_disconnected)},
        {"nativeDeviceException", "(JLjava/lang/String;)V", reinterpret_cast<void*>(native_exception)},
        {"nativeDeviceNewData", "(J[B)V", reinterpret_cast<void*>(native_new_data)},
    };
    env->RegisterNatives(g_manager_class, methods, 3);
    clear_exception(env, "RegisterNatives");
}

bool resolve_manager(JNIEnv* env) {
    if (!env || g_manager_class) return g_manager_class != nullptr;
    jclass local = env->FindClass(kManagerClass);
    if (!local) {
        clear_exception(env, "FindClass(MGCUsbSerialManager)");
        return false;
    }
    g_manager_class = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!g_manager_class) return false;

    g_available_devices = env->GetStaticMethodID(g_manager_class, "availableDevicesInfo", "()[Ljava/lang/String;");
    g_get_device_id = env->GetStaticMethodID(g_manager_class, "getDeviceId", "(Ljava/lang/String;)I");
    g_open = env->GetStaticMethodID(g_manager_class, "open", "(Ljava/lang/String;IJ)I");
    g_close = env->GetStaticMethodID(g_manager_class, "close", "(I)Z");
    g_start_io = env->GetStaticMethodID(g_manager_class, "startIoManager", "(I)Z");
    g_stop_io = env->GetStaticMethodID(g_manager_class, "stopIoManager", "(I)Z");
    g_write = env->GetStaticMethodID(g_manager_class, "write", "(I[BII)I");
    g_request_permission = env->GetStaticMethodID(g_manager_class, "requestPermission", "(Ljava/lang/String;)V");
    if (!g_available_devices || !g_get_device_id || !g_open || !g_close ||
        !g_start_io || !g_stop_io || !g_write || !g_request_permission) {
        clear_exception(env, "GetStaticMethodID");
        return false;
    }
    register_native_methods(env);
    return true;
}

std::mutex g_registry_mutex;
std::unordered_map<uint64_t, AndroidUsbSerialLink*> g_registry;
std::atomic<uint64_t> g_next_token{1};

} // namespace

AndroidUsbSerialLink::AndroidUsbSerialLink(const LinkDescriptor& descriptor)
    : descriptor_(descriptor) {}

AndroidUsbSerialLink::~AndroidUsbSerialLink() {
    close();
}

uint64_t AndroidUsbSerialLink::register_link(AndroidUsbSerialLink* link) {
    const uint64_t token = g_next_token.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_registry[token] = link;
    return token;
}

void AndroidUsbSerialLink::unregister_link(uint64_t token) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_registry.erase(token);
}

AndroidUsbSerialLink* AndroidUsbSerialLink::lookup_link(uint64_t token) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    const auto found = g_registry.find(token);
    return found == g_registry.end() ? nullptr : found->second;
}

bool AndroidUsbSerialLink::open() {
    if (opened_.load()) return true;
    JniEnvScope env_scope;
    JNIEnv* env = env_scope.get();
    if (!resolve_manager(env)) return false;

    jstring port_name = to_jstring(env, descriptor_.endpoint);
    env->CallStaticVoidMethod(g_manager_class, g_request_permission, port_name);
    clear_exception(env, "requestPermission");
    if (port_name) env->DeleteLocalRef(port_name);

    token_ = register_link(this);
    port_name = to_jstring(env, descriptor_.endpoint);
    const jint device_id = env->CallStaticIntMethod(
        g_manager_class, g_open, port_name, static_cast<jint>(descriptor_.baud_rate), static_cast<jlong>(token_));
    clear_exception(env, "open");
    if (port_name) env->DeleteLocalRef(port_name);
    if (device_id < 0) {
        unregister_link(token_);
        token_ = 0;
        return false;
    }
    device_id_ = device_id;
    if (!env->CallStaticBooleanMethod(g_manager_class, g_start_io, device_id)) {
        clear_exception(env, "startIoManager");
        env->CallStaticBooleanMethod(g_manager_class, g_close, device_id);
        unregister_link(token_);
        token_ = 0;
        device_id_ = -1;
        return false;
    }
    closed_.store(false);
    opened_.store(true);
    return true;
}

void AndroidUsbSerialLink::close() {
    if (closed_.exchange(true)) return;
    if (device_id_ >= 0) {
        JniEnvScope env_scope;
        JNIEnv* env = env_scope.get();
        if (env && g_manager_class) {
            env->CallStaticBooleanMethod(g_manager_class, g_stop_io, device_id_);
            env->CallStaticBooleanMethod(g_manager_class, g_close, device_id_);
            clear_exception(env, "close");
        }
        device_id_ = -1;
    }
    if (token_ != 0) {
        unregister_link(token_);
        token_ = 0;
    }
    opened_.store(false);
}

bool AndroidUsbSerialLink::is_open() const {
    return opened_.load();
}

bool AndroidUsbSerialLink::send(const uint8_t* data, size_t size) {
    if (!opened_.load() || !data || size == 0 || device_id_ < 0) return false;
    JniEnvScope env_scope;
    JNIEnv* env = env_scope.get();
    if (!env || !g_manager_class) return false;
    jbyteArray bytes = env->NewByteArray(static_cast<jsize>(size));
    if (!bytes) return false;
    env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(size), reinterpret_cast<const jbyte*>(data));
    const jint written = env->CallStaticIntMethod(
        g_manager_class, g_write, device_id_, bytes, static_cast<jint>(size), 1000);
    env->DeleteLocalRef(bytes);
    clear_exception(env, "write");
    return written == static_cast<jint>(size);
}

void AndroidUsbSerialLink::set_receive_callback(ReceiveCallback cb) {
    mg::core::platform::LockGuard lock(cb_mtx_);
    receive_cb_ = std::move(cb);
}

void AndroidUsbSerialLink::on_java_new_data(uint64_t token, const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    // 注册表锁内只做查表与回调拷贝：回调属于外部代码（ConnectionManager 的接收
    // 路径），持锁调用违反 AGENTS.md 的"不在 core 持锁路径中调用外部回调"；而且
    // g_registry_mutex 同时保护注册表，回调里若触发 on_java_disconnected /
    // unregister_link 就会自死锁。
    ReceiveCallback callback;
    {
        std::lock_guard<std::mutex> registry_lock(g_registry_mutex);
        const auto found = g_registry.find(token);
        if (found == g_registry.end() || !found->second) return;
        auto* link = found->second;
        mg::core::platform::LockGuard lock(link->cb_mtx_);
        callback = link->receive_cb_;
    }  // g_registry_mutex released here: only lookup and copy happened under it
    if (callback) callback(data, size);
}

void AndroidUsbSerialLink::on_java_disconnected(uint64_t token) {
    auto* link = lookup_link(token);
    if (link) link->opened_.store(false);
}

void AndroidUsbSerialLink::on_java_exception(uint64_t token, const std::string& message) {
    (void)lookup_link(token);
    MGC_LOG_ERROR("[AndroidSerial] %s", message.c_str());
}

bool AndroidUsbSerialLink::discover(std::vector<DiscoveredDevice>& devices) {
    JniEnvScope env_scope;
    JNIEnv* env = env_scope.get();
    if (!resolve_manager(env)) return false;
    jobjectArray array = static_cast<jobjectArray>(
        env->CallStaticObjectMethod(g_manager_class, g_available_devices));
    clear_exception(env, "availableDevicesInfo");
    if (!array) return false;
    const jsize count = env->GetArrayLength(array);
    for (jsize index = 0; index < count; ++index) {
        auto text = static_cast<jstring>(env->GetObjectArrayElement(array, index));
        const std::string item = java_string(env, text);
        if (text) env->DeleteLocalRef(text);
        const size_t first = item.find('\t');
        const size_t second = first == std::string::npos ? std::string::npos : item.find('\t', first + 1);
        if (first == std::string::npos) continue;
        DiscoveredDevice device;
        device.endpoint = item.substr(0, first);
        device.description = second == std::string::npos ? "" : item.substr(first + 1, second - first - 1);
        device.is_likely_fc = true;
        devices.push_back(std::move(device));
    }
    env->DeleteLocalRef(array);
    return !devices.empty();
}

bool initialize_android_serial_jni(JavaVM* vm, JNIEnv* env) {
    g_vm = vm;
    return resolve_manager(env);
}

} // namespace mg::core::transport

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    return mg::core::transport::initialize_android_serial_jni(vm, env)
        ? JNI_VERSION_1_6
        : JNI_ERR;
}

#endif
