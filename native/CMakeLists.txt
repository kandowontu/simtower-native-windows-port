cmake_minimum_required(VERSION 3.20)
project(SimTowerNative LANGUAGES CXX)

include(FetchContent)
set(WEBVIEW_USE_COMPAT_MINGW ON CACHE BOOL "Use the webview MinGW compatibility layer")
set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  webview
  GIT_REPOSITORY https://github.com/webview/webview.git
  GIT_TAG 0.12.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(webview)

set(EMBEDDED_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/generated/web_assets.res.o")
set_source_files_properties("${EMBEDDED_RESOURCES}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
add_executable(SimTowerNative WIN32 src/main.cpp "${EMBEDDED_RESOURCES}")
target_compile_features(SimTowerNative PRIVATE cxx_std_17)
target_compile_definitions(SimTowerNative PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(SimTowerNative PRIVATE generated)
target_link_libraries(SimTowerNative PRIVATE webview::core ws2_32)

if(MINGW)
  target_compile_options(SimTowerNative PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(SimTowerNative PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(SimTowerNative PROPERTIES
  OUTPUT_NAME "SimTowerNative"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
