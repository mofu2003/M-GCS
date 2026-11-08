/**
 * android_jni.cc - JavaVM 句柄的进程级登记点
 *
 * 见 android_jni.h。`JNI_OnLoad`（定义在 android_usb_serial_link.cc）由
 * Flutter 的 `System.loadLibrary("mgcore")` 触发，早于任何原生调用，所以
 * 那里登记的 VM 一定能被后续代码取到。
 */
#include "core/platform/android_jni.h"

#ifdef __ANDROID__

namespace mg::core::platform {

namespace {
JavaVM* g_java_vm = nullptr;
}  // namespace

void set_android_java_vm(JavaVM* vm) { g_java_vm = vm; }

JavaVM* android_java_vm() { return g_java_vm; }

}  // namespace mg::core::platform

#endif  // __ANDROID__
