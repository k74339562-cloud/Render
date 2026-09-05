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

find_program(GLSLC glslc HINTS ${ANDROID_NDK}/shader-tools/${ANDROID_NDK_HOST_SYSTEM_NAME})
if(GLSLC)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_vert.h
        COMMAND ${GLSLC} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_vert.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube.vert
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_frag.h
        COMMAND ${GLSLC} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_frag.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube.frag
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_vert.h
        COMMAND ${GLSLC} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_vert.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line.vert
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_frag.h
        COMMAND ${GLSLC} -mfmt=c ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_frag.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line.frag
    )
    set(SHADER_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_vert.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/cube_frag.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_vert.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/line_frag.h
    )
endif()

set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/box.cpp
    src/gizmo.cpp
    src/vulkan_pipeline.cpp
    src/vulkan_renderer.cpp
    ${SHADER_HEADERS}
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
