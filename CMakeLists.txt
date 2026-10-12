cmake_minimum_required(VERSION 3.20)
project(mgcore VERSION 0.2.2 LANGUAGES CXX DESCRIPTION "mgroundcontrol C++ Core Engine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

if(MINGW)
    # 静态链接 C++ 运行时，避免 MinGW DLL 兼容性问题
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

# Ensure M_PI and other math constants are defined
add_definitions(-D_USE_MATH_DEFINES)

option(MGCORE_BUILD_SHARED "Build as shared library (DLL)" ON)
option(MGCORE_BUILD_TESTS  "Build unit tests" OFF)
option(MGCORE_REGISTER_HARDWARE_TESTS
       "Register hardware/manual tests with CTest (requires explicit operator approval)" OFF)
option(MGCORE_ENABLE_ASAN  "Enable AddressSanitizer (debug builds)" OFF)
option(MGCORE_ENABLE_TSAN  "Enable ThreadSanitizer (debug builds)" OFF)

# Platform detection
if(WIN32)
    set(MGCORE_PLATFORM "win32")
elseif(ANDROID)
    set(MGCORE_PLATFORM "android")
elseif(UNIX AND NOT APPLE)
    set(MGCORE_PLATFORM "linux")
elseif(APPLE)
    set(MGCORE_PLATFORM "macos")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()
message(STATUS "mgroundcontrol platform: ${MGCORE_PLATFORM}")

if(MSVC)
    add_compile_options(/utf-8)
endif()
if(MGCORE_PLATFORM STREQUAL "win32")
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Sanitizers (non-MSVC only)
if(NOT MSVC)
    if(MGCORE_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer enabled")
    endif()
    if(MGCORE_ENABLE_TSAN)
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
        message(STATUS "ThreadSanitizer enabled")
    endif()
endif()

# FFmpeg (optional, for video decoding)
option(MGCORE_HAS_FFMPEG "Enable FFmpeg video decoding" OFF)
if(MGCORE_HAS_FFMPEG)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libswscale)
    endif()
    if(FFMPEG_FOUND)
        message(STATUS "FFmpeg found: ${FFMPEG_VERSION}")
    else()
        # Fallback: try manual paths
        find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h PATHS
            ${FFMPEG_ROOT}/include /usr/include /usr/local/include)
        find_library(AVFORMAT_LIB avformat)
        find_library(AVCODEC_LIB avcodec)
        find_library(SWSCALE_LIB swscale)
        if(FFMPEG_INCLUDE_DIR AND AVFORMAT_LIB AND AVCODEC_LIB AND SWSCALE_LIB)
            set(FFMPEG_FOUND TRUE)
            message(STATUS "FFmpeg found (manual): ${FFMPEG_INCLUDE_DIR}")
        else()
            message(WARNING "FFmpeg not found. Video decoding disabled.")
            set(MGCORE_HAS_FFMPEG OFF)
        endif()
    endif()
endif()

# Third-party paths
set(MGCORE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MAVLINK_INCLUDE_DIR
    ${MGCORE_ROOT_DIR}/third_party/mavlink/include/mavlink/v2.0
    CACHE PATH "Path to generated MAVLink v2 C headers")
set(CJSON_INCLUDE_DIR   ${MGCORE_ROOT_DIR}/third_party/cjson)

