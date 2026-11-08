/**
 * android_jni.h - JavaVM 句柄的进程级登记点
 *
 * Android 上没有可移植的方式静态获取 JavaVM：`JNI_GetCreatedJavaVMs` 只在
 * libnativehelper（平台库，NDK 应用不是随便都能链接）里，应用侧可靠的做法是
 * 在 `JNI_OnLoad` 里把 VM 记下来。`libmgcore.so` 由 Flutter 通过
 * `System.loadLibrary("mgcore")` 加载，`JNI_OnLoad` 一定会先于任何原生调用。
 *
 * 非 Android 平台下这些声明不参与编译。
 */
#pragma once

#ifdef __ANDROID__

#include <jni.h>

namespace mg::core::platform {

/// 由 `JNI_OnLoad` 调用，登记当前进程的 JavaVM。
void set_android_java_vm(JavaVM* vm);

/// 取回登记的 JavaVM；尚未加载/不可用时返回 nullptr。
JavaVM* android_java_vm();

}  // namespace mg::core::platform

#endif  // __ANDROID__
