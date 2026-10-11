plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

val repoRoot = rootProject.projectDir.parentFile?.parentFile
    ?: error("Unable to locate repository root from Android project")
val mgcoreAssetsDir = layout.buildDirectory.dir("generated/mgcoreAssets").get().asFile

android {
    namespace = "com.mgcs.mgcs_frontend"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDir(mgcoreAssetsDir)
        }
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.mgcs.mgcs_frontend"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DMGCORE_ROOT_DIR=${repoRoot.absolutePath}",
                    "-DMGCORE_BUILD_SHARED=ON",
                    "-DMGCORE_BUILD_TESTS=OFF",
                    "-DANDROID_STL=c++_shared",
                )
            }
        }
    }

    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

dependencies {
    implementation("com.github.mik3y:usb-serial-for-android:3.10.0")
}

val syncMgcoreAssets by tasks.registering(Copy::class) {
    from(repoRoot.resolve("core/src/transport/USBBoardInfo.json"))
    from(repoRoot.resolve("core/src/sensor/SensorDeviceInfo.json"))
    from(repoRoot.resolve("core/src/config/ParameterDescriptors.json"))
    from(repoRoot.resolve("core/src/config/ParameterTranslations.zh-CN.json"))
    from(repoRoot.resolve("core/src/config/PX4ParameterMetaData.json"))
    into(File(mgcoreAssetsDir, "mgcore"))
}

tasks.configureEach {
    if (name == "preBuild") {
        dependsOn(syncMgcoreAssets)
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}
