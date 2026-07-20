/**
 * platform_filesystem_android.cc - Android implementation of filesystem utilities
 * Android 文件系统实现 — 通过 AssetManager 加载数据文件
 */
#include "core/platform/platform_filesystem.h"

#ifdef __ANDROID__
#include <jni.h>

namespace mg::core::platform {

// Android: data files are loaded via AssetManager, not filesystem paths.
// This is a placeholder — actual implementation needs JNI context.
std::string get_module_directory() { return ""; }

std::string find_data_file(const char* filename) { return ""; }

} // namespace mg::core::platform
#endif