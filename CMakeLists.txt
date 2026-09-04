cmake_minimum_required(VERSION 3.22.1)

# تفعيل لغة C ولغة C++ معاً
project(render_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 1. تجميع مكتبة أندرويد الرسمية (Native App Glue)
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# 2. تجميع ملفات كود المحرك
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)

add_library(render_engine SHARED
    ${SOURCES}
)

# تضمين مجلدات الأكواد
target_include_directories(render_engine PRIVATE 
    ${ANDROID_NDK}/sources/android/native_app_glue
    src
)

# 3. ربط المكتبات الرسومية والنظام
target_link_libraries(render_engine
    android
    native_app_glue
    log
    EGL
    GLESv3
)
