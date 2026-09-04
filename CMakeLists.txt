cmake_minimum_required(VERSION 3.22.1)
project(render_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# إضافة ملفات C++ المقسمة
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)

add_library(render_engine SHARED
    ${SOURCES}
)

# ربط مكتبات أندرويد الأصلية (الرسوميات EGL/GLES3، النوافذ، والسجلات)
target_link_libraries(render_engine
    android
    native_app_glue
    log
    EGL
    GLESv3
)

# تضمين مكتبة Native App Glue الرسمية من جوجل NDK
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
