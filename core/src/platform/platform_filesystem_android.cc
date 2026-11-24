/**
 * platform_filesystem_android.cc - Android data-file loading.
 *
 * The core cannot read APK assets through std::ifstream. Resolve the current
 * Android application, open the bundled asset through AssetManager, extract it
 * once into the application cache directory, and return that real filesystem
 * path to the existing JSON loaders.
 */
#include "core/platform/platform_filesystem.h"
#include "core/platform/android_jni.h"

#ifdef __ANDROID__

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace mg::core::platform {

namespace {

class JniEnvScope {
public:
    JniEnvScope() {
        JavaVM* vm = java_vm();
        if (!vm) return;
        if (vm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6) == JNI_OK) {
            return;
        }
        if (vm->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
            attached_ = true;
        }
    }

    ~JniEnvScope() {
        if (attached_) {
            if (JavaVM* vm = java_vm()) vm->DetachCurrentThread();
        }
    }

    JNIEnv* get() const { return env_; }

private:
    static JavaVM* java_vm() {
        // 由 JNI_OnLoad 登记（core/src/platform/android_jni.cc）。
        // 不再调用 JNI_GetCreatedJavaVMs：那是平台库 libnativehelper 的符号，
        // NDK 应用不能依赖它，否则 libmgcore.so 在 arm64-v8a 上链接失败。
        return mg::core::platform::android_java_vm();
    }

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

jobject application_context(JNIEnv* env) {
    static std::mutex mutex;
    static jobject context = nullptr;
    std::lock_guard<std::mutex> lock(mutex);
    if (context || !env) return context;

    jclass activity_thread = env->FindClass("android/app/ActivityThread");
    if (!activity_thread) return nullptr;
    jmethodID current_application = env->GetStaticMethodID(
        activity_thread,
        "currentApplication",
        "()Landroid/app/Application;");
    if (current_application) {
        jobject local = env->CallStaticObjectMethod(activity_thread, current_application);
        if (local) context = env->NewGlobalRef(local);
        if (local) env->DeleteLocalRef(local);
    }
    env->DeleteLocalRef(activity_thread);
    return context;
}

jobject asset_manager_object(JNIEnv* env, jobject context) {
    static std::mutex mutex;
    static jobject manager = nullptr;
    std::lock_guard<std::mutex> lock(mutex);
    if (manager || !env || !context) return manager;

    jclass context_class = env->GetObjectClass(context);
    if (!context_class) return nullptr;
    jmethodID get_assets = env->GetMethodID(
        context_class,
        "getAssets",
        "()Landroid/content/res/AssetManager;");
    if (get_assets) {
        jobject local = env->CallObjectMethod(context, get_assets);
        if (local) manager = env->NewGlobalRef(local);
        if (local) env->DeleteLocalRef(local);
    }
    env->DeleteLocalRef(context_class);
    return manager;
}

std::string cache_directory(JNIEnv* env, jobject context) {
    static std::mutex mutex;
    static std::string cached;
    std::lock_guard<std::mutex> lock(mutex);
    if (!cached.empty() || !env || !context) return cached;

    jclass context_class = env->GetObjectClass(context);
    if (!context_class) return "";
    jmethodID get_cache_dir = env->GetMethodID(
        context_class,
        "getCacheDir",
        "()Ljava/io/File;");
    if (get_cache_dir) {
        jobject file = env->CallObjectMethod(context, get_cache_dir);
        if (file) {
            jclass file_class = env->GetObjectClass(file);
            jmethodID get_absolute_path = env->GetMethodID(
                file_class,
                "getAbsolutePath",
                "()Ljava/lang/String;");
            if (get_absolute_path) {
                auto path = static_cast<jstring>(
                    env->CallObjectMethod(file, get_absolute_path));
                cached = java_string(env, path);
                if (path) env->DeleteLocalRef(path);
            }
            env->DeleteLocalRef(file_class);
            env->DeleteLocalRef(file);
        }
    }
    env->DeleteLocalRef(context_class);
    return cached;
}

std::string extract_asset(const char* filename) {
    if (!filename || filename[0] == '\0') return "";

    JniEnvScope env_scope;
    JNIEnv* env = env_scope.get();
    if (!env) return "";
    jobject context = application_context(env);
    if (!context) return "";
    jobject manager_object = asset_manager_object(env, context);
    if (!manager_object) return "";
    AAssetManager* manager = AAssetManager_fromJava(env, manager_object);
    if (!manager) return "";

    const std::string asset_path = std::string("mgcore/") + filename;
    AAsset* asset = AAssetManager_open(manager, asset_path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) return "";

    const off64_t length = AAsset_getLength64(asset);
    const void* data = AAsset_getBuffer(asset);
    if (!data || length <= 0) {
        AAsset_close(asset);
        return "";
    }

    const std::string cache_dir = cache_directory(env, context);
    if (cache_dir.empty()) {
        AAsset_close(asset);
        return "";
    }
    const std::filesystem::path output_dir =
        std::filesystem::path(cache_dir) / "mgcore";
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        AAsset_close(asset);
        return "";
    }

    const std::filesystem::path output_path = output_dir / filename;
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        AAsset_close(asset);
        return "";
    }
    output.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    output.close();
    AAsset_close(asset);
    if (!output) return "";
    return output_path.string();
}

std::string module_directory() {
    JniEnvScope env_scope;
    JNIEnv* env = env_scope.get();
    if (!env) return "";
    jobject context = application_context(env);
    if (!context) return "";
    const std::string cache = cache_directory(env, context);
    if (cache.empty()) return "";
    return (std::filesystem::path(cache) / "mgcore").string() +
           std::string(1, std::filesystem::path::preferred_separator);
}

} // namespace

std::string get_module_directory() {
    static std::mutex mutex;
    static std::string cached;
    std::lock_guard<std::mutex> lock(mutex);
    if (cached.empty()) cached = module_directory();
    return cached;
}

std::string find_data_file(const char* filename) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::string> cached;
    if (!filename || filename[0] == '\0') return "";

    std::lock_guard<std::mutex> lock(mutex);
    const auto found = cached.find(filename);
    if (found != cached.end()) return found->second;

    std::string path = extract_asset(filename);
    if (path.empty()) path = get_module_directory() + filename;
    if (!std::filesystem::exists(path)) {
        cached.emplace(filename, std::string());
        return "";
    }
    cached.emplace(filename, path);
    return path;
}

} // namespace mg::core::platform

#endif
