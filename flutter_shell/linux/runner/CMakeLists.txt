cmake_minimum_required(VERSION 3.13)
project(runner LANGUAGES CXX)

# Define the application target. To change its name, change BINARY_NAME in the
# top-level CMakeLists.txt, not the value here, or `flutter run` will no longer
# work.
#
# Any new source files that you add to the application should be added here.
add_executable(${BINARY_NAME}
  "main.cc"
  "my_application.cc"
  "${FLUTTER_MANAGED_DIR}/generated_plugin_registrant.cc"
)

# Apply the standard set of build settings. This can be removed for applications
# that need different build settings.
apply_standard_settings(${BINARY_NAME})

# Add preprocessor definitions for the application ID.
add_definitions(-DAPPLICATION_ID="${APPLICATION_ID}")

# Add dependency libraries. Add any application-specific dependencies here.
target_link_libraries(${BINARY_NAME} PRIVATE flutter)
target_link_libraries(${BINARY_NAME} PRIVATE PkgConfig::GTK)

target_include_directories(${BINARY_NAME} PRIVATE "${CMAKE_SOURCE_DIR}")

# 后端动态库与随包资源必须和可执行文件放在一起。add_custom_command(TARGET)
# 只能在创建目标的目录（这里）调用，MGCORE_ROOT_DIR 由上层 CMakeLists 传入。
add_custom_command(TARGET ${BINARY_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MGCORE_ROOT_DIR}/build/libmgcore.so"
    "$<TARGET_FILE_DIR:${BINARY_NAME}>/libmgcore.so"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MGCORE_ROOT_DIR}/core/src/transport/USBBoardInfo.json"
    "$<TARGET_FILE_DIR:${BINARY_NAME}>/USBBoardInfo.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MGCORE_ROOT_DIR}/core/src/sensor/SensorDeviceInfo.json"
    "$<TARGET_FILE_DIR:${BINARY_NAME}>/SensorDeviceInfo.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MGCORE_ROOT_DIR}/core/src/config/ParameterDescriptors.json"
    "$<TARGET_FILE_DIR:${BINARY_NAME}>/ParameterDescriptors.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MGCORE_ROOT_DIR}/core/src/config/ParameterTranslations.zh-CN.json"
    "$<TARGET_FILE_DIR:${BINARY_NAME}>/ParameterTranslations.zh-CN.json"
  VERBATIM
)