# All sources
set(MGCORE_SOURCES
    # Core
    core/src/mavlink/mavlink_adapter.cc
    core/src/mavlink/mavlink_adapter_command.cc
    core/src/mavlink/mavlink_adapter_dispatch.cc
    core/src/mavlink/mavlink_adapter_handlers.cc
    core/src/mavlink/mavlink_adapter_log.cc
    core/src/mavlink/mavlink_adapter_mission.cc
    core/src/mavlink/mavlink_adapter_params.cc
    core/src/mavlink/mavlink_senders.cc
    core/src/mavlink/mavlink_router.cc
    core/src/mavlink/command_queue.cc
    core/src/mavlink/param_cache.cc
    core/src/mavlink/mission_transfer.cc
    core/src/mavlink/mavlink_message_names.cc
    core/src/firmware/firmware_adapter.cc
    core/src/firmware/firmware_plugin_ardupilot.cc
    core/src/firmware/firmware_plugin_px4.cc
    core/src/firmware/param_remapping.cc
    core/src/sensor/sensor_id_decoder.cc
    core/src/calibration/calibration_validator.cc
    core/src/calibration/calibration_service.cc
    core/src/calibration/strategies/gyro_calibration.cc
    core/src/calibration/strategies/accel_calibration.cc
    core/src/calibration/strategies/compass_calibration.cc
    core/src/calibration/strategies/baro_calibration.cc
    core/src/calibration/strategies/radio_calibration.cc
    core/src/settings/flight_logging_service.cc
    core/src/transport/connection_manager.cc
    core/src/transport/link_session.cc
    core/src/transport/in_memory_link.cc
    # Platform-specific transport
    core/src/transport/serial_link.cc
    core/src/transport/serial_port_discovery.cc
    # TCP/UDP transport
    core/src/transport/tcp_link.cc
    core/src/transport/udp_link.cc
    # Mission management
    core/src/mission/mission_item_factory.cc
    core/src/mission/survey_calculator.cc
    core/src/mission/cad/mission_document.cc
    core/src/mission/cad/mission_validation.cc
    core/src/mission/cad/mission_compile.cc
    core/src/mission/cad/mission_estimate.cc
    core/src/mission/cad/mission_command_catalog.cc
    core/src/mission/cad/mission_interchange.cc
    core/src/mission/cad/mission_waypoints.cc
    core/src/mission/cad/mission_qgc_plan.cc
    # Flight mode management
    core/src/flight/flight_mode_manager.cc
    # Configuration management
    core/src/config/param_descriptors.cc
    core/src/config/param_file_io.cc
    core/src/config/parameter_change_set.cc
    # Settings
    core/src/settings/flight_log_writer.cc
    core/src/log/flight_log_replayer.cc
    core/src/ftp/mavlink_ftp.cc
    core/src/settings/mavlink_log_writer.cc
    # Telemetry
    core/src/telemetry/telemetry_ring_buffer.cc
    core/src/telemetry/waveform_buffer.cc
    core/src/telemetry/adsb_tracker.cc
    # Geofence
    core/src/geofence/geofence_manager.cc
    # Terrain
    core/src/terrain/terrain_store.cc
    # Video
    core/src/video/video_decoder.cc
    # Vehicle (multi-vehicle)
    core/src/vehicle/multi_vehicle_manager.cc
    core/src/vehicle/vehicle.cc
    core/src/vehicle/vehicle_link_manager.cc
    core/src/transport/link_config.cc
    core/src/transport/link_factory.cc
    # SITL
    core/src/sitl/sitl_launcher.cc
    # Bridge / FFI
    bridge/ffi/ffi_connection.cc
    bridge/ffi/ffi_diagnostics.cc
    bridge/ffi/ffi_operations.cc
    bridge/ffi/ffi_error.cc
    bridge/ffi/ffi_vehicle.cc
    bridge/ffi/ffi_params.cc
    bridge/ffi/ffi_commands.cc
    bridge/ffi/ffi_flight_modes.cc
    bridge/ffi/ffi_log_commands.cc
    bridge/ffi/ffi_preflight_commands.cc
    bridge/ffi/ffi_flight_commands.cc
    bridge/ffi/ffi_flight_mode_state.cc
    bridge/ffi/ffi_calibration.cc
    bridge/ffi/ffi_health.cc
    bridge/ffi/ffi_rc.cc
    bridge/ffi/ffi_utility.cc
    bridge/ffi/ffi_logging.cc
    bridge/ffi/ffi_telemetry_summary.cc
    bridge/ffi/ffi_telemetry_navigation.cc
    bridge/ffi/ffi_telemetry_sensors.cc
    bridge/ffi/ffi_sensor_decode.cc
    bridge/ffi/ffi_mission_commands.cc
    bridge/ffi/ffi_mission_document.cc
    bridge/ffi/ffi_mission_shared.cc
    bridge/ffi/ffi_mission_interchange.cc
    bridge/ffi/ffi_mission_catalog.cc
    bridge/ffi/ffi_payload_commands.cc
    bridge/ffi/ffi_sitl_commands.cc
    bridge/ffi/ffi_survey_commands.cc
    bridge/ffi/ffi_terrain_commands.cc
    bridge/ffi/ffi_vehicle_info.cc
    bridge/ffi/ffi_vehicle_session.cc
    bridge/ffi/ffi_mission_bridge.cc
    bridge/ffi/ffi_config.cc
    bridge/ffi/ffi_config_extra.cc
    bridge/ffi/ffi_telemetry.cc
    bridge/ffi/ffi_geofence.cc
    bridge/ffi/ffi_vehicle_manager.cc
    bridge/ffi/ffi_serial.cc
    bridge/ffi/ffi_actuator.cc
    bridge/ffi/ffi_rc_config.cc
    bridge/ffi/ffi_px4.cc
)

