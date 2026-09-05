cmake_minimum_required(VERSION 3.22.1)
project(render_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)

include_directories(
    ${ANDROID_NDK}/sources/android/native_app_glue
    src
)

set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/gizmo.cpp
    src/vulkan_pipeline.cpp
    src/vulkan_renderer.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

add_library(render_engine SHARED
    ${SOURCES}
)

target_link_options(render_engine PRIVATE "-Wl,-u,ANativeActivity_onCreate")

target_link_libraries(render_engine
    android
    log
    vulkan
)
