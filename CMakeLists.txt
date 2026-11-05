cmake_minimum_required(VERSION 3.21)
project(slint_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 未手动指定 CMAKE_PREFIX_PATH 时，使用随项目下载的 Slint C++ 包
file(GLOB SLINT_LOCAL_PKG "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slint/*")
list(APPEND CMAKE_PREFIX_PATH ${SLINT_LOCAL_PKG})

find_package(Slint REQUIRED)

add_executable(slint_demo
    src/main.cpp
)

# 把 .slint 文件编译成 C++ 代码，并生成头文件 ui/app.h
slint_target_sources(slint_demo ui/app.slint)

target_link_libraries(slint_demo PRIVATE Slint::Slint)