# cJSON (C source, compile as part of the library)
enable_language(C)
list(APPEND MGCORE_SOURCES third_party/cjson/cJSON.c)

# Platform-specific platform_filesystem
if(MGCORE_PLATFORM STREQUAL "win32")
    list(APPEND MGCORE_SOURCES core/src/platform/platform_filesystem_win32.cc)
elseif(MGCORE_PLATFORM STREQUAL "linux" OR MGCORE_PLATFORM STREQUAL "macos")
    list(APPEND MGCORE_SOURCES core/src/platform/platform_filesystem_posix.cc)
elseif(MGCORE_PLATFORM STREQUAL "android")
    list(APPEND MGCORE_SOURCES core/src/platform/platform_filesystem_android.cc)
    list(APPEND MGCORE_SOURCES core/src/transport/android_usb_serial_link.cc)
endif()

# ================================================================
# 目标拆分：core 与 bridge/ffi 分成两个目标。
#
# A1（core 不依赖上层）由目标边界强制：core 目标只带 core/include，不带 bridge/ffi，
# 所以 core 里一旦 include bridge 头就直接编译失败。以前两者同在一个目标里、且
# bridge/ffi 在 PUBLIC include 路径上，只能靠 test_bridge_architecture_guards 的
# 文本扫描兜底（KD-20）。
# ================================================================
set(MGCORE_FFI_SOURCES ${MGCORE_SOURCES})
list(FILTER MGCORE_FFI_SOURCES INCLUDE REGEX "^bridge/ffi/")
set(MGCORE_CORE_SOURCES ${MGCORE_SOURCES})
list(FILTER MGCORE_CORE_SOURCES EXCLUDE REGEX "^bridge/ffi/")

# core 目标的公共配置：include 路径、第三方可见性、FFmpeg 与平台库。
function(mg_configure_core_target target)
    target_include_directories(${target} PUBLIC
        ${MGCORE_ROOT_DIR}/core/include
        ${CJSON_INCLUDE_DIR}
    )
    target_include_directories(${target} SYSTEM PUBLIC
        ${MAVLINK_INCLUDE_DIR}
    )
    target_compile_definitions(${target} PRIVATE CJSON_HIDE_SYMBOLS)
    if(MGCORE_HAS_FFMPEG AND FFMPEG_FOUND)
        target_compile_definitions(${target} PUBLIC MGCORE_HAS_FFMPEG)
        if(FFMPEG_INCLUDE_DIRS)
            target_include_directories(${target} PRIVATE ${FFMPEG_INCLUDE_DIRS})
            target_link_libraries(${target} PUBLIC ${FFMPEG_LIBRARIES})
        elseif(FFMPEG_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${FFMPEG_INCLUDE_DIR})
            target_link_libraries(${target} PUBLIC
                ${AVFORMAT_LIB} ${AVCODEC_LIB} ${SWSCALE_LIB})
        endif()
    endif()
    if(MGCORE_PLATFORM STREQUAL "win32")
        target_link_libraries(${target} PUBLIC setupapi ws2_32)
    elseif(MGCORE_PLATFORM STREQUAL "linux")
        target_link_libraries(${target} PUBLIC pthread)
    elseif(MGCORE_PLATFORM STREQUAL "macos")
        find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
        find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
        target_link_libraries(${target} PUBLIC
            ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
    elseif(MGCORE_PLATFORM STREQUAL "android")
        find_library(LOG_LIB log)
        target_link_libraries(${target} PUBLIC ${LOG_LIB})
    endif()
endfunction()

# Build library
add_library(mgcore_core STATIC ${MGCORE_CORE_SOURCES})
mg_configure_core_target(mgcore_core)

if(MGCORE_BUILD_SHARED)
    add_library(mgcore SHARED ${MGCORE_FFI_SOURCES})
    # Windows consumers load the DLL as mgcore.dll. Keep the platform
    # default prefix on Unix so Linux/Android produce libmgcore.so and
    # macOS produces libmgcore.dylib, matching the FFI loaders.
    if(WIN32)
        set_target_properties(mgcore PROPERTIES PREFIX "")
    endif()
else()
    add_library(mgcore STATIC ${MGCORE_FFI_SOURCES})
endif()
target_link_libraries(mgcore PUBLIC mgcore_core)
# 只有 bridge/ffi 进 include 路径：FFI 源码与外部消费者都要能看到 ffi_api.h。
target_include_directories(mgcore PUBLIC ${MGCORE_ROOT_DIR}/bridge/ffi)
target_compile_definitions(mgcore PRIVATE CJSON_HIDE_SYMBOLS)

# FFmpeg 与平台库（setupapi/ws2_32/pthread/框架/log）都挂在 core 目标上，
# 由 mg_configure_core_target() 统一配置——它们都是 core 的实现依赖。
if(MINGW)
    target_link_options(mgcore PRIVATE -static-libgcc -static-libstdc++)
endif()

# Output directory
set_target_properties(mgcore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${MGCORE_ROOT_DIR}/build
    LIBRARY_OUTPUT_DIRECTORY ${MGCORE_ROOT_DIR}/build
    ARCHIVE_OUTPUT_DIRECTORY ${MGCORE_ROOT_DIR}/build
)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG_TYPE IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG_TYPE}" CONFIG_TYPE_UPPER)
        set_target_properties(mgcore PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${MGCORE_ROOT_DIR}/build
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${MGCORE_ROOT_DIR}/build
            ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${MGCORE_ROOT_DIR}/build
        )
    endforeach()
