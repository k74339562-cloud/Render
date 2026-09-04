cmake_minimum_required(VERSION 3.22.1)
project(render_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# تضمين ترويسات أندرويد الرسمية
include_directories(
    ${ANDROID_NDK}/sources/android/native_app_glue
    src
)

file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)

# دمج glue مباشرة لمنع حذف دالة ANativeActivity_onCreate
add_library(render_engine SHARED
    ${SOURCES}
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# فرض تصدير دالة الإقلاع لنظام أندرويد 13
target_link_options(render_engine PRIVATE "-Wl,-u,ANativeActivity_onCreate")

target_link_libraries(render_engine
    android
    log
    EGL
    GLESv3
)
