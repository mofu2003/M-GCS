cmake_minimum_required(VERSION 3.22.1)
project(mgcs_android_backend LANGUAGES C CXX)

if(NOT DEFINED MGCORE_ROOT_DIR)
  message(FATAL_ERROR "MGCORE_ROOT_DIR is required")
endif()

set(MGCORE_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(MGCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory("${MGCORE_ROOT_DIR}" mgcore_root)

# Root CMake normally writes desktop artifacts into the repository build/
# directory. Android packaging must see the target inside the Gradle CMake
# build tree instead.
set(_mgcore_output_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
if(NOT _mgcore_output_dir)
  set(_mgcore_output_dir "${CMAKE_CURRENT_BINARY_DIR}/mgcore")
endif()
set_target_properties(mgcore PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${_mgcore_output_dir}"
  RUNTIME_OUTPUT_DIRECTORY "${_mgcore_output_dir}"
)