endif()

# Copy data files
set(MGCORE_DATA_FILES
    core/src/transport/USBBoardInfo.json
    core/src/sensor/SensorDeviceInfo.json
    core/src/config/ParameterDescriptors.json
    core/src/config/ParameterTranslations.zh-CN.json
    core/src/config/PX4ParameterMetaData.json
)
foreach(DATA_FILE ${MGCORE_DATA_FILES})
    get_filename_component(DATA_NAME ${DATA_FILE} NAME)
    configure_file(${DATA_FILE} ${CMAKE_BINARY_DIR}/${DATA_NAME} COPYONLY)
endforeach()

# Multi-config generators place test executables in per-config directories.
# Keep runtime data beside those executables as well as at the build root.
if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG_TYPE IN LISTS CMAKE_CONFIGURATION_TYPES)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CONFIG_TYPE})
        foreach(DATA_FILE ${MGCORE_DATA_FILES})
            get_filename_component(DATA_NAME ${DATA_FILE} NAME)
            configure_file(
                ${DATA_FILE}
                ${CMAKE_BINARY_DIR}/${CONFIG_TYPE}/${DATA_NAME}
                COPYONLY
            )
        endforeach()
    endforeach()
endif()

# Tests (optional)
if(MGCORE_BUILD_TESTS)
    enable_testing()

    # Build a static library for tests (exports all symbols).
    # 测试用的 core 变体单独编译：MGCORE_TEST_BUILD 让校准历史写到临时目录而不是
    # 开发者的真实状态目录（calibration_service.cc 的 history_storage_path）。
    add_library(mgcore_core_test STATIC ${MGCORE_CORE_SOURCES})
    mg_configure_core_target(mgcore_core_test)
    target_compile_definitions(mgcore_core_test PRIVATE MGCORE_TEST_BUILD)

    add_library(mgcore_static STATIC ${MGCORE_FFI_SOURCES})
    target_link_libraries(mgcore_static PUBLIC mgcore_core_test)
    target_include_directories(mgcore_static PUBLIC ${MGCORE_ROOT_DIR}/bridge/ffi)
    target_compile_definitions(mgcore_static PRIVATE CJSON_HIDE_SYMBOLS)

    set(TEST_SOURCES
        # Offline / automated tests only
        core/tests/test_in_memory.cc
        core/tests/test_telemetry_cache.cc
        core/tests/test_multi_vehicle.cc
        core/tests/test_geofence.cc
        core/tests/test_param_file_io.cc
        core/tests/test_param_descriptors.cc
        core/tests/test_sensor_decode_offline.cc
        core/tests/test_firmware_adapter.cc
        core/tests/test_telemetry_ring_buffer.cc
        core/tests/test_waveform_buffer.cc
        core/tests/test_waveform_instances.cc
        core/tests/test_video_pipeline.cc
        core/tests/test_ffi_contract.cc
        core/tests/test_ffi_comprehensive.cc
        core/tests/test_bridge_architecture_guards.cc
        core/tests/test_bridge_vehicle_session.cc
        core/tests/test_bridge_mission_workflow.cc
    core/tests/test_mission_document.cc
        tests/test_ffi_response_contract.cc
        core/tests/test_mavlink_protocol.cc
        core/tests/test_mavlink_router.cc
        core/tests/test_mavlink_send_callback_reentrancy.cc
        core/tests/test_shared_link_transports.cc

        tests/test_px4_sensor_offline.cc
        tests/manual/test_real_fc_cal_mission.cc
        tests/manual/test_sitl_mission_scenario.cc
        tests/manual/test_real_fc_calibration.cc
        tests/manual/test_real_fc_modes.cc
        tests/manual/test_real_fc_px4_ops.cc
        tests/test_mp_qgc_features.cc

        # Simulation tests (MockFlightController)
        core/tests/test_sim_telemetry.cc
        core/tests/test_sim_params.cc
        core/tests/test_sim_mission.cc
        core/tests/test_sim_mission_document.cc
        core/tests/test_sim_commands.cc
        core/tests/test_sim_calibration.cc
        core/tests/test_sim_survey.cc
        core/tests/test_terrain.cc
        core/tests/test_state_machines.cc
        core/tests/test_transport_links.cc
        core/tests/test_new_modules.cc
        core/tests/test_sim_geofence.cc
        core/tests/test_sim_fence_rally.cc
        core/tests/test_sim_full_flow.cc
        core/tests/test_sim_ui_paths.cc
        tests/test_real_fc_dump.cc
        tests/test_scan_connect.cc
    )
    foreach(TEST_SRC ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SRC})
        target_include_directories(${TEST_NAME} PRIVATE
            ${MGCORE_ROOT_DIR}/core/include
            ${MGCORE_ROOT_DIR}/bridge/ffi
            ${MGCORE_ROOT_DIR}/core/src/mavlink
        )
        target_include_directories(${TEST_NAME} SYSTEM PRIVATE
            ${MAVLINK_INCLUDE_DIR}
            ${CJSON_INCLUDE_DIR}
        )
        target_compile_definitions(${TEST_NAME} PRIVATE CJSON_HIDE_SYMBOLS)
        target_compile_definitions(${TEST_NAME} PRIVATE MGCORE_TEST_ROOT="${MGCORE_ROOT_DIR}")
        file(READ ${TEST_SRC} TEST_SRC_CONTENTS)
        if(TEST_SRC_CONTENTS MATCHES "ffi_api\\.h" AND
           NOT TEST_SRC_CONTENTS MATCHES "MGCORE_EXPORTS")
            target_compile_definitions(${TEST_NAME} PRIVATE MGCORE_EXPORTS)
        endif()
        target_link_libraries(${TEST_NAME} PRIVATE mgcore_static)
        if(TEST_SRC MATCHES "/manual/" OR TEST_NAME MATCHES "test_real_fc_cal_mission|test_real_fc_calibration|test_real_fc_modes")
            set(TEST_LABELS "manual;hardware")
        elseif(TEST_NAME MATCHES "test_real_fc|test_scan_connect")
            set(TEST_LABELS "hardware")
        elseif(TEST_NAME MATCHES "test_sim_|test_state_machines|test_transport_links|test_new_modules|test_shared_link_transports")
            set(TEST_LABELS "simulation")
        else()
            set(TEST_LABELS "offline")
        endif()

        # Hardware tests remain buildable but must never be picked up by a
        # plain `ctest` invocation. They can connect to a real vehicle and may
        # issue calibration, mission, or flight-mode commands.
        if(MGCORE_REGISTER_HARDWARE_TESTS OR NOT TEST_LABELS MATCHES "hardware")
            add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
            set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${TEST_LABELS}")
        endif()
    endforeach()
endif()

# Interactive reconnect test (only registered with explicit hardware opt-in).
if(MGCORE_BUILD_TESTS)
    add_executable(test_reconnect tests/test_reconnect.cc)
    target_include_directories(test_reconnect PRIVATE
        ${MGCORE_ROOT_DIR}/core/include
        ${MGCORE_ROOT_DIR}/bridge/ffi
    )
    target_include_directories(test_reconnect SYSTEM PRIVATE
        ${MAVLINK_INCLUDE_DIR}
        ${CJSON_INCLUDE_DIR}
    )
    target_compile_definitions(test_reconnect PRIVATE CJSON_HIDE_SYMBOLS)
    file(READ tests/test_reconnect.cc TEST_RECONNECT_CONTENTS)
    if(TEST_RECONNECT_CONTENTS MATCHES "ffi_api\\.h" AND
       NOT TEST_RECONNECT_CONTENTS MATCHES "MGCORE_EXPORTS")
        target_compile_definitions(test_reconnect PRIVATE MGCORE_EXPORTS)
    endif()
    target_link_libraries(test_reconnect PRIVATE mgcore_static)
    if(MGCORE_PLATFORM STREQUAL "win32")
        target_link_libraries(test_reconnect PRIVATE setupapi ws2_32)
    endif()
    if(MGCORE_REGISTER_HARDWARE_TESTS)
        add_test(NAME test_reconnect COMMAND test_reconnect)
        set_tests_properties(test_reconnect PROPERTIES LABELS "manual;hardware")
    endif()
endif()
